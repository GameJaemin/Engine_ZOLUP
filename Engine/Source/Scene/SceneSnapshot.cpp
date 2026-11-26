#include "SceneSnapshot.h"

#include "Component/CameraComponent.h"
#include "Component/Component.h"
#include "Component/ComponentTypeRegistry.h"
#include "Core/Log.h"
#include "GameObject.h"
#include "World.h"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
	/** typeInfo나 그 부모에 저장되는 속성(isTransient가 아닌 속성)이 하나라도 있으면 true를 반환한다. */
	bool HasSavedField(const ComponentTypeInfo& typeInfo)
	{
		const ComponentTypeInfo* current = &typeInfo;
		while (current != nullptr)
		{
			for (const ComponentFieldInfo& field : current->fields)
			{
				if (!field.isTransient) { return true; }
			}
			current = current->parent;
		}
		return false;
	}

	/** object 하나를 속성 표로 담아 snapshot 끝에 더한다. 담을 수 없는 객체나 컴포넌트를 만나면 로그를 남기고 false를 반환한다. */
	bool CaptureGameObject(const GameObject& object, SceneSnapshot& snapshot)
	{
		// 파생 타입은 생성자가 붙이는 컴포넌트와 자체 상태가 있어 기본 객체로 바꿔 담지 않는다.
		if (typeid(object) != typeid(GameObject))
		{
			Log::Write(LogLevel::Error, "Snapshot", std::format("{}: derived GameObject type is not supported", object.GetName()));
			return false;
		}

		GameObjectSnapshot data{ .objectID = object.GetObjectID(), .name = object.GetName() };
		for (const GameObject* child : object.GetChildren())
		{
			// 삭제 요청은 자손에게 전파되므로 담는 객체의 부모는 항상 함께 담긴다. 삭제 예정 자식만 빼면 된다.
			if (child->IsPendingDestroy()) { continue; }
			data.childObjectIDs.push_back(child->GetObjectID());
		}

		const ComponentTypeRegistry& registry = ComponentTypeRegistry::Get();
		std::unordered_set<const ComponentTypeInfo*> capturedTypes;
		for (const std::unique_ptr<Component>& component : object.GetComponents())
		{
			const ComponentTypeInfo* typeInfo = registry.Find(*component);
			if (typeInfo == nullptr)
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: unregistered component {}", object.GetName(), typeid(*component).name()));
				return false;
			}
			if (!typeInfo->create)
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: component {} cannot be created", object.GetName(), typeInfo->name));
				return false;
			}
			// 저장되는 속성이 없는 타입은 상태를 옮길 수 없으므로 빈 컴포넌트로 바꿔 담지 않는다.
			if (!HasSavedField(*typeInfo))
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: component {} has no saved fields", object.GetName(), typeInfo->name));
				return false;
			}
			// 컴포넌트 참조는 "같은 객체에 같은 타입은 하나"라는 규칙에 기대므로 중복을 거부한다.
			if (!capturedTypes.insert(typeInfo).second)
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: duplicate {}", object.GetName(), typeInfo->name));
				return false;
			}

			ComponentSnapshot componentData{ .typeName = typeInfo->name };
			bool allFieldsCaptured = true;
			ComponentTypeRegistry::ForEachField(*typeInfo, [&](const ComponentFieldInfo& field)
			{
				// 편집 전용 속성의 상태는 저장되는 다른 속성에 담겨 있으므로 담지 않는다.
				if (!allFieldsCaptured || field.isTransient) { return; }

				// 비어 있으면 다른 World나 등록되지 않은 객체의 컴포넌트를 가리키는 참조가 있다. 빈 참조로 바꿔 담지 않는다.
				std::optional<ComponentFieldValue> value = field.get(*component);
				if (!value.has_value())
				{
					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: {}.{} refers to a component outside this world", object.GetName(), typeInfo->name, field.name));
					allFieldsCaptured = false;
					return;
				}
				componentData.fields.emplace_back(field.name, std::move(*value));
			});
			if (!allFieldsCaptured) { return false; }
			data.components.push_back(std::move(componentData));
		}

		snapshot.objects.push_back(std::move(data));
		return true;
	}

	// 스냅샷 안의 객체를 ID로 찾는 표다. 값은 스냅샷 안 객체의 주소라 스냅샷이 살아 있는 동안만 쓴다.
	using ObjectSnapshotMap = std::unordered_map<ObjectID, const GameObjectSnapshot*>;

	/** 스냅샷의 객체를 ID로 찾는 표를 만든다. */
	ObjectSnapshotMap MapObjectsByID(const SceneSnapshot& snapshot)
	{
		ObjectSnapshotMap objectsByID;
		for (const GameObjectSnapshot& object : snapshot.objects) { objectsByID.emplace(object.objectID, &object); }
		return objectsByID;
	}

	/** typeName 타입이 required 타입이거나 그 파생이면 true를 반환한다. 등록부의 부모 연결을 따라 올라가며 확인한다. */
	bool IsTypeOrDerived(const std::string& typeName, const ComponentTypeInfo& required)
	{
		const ComponentTypeInfo* current = ComponentTypeRegistry::Get().FindByName(typeName);
		while (current != nullptr)
		{
			if (current == &required) { return true; }
			current = current->parent;
		}
		return false;
	}

	/** reference가 가리키지 않거나, 스냅샷 안의 객체이며 그 객체에 해당 타입 컴포넌트가 있으면 true를 반환한다. */
	bool IsReferenceInSnapshot(const ComponentReference& reference, const ObjectSnapshotMap& objectsByID)
	{
		if (!reference.objectID.IsValid()) { return true; }

		const auto foundObject = objectsByID.find(reference.objectID);
		const ComponentTypeInfo* required = ComponentTypeRegistry::Get().FindByType(reference.componentType);
		if (foundObject == objectsByID.end() || required == nullptr) { return false; }

		for (const ComponentSnapshot& component : foundObject->second->components)
		{
			if (IsTypeOrDerived(component.typeName, *required)) { return true; }
		}
		return false;
	}

	/** value 안의 컴포넌트 참조가 모두 스냅샷 안을 가리키면 true를 반환한다. 목록이면 원소마다 확인하고, 참조가 아니면 true다. */
	bool AreReferencesInSnapshot(const ComponentFieldValue& value, const ObjectSnapshotMap& objectsByID)
	{
		if (const ComponentReference* reference = std::get_if<ComponentReference>(&value))
		{
			return IsReferenceInSnapshot(*reference, objectsByID);
		}

		const ComponentFieldList* list = std::get_if<ComponentFieldList>(&value);
		if (list == nullptr) { return true; }

		for (const ComponentFieldScalar& element : list->elements)
		{
			const ComponentReference* reference = std::get_if<ComponentReference>(&element);
			if (reference != nullptr && !IsReferenceInSnapshot(*reference, objectsByID)) { return false; }
		}
		return true;
	}

	/**
	 * 모든 속성 값 안의 컴포넌트 참조가 스냅샷 안을 가리키는지 확인한다. 아니면 로그를 남기고 false를 반환한다.
	 * 다른 World나 등록되지 않은 객체를 가리킨 참조는 속성을 담을 때(field.get) 이미 걸러진다.
	 */
	bool ValidateReferences(const SceneSnapshot& snapshot)
	{
		const ObjectSnapshotMap objectsByID = MapObjectsByID(snapshot);

		for (const GameObjectSnapshot& object : snapshot.objects)
		{
			for (const ComponentSnapshot& component : object.components)
			{
				for (const std::pair<std::string, ComponentFieldValue>& field : component.fields)
				{
					if (AreReferencesInSnapshot(field.second, objectsByID)) { continue; }

					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: {}.{} refers to a component outside the snapshot", object.name, component.typeName, field.first));
					return false;
				}
			}
		}
		return true;
	}

	/** typeInfo와 그 부모에서 fieldName 속성을 찾는다. 등록부가 타입과 조상, 자손 사이의 이름 중복을 막으므로 결과는 하나다. 없으면 nullptr을 반환한다. */
	const ComponentFieldInfo* FindField(const ComponentTypeInfo& typeInfo, const std::string& fieldName)
	{
		const ComponentTypeInfo* current = &typeInfo;
		while (current != nullptr)
		{
			for (const ComponentFieldInfo& field : current->fields)
			{
				if (field.name == fieldName) { return &field; }
			}
			current = current->parent;
		}
		return nullptr;
	}

	/** 컴포넌트 참조이거나 컴포넌트 참조의 목록이면 true다. 이런 속성은 모든 컴포넌트를 붙인 뒤에 쓴다. */
	bool IsDeferredField(const ComponentFieldInfo& field, const ComponentFieldValue& value)
	{
		if (std::holds_alternative<ComponentReference>(value)) { return true; }
		// 빈 목록은 원소로 타입을 알 수 없으므로 원소 기본값의 종류를 본다.
		return field.elementPrototype.has_value() && std::holds_alternative<ComponentReference>(*field.elementPrototype);
	}

	/** object에 이미 붙은 컴포넌트 중 실제 타입이 typeInfo인 것을 찾는다. 생성자가 붙인 Transform처럼 새로 만들면 중복되는 경우에 쓴다. 없으면 nullptr을 반환한다. */
	Component* FindAttachedComponent(GameObject& object, const ComponentTypeInfo& typeInfo)
	{
		const ComponentTypeRegistry& registry = ComponentTypeRegistry::Get();
		for (const std::unique_ptr<Component>& component : object.GetComponents())
		{
			if (registry.Find(*component) == &typeInfo) { return component.get(); }
		}
		return nullptr;
	}

	/**
	 * object에 컴포넌트 하나를 복원한다. 이미 같은 타입이 붙어 있으면 그것에 쓰고, 아니면 만들어서 속성을 쓴 뒤 붙인다.
	 * 컴포넌트 참조 속성은 건너뛰며, 모든 컴포넌트를 붙인 뒤 따로 쓴다. 실패하면 로그를 남기고 false를 반환한다.
	 */
	bool RestoreComponent(GameObject& object, const ComponentSnapshot& data)
	{
		const ComponentTypeInfo* typeInfo = ComponentTypeRegistry::Get().FindByName(data.typeName);
		if (typeInfo == nullptr || !typeInfo->create)
		{
			Log::Write(LogLevel::Error, "Snapshot", std::format("{}: cannot create component {}", object.GetName(), data.typeName));
			return false;
		}

		// 생성자가 이미 붙인 Transform처럼 같은 타입이 있으면 새로 만들지 않는다.
		Component* target = FindAttachedComponent(object, *typeInfo);
		std::unique_ptr<Component> created;
		if (target == nullptr)
		{
			created = typeInfo->create();
			target = created.get();
		}

		for (const std::pair<std::string, ComponentFieldValue>& field : data.fields)
		{
			const ComponentFieldInfo* fieldInfo = FindField(*typeInfo, field.first);
			if (fieldInfo == nullptr)
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: {} has no field {}", object.GetName(), data.typeName, field.first));
				return false;
			}
			if (IsDeferredField(*fieldInfo, field.second)) { continue; }
			if (!fieldInfo->set(*target, field.second))
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: {}.{} rejected the value", object.GetName(), data.typeName, field.first));
				return false;
			}
		}

		// 붙이기 전에 값을 채워, 붙일 때 호출되는 OnRegister가 완성된 값을 보게 한다.
		if (created != nullptr) { object.AddComponent(std::move(created)); }
		return true;
	}

	/** object에 이미 붙은 컴포넌트에 컴포넌트 참조 속성만 쓴다. 모든 컴포넌트를 붙인 뒤 부른다. 실패하면 로그를 남기고 false를 반환한다. */
	bool RestoreDeferredFields(GameObject& object, const ComponentSnapshot& data)
	{
		// RestoreComponent가 같은 이름으로 타입과 속성을 찾고 컴포넌트를 붙인 뒤에만 부르므로 셋 다 있다.
		const ComponentTypeInfo& typeInfo = *ComponentTypeRegistry::Get().FindByName(data.typeName);
		Component& target = *FindAttachedComponent(object, typeInfo);

		for (const std::pair<std::string, ComponentFieldValue>& field : data.fields)
		{
			const ComponentFieldInfo& fieldInfo = *FindField(typeInfo, field.first);
			if (!IsDeferredField(fieldInfo, field.second)) { continue; }
			if (!fieldInfo.set(target, field.second))
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: {}.{} could not connect the reference", object.GetName(), data.typeName, field.first));
				return false;
			}
		}
		return true;
	}

	/** 빈 World에 객체와 컴포넌트를 만든다. 실패하면 이미 만든 객체를 남긴 채 false를 반환하며 정리는 호출자가 한다. */
	bool RestoreObjects(const SceneSnapshot& snapshot, World& world)
	{
		// 1) 모든 객체를 배열 순서대로 만들고 저장된 ID로 등록한다. 등록 순서가 원본과 같아진다.
		//    계층 연결은 등록된 객체끼리만 가능하므로 모두 등록한 뒤에 한다.
		for (const GameObjectSnapshot& data : snapshot.objects)
		{
			std::unique_ptr<GameObject> object = std::make_unique<GameObject>(world);
			object->SetName(data.name);
			if (world.AddGameObject(std::move(object), data.objectID) == nullptr)
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: could not register ObjectID {}", data.name, data.objectID.ToString()));
				return false;
			}
		}

		// 2) SetParent는 자식을 부모 목록 끝에 붙이므로 각 부모의 자식 목록 순서대로 붙이면 자식 순서가 원본과 같아진다.
		for (const GameObjectSnapshot& data : snapshot.objects)
		{
			GameObject* parent = world.FindGameObject(data.objectID);
			for (const ObjectID& childObjectID : data.childObjectIDs)
			{
				GameObject* child = world.FindGameObject(childObjectID);
				// 이미 부모가 있으면 두 부모의 자식 목록에 있는 손상된 데이터다. 순환은 SetParent가 거절한다.
				if (child == nullptr || child->GetParent() != nullptr || !child->SetParent(parent))
				{
					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: could not attach child {}", data.name, childObjectID.ToString()));
					return false;
				}
			}
		}

		// 3) 컴포넌트를 담긴 순서(원본 객체의 컴포넌트 순서)대로 복원한다. 컴포넌트 참조 속성은 아직 쓰지 않는다.
		for (const GameObjectSnapshot& data : snapshot.objects)
		{
			GameObject& object = *world.FindGameObject(data.objectID);
			for (const ComponentSnapshot& component : data.components)
			{
				if (!RestoreComponent(object, component)) { return false; }
			}
		}

		// 4) 모든 컴포넌트를 붙인 뒤 컴포넌트 참조 속성을 쓴다. 가리킬 컴포넌트가 이제 모두 있다.
		for (const GameObjectSnapshot& data : snapshot.objects)
		{
			GameObject& object = *world.FindGameObject(data.objectID);
			for (const ComponentSnapshot& component : data.components)
			{
				if (!RestoreDeferredFields(object, component)) { return false; }
			}
		}

		// 5) World 상태
		if (snapshot.activeCameraObjectID.IsValid())
		{
			GameObject* owner = world.FindGameObject(snapshot.activeCameraObjectID);
			CameraComponent* camera = owner != nullptr ? owner->GetComponent<CameraComponent>() : nullptr;
			if (camera == nullptr || !world.SetActiveCamera(camera))
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("could not activate camera of {}", snapshot.activeCameraObjectID.ToString()));
				return false;
			}
		}
		return true;
	}
}

