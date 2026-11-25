#include "ComponentTypeRegistry.h"

#include "Core/Log.h"
#include "GameObject.h"
#include "World.h"

#include <cassert>
#include <format>

namespace
{
	/** parent부터 위로 따라가는 부모 체인에 target이 있으면 true를 반환한다. */
	bool IsInParentChain(const ComponentTypeInfo* parent, const ComponentTypeInfo* target)
	{
		const ComponentTypeInfo* current = parent;
		while (current != nullptr)
		{
			if (current == target) { return true; }
			current = current->parent;
		}
		return false;
	}
}

ComponentTypeRegistry* ComponentTypeRegistry::instance = nullptr;

ComponentTypeRegistry::ComponentTypeRegistry()
{
	assert(instance == nullptr && "ComponentTypeRegistry는 한 번에 하나만 존재해야 한다.");
	instance = this;
}

ComponentTypeRegistry::~ComponentTypeRegistry()
{
	if (instance == this) { instance = nullptr; }
}

ComponentTypeRegistry& ComponentTypeRegistry::Get()
{
	assert(instance != nullptr && "ComponentTypeRegistry가 없을 때 Get을 호출했다.");
	return *instance;
}

const ComponentTypeInfo* ComponentTypeRegistry::AddType(std::string_view name, std::type_index type, const std::type_info* parentType, std::function<std::unique_ptr<Component>()> create, std::function<bool(const Component&)> isInstance)
{
	if (name.empty())
	{
		Log::Write(LogLevel::Error, "ComponentType", "Type name is empty");
		return nullptr;
	}
	if (typesByName.find(name) != typesByName.end())
	{
		Log::Write(LogLevel::Error, "ComponentType", std::format("{}: name is already registered", name));
		return nullptr;
	}
	if (typesByType.find(type) != typesByType.end())
	{
		Log::Write(LogLevel::Error, "ComponentType", std::format("{}: type is already registered", name));
		return nullptr;
	}

	const ComponentTypeInfo* parent = nullptr;
	if (parentType != nullptr)
	{
		const auto foundParent = typesByType.find(std::type_index(*parentType));
		if (foundParent == typesByType.end())
		{
			Log::Write(LogLevel::Error, "ComponentType", std::format("{}: parent type is not registered", name));
			return nullptr;
		}
		parent = foundParent->second;
	}

	if (!IsHierarchyConsistent(name, parent, create, isInstance)) { return nullptr; }

	std::unique_ptr<ComponentTypeInfo> info = std::make_unique<ComponentTypeInfo>();
	info->name = std::string(name);
	info->parent = parent;
	info->create = std::move(create);
	info->isInstance = std::move(isInstance);

	ComponentTypeInfo* added = info.get();
	types.push_back(std::move(info));
	typesByName.emplace(added->name, added);
	typesByType.emplace(type, added);
	return added;
}

bool ComponentTypeRegistry::IsHierarchyConsistent(std::string_view name, const ComponentTypeInfo* parent, const std::function<std::unique_ptr<Component>()>& create, const std::function<bool(const Component&)>& isInstance) const
{
	// 새 타입이 이미 등록된 타입의 자손인데 그 타입이 부모 체인에 없으면 중간 부모를 건너뛴 것이다.
	if (create)
	{
		const std::unique_ptr<Component> probe = create();
		for (const std::unique_ptr<ComponentTypeInfo>& registered : types)
		{
			if (!registered->isInstance(*probe)) { continue; }
			if (IsInParentChain(parent, registered.get())) { continue; }

			Log::Write(LogLevel::Error, "ComponentType", std::format("{}: skipped registered base type {}", name, registered->name));
			return false;
		}
	}

	// 이미 등록된 타입이 새 타입의 자손이면 새 타입이 자식보다 늦게 등록된 것이다.
	for (const std::unique_ptr<ComponentTypeInfo>& registered : types)
	{
		if (!registered->create) { continue; }

		const std::unique_ptr<Component> probe = registered->create();
		if (isInstance(*probe))
		{
			Log::Write(LogLevel::Error, "ComponentType", std::format("{}: registered after derived type {}", name, registered->name));
			return false;
		}
	}

	return true;
}