std::optional<SceneSnapshot> CaptureWorld(const World& world)
{
	SceneSnapshot snapshot;
	// 등록 순서가 Tick 순서이므로 계층이 아니라 World 목록 순서대로 담는다.
	for (const std::unique_ptr<GameObject>& object : world.GetGameObjects())
	{
		if (object->IsPendingDestroy()) { continue; }
		if (!CaptureGameObject(*object, snapshot)) { return std::nullopt; }
	}
	if (!ValidateReferences(snapshot)) { return std::nullopt; }

	if (const CameraComponent* camera = world.GetActiveCamera())
	{
		const GameObject* owner = camera->GetGameObject();
		// 삭제 예정 객체는 담지 않으므로 그 카메라도 활성으로 담지 않는다.
		if (!owner->IsPendingDestroy()) { snapshot.activeCameraObjectID = owner->GetObjectID(); }
	}
	return snapshot;
}

bool RestoreWorld(const SceneSnapshot& snapshot, World& world)
{
	// 이미 있는 객체는 호출자의 것이므로 건드리지 않고 거절한다.
	if (!world.GetGameObjects().empty())
	{
		Log::Write(LogLevel::Error, "Snapshot", "Restore target world is not empty");
		return false;
	}
	if (RestoreObjects(snapshot, world)) { return true; }

	// 실패하면 이번에 만든 객체를 모두 지워 빈 World로 되돌린다. 삭제 요청은 표시만 하므로 순회 중에도 된다.
	// 활성 카메라는 마지막 단계에서만 지정하므로 지정되지 않은 상태다.
	for (const std::unique_ptr<GameObject>& object : world.GetGameObjects())
	{
		world.DestroyGameObject(object.get());
	}
	world.DestroyPendingGameObjects();
	return false;
}