bool ComponentTypeRegistry::AddFieldInfo(std::type_index type, ComponentFieldInfo field)
{
	if (field.name.empty())
	{
		Log::Write(LogLevel::Error, "ComponentType", "Field name is empty");
		return false;
	}

	const auto found = typesByType.find(type);
	if (found == typesByType.end())
	{
		Log::Write(LogLevel::Error, "ComponentType", std::format("{}: type is not registered", field.name));
		return false;
	}
	ComponentTypeInfo* owner = found->second;

	// owner의 조상(자신 포함)이나 자손에 같은 이름이 있으면 부모 속성과 합쳐 볼 때 이름이 겹친다.
	for (const std::unique_ptr<ComponentTypeInfo>& registered : types)
	{
		if (!IsInParentChain(owner, registered.get()) && !IsInParentChain(registered.get(), owner)) { continue; }

		for (const ComponentFieldInfo& existing : registered->fields)
		{
			if (existing.name != field.name) { continue; }

			Log::Write(LogLevel::Error, "ComponentType", std::format("{}.{}: field name is already used by {}", owner->name, field.name, registered->name));
			return false;
		}
	}

	owner->fields.push_back(std::move(field));
	return true;
}

ObjectID ComponentTypeRegistry::GetOwnerObjectID(const Component* component)
{
	if (component == nullptr) { return ObjectID{}; }

	const GameObject* owner = component->GetGameObject();
	return owner != nullptr ? owner->GetObjectID() : ObjectID{};
}

Component* ComponentTypeRegistry::FindReferencedComponent(Component& target, const ComponentReference& reference)
{
	GameObject* targetOwner = target.GetGameObject();
	if (targetOwner == nullptr || !reference.objectID.IsValid()) { return nullptr; }

	// 값을 쓸 컴포넌트와 같은 World에서 찾는다. Play World에 쓰면 Play World의 객체를 찾는다.
	GameObject* referencedOwner = targetOwner->GetWorld().FindGameObject(reference.objectID);
	if (referencedOwner == nullptr) { return nullptr; }

	// 타입 검사는 등록 정보의 isInstance(dynamic_cast)로 해 GetComponent<T>와 같은 기준을 쓴다.
	const ComponentTypeRegistry& registry = Get();
	const auto foundType = registry.typesByType.find(reference.componentType);
	if (foundType == registry.typesByType.end()) { return nullptr; }

	for (const std::unique_ptr<Component>& candidate : referencedOwner->GetComponents())
	{
		if (foundType->second->isInstance(*candidate)) { return candidate.get(); }
	}
	return nullptr;
}

const ComponentTypeInfo* ComponentTypeRegistry::FindByName(std::string_view name) const
{
	const auto found = typesByName.find(name);
	if (found == typesByName.end()) { return nullptr; }
	return found->second;
}

const ComponentTypeInfo* ComponentTypeRegistry::Find(const Component& component) const
{
	// Component는 가상 함수가 있어 typeid가 선언 타입이 아닌 실제 타입을 돌려준다.
	const auto found = typesByType.find(std::type_index(typeid(component)));
	if (found == typesByType.end()) { return nullptr; }
	return found->second;
}

const ComponentTypeInfo* ComponentTypeRegistry::FindByType(std::type_index type) const
{
	const auto found = typesByType.find(type);
	if (found == typesByType.end()) { return nullptr; }
	return found->second;
}

void ComponentTypeRegistry::ForEachField(const ComponentTypeInfo& type, const std::function<void(const ComponentFieldInfo&)>& visit)
{
	// 부모를 먼저 방문해 color, intensity 같은 공통 속성이 자식 속성보다 앞에 오게 한다.
	if (type.parent != nullptr) { ForEachField(*type.parent, visit); }

	for (const ComponentFieldInfo& field : type.fields) { visit(field); }
}
