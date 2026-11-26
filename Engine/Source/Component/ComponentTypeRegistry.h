#pragma once

#include "AssetReference.h"
#include "Component/Component.h"
#include "Core/Math/EulerAngles.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "ObjectID.h"

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

/**
 * 다른 객체의 컴포넌트를 가리키는 참조다. 주소 대신 소유 객체의 ID와 컴포넌트 타입으로 담아 다른 World에서도 다시 찾을 수 있다.
 * 같은 객체에 같은 타입 컴포넌트가 하나라는 규칙에 기댄다. objectID가 무효면 가리키지 않는 상태다.
 */
struct ComponentReference
{
	ObjectID objectID;
	std::type_index componentType = typeid(void);
};

// 값 하나다. 목록의 원소가 될 수 있는 종류다.
using ComponentFieldScalar = std::variant<bool, int, float, Vector2, Vector3, EulerAngles, Quaternion, Transform, AssetReference, ComponentReference, std::string>;

/** 값 하나의 목록이다. 원소는 다시 목록이 될 수 없다(중첩 배열은 지원하지 않는다). */
struct ComponentFieldList
{
	std::vector<ComponentFieldScalar> elements;
};

// 속성 값 하나를 담는다. 값 하나이거나 목록이다. 앞쪽 종류는 ComponentFieldScalar와 같은 순서로 둔다.
using ComponentFieldValue = std::variant<bool, int, float, Vector2, Vector3, EulerAngles, Quaternion, Transform, AssetReference, ComponentReference, std::string, ComponentFieldList>;

static_assert(std::variant_size_v<ComponentFieldValue> == std::variant_size_v<ComponentFieldScalar> + 1, "ComponentFieldValue must list every ComponentFieldScalar type plus ComponentFieldList.");

// Value가 shared_ptr<const T>이고 T가 Asset을 상속하면 true다. 이런 getter와 setter는 AssetReference로 감싸 등록한다.
// Asset 상속 여부와 typeid를 판단하므로, 호출하는 곳에서 에셋 타입의 전체 정의가 보여야 한다.
template <typename Value>
inline constexpr bool IsAssetPointer = false;

template <typename AssetType>
inline constexpr bool IsAssetPointer<std::shared_ptr<const AssetType>> = std::is_base_of_v<Asset, AssetType>;

// Value가 컴포넌트 포인터(const 여부 무관)면 true다. 이런 getter와 setter는 ComponentReference로 감싸 등록한다.
template <typename Value>
inline constexpr bool IsComponentPointer = std::is_pointer_v<Value>
	&& std::is_base_of_v<Component, std::remove_cv_t<std::remove_pointer_t<Value>>>;

// Value가 std::vector면 true다. 이런 getter와 setter는 ComponentFieldList로 감싸 등록한다.
template <typename Value>
inline constexpr bool IsFieldVector = false;

template <typename Element>
inline constexpr bool IsFieldVector<std::vector<Element>> = true;

// vector면 원소 타입, 아니면 Value 자신이다. vector가 아닐 때도 원소 타입을 계산할 수 있어야 해서 따로 둔다.
template <typename Value>
struct FieldElementOf { using Type = Value; };

template <typename Element>
struct FieldElementOf<std::vector<Element>> { using Type = Element; };

// 컴포넌트 값 타입을 속성 표에 담을 때의 타입이다. 에셋 포인터는 AssetReference, 컴포넌트 포인터는 ComponentReference, 그 밖은 그대로다.
template <typename Value>
using FieldStoredType = std::conditional_t<IsAssetPointer<Value>, AssetReference,
	std::conditional_t<IsComponentPointer<Value>, ComponentReference, Value>>;

// Value가 Variant의 대안 타입 중 하나와 정확히 같으면 true다.
template <typename Value, typename Variant>
inline constexpr bool IsVariantAlternative = false;

template <typename Value, typename... Types>
inline constexpr bool IsVariantAlternative<Value, std::variant<Types...>> = (std::is_same_v<Value, Types> || ...);

// Value가 ComponentFieldValue에 담을 수 있는 타입 중 하나와 정확히 같으면 true다.
template <typename Value>
inline constexpr bool IsComponentFieldValueType = IsVariantAlternative<Value, ComponentFieldValue>;

/** value 안의 실수가 모두 유한하면 true를 반환한다. bool, int, 문자열처럼 실수가 없는 값은 항상 true다. */
template <typename Value>
bool IsFiniteFieldValue(const Value& value)
{
	if constexpr (std::is_same_v<Value, float>) { return std::isfinite(value); }
	else if constexpr (std::is_same_v<Value, Vector2>) { return std::isfinite(value.x) && std::isfinite(value.y); }
	else if constexpr (std::is_same_v<Value, Vector3>) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
	else if constexpr (std::is_same_v<Value, EulerAngles>) { return std::isfinite(value.pitch) && std::isfinite(value.yaw) && std::isfinite(value.roll); }
	else if constexpr (std::is_same_v<Value, Quaternion>) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w); }
	else if constexpr (std::is_same_v<Value, Transform>)
	{
		return IsFiniteFieldValue(value.position) && IsFiniteFieldValue(value.scale)
			&& std::isfinite(value.rotation.x) && std::isfinite(value.rotation.y) && std::isfinite(value.rotation.z) && std::isfinite(value.rotation.w);
	}
	else { return true; }
}

// Inspector에서 속성을 어떻게 보여 줄지 정한다. 저장과 복제 여부와는 별개다.
enum class ComponentFieldVisibility : unsigned char
{
	// 보이고 편집할 수 있다.
	Editable,
	// 보이지만 편집 칸이 잠긴다.
	ReadOnly,
	// Inspector에 보이지 않는다.
	Hidden
};

/** Inspector가 속성을 그릴 때 쓰는 표시 정보다. 값이 올바른지는 setter가 판단하며, 이 정보는 화면에만 쓴다. */
struct ComponentFieldDisplay
{
	// 비어 있으면 속성 이름을 자동 변환해 쓴다.
	std::string displayName;
	ComponentFieldVisibility visibility = ComponentFieldVisibility::Editable;
	// 드래그 칸에서 마우스 1픽셀당 바뀌는 양이다.
	float dragSpeed = 0.1f;
	// minValue < maxValue일 때만 드래그 범위로 쓴다. 기본값(둘 다 0)은 범위 없음이다.
	float minValue = 0.0f;
	float maxValue = 0.0f;
	// Vector3를 RGB 색 선택기로 그린다. 1을 넘는 값(HDR)도 그대로 둔다.
	bool isColor = false;
	// int 속성을 드롭다운으로 그릴 때의 이름 목록이다. 값 0, 1, 2...가 차례로 각 이름에 대응한다. 비어 있으면 숫자 칸으로 그린다.
	std::vector<std::string> enumNames;
	// 목록 속성에서 Inspector가 원소 추가와 삭제 버튼을 그릴지 정한다. 원소 값 편집은 이 값과 상관없이 된다.
	// 원소 개수가 다른 상태와 맞아야 하는 목록(예: 애니메이션 노드)은 false로 둔다.
	bool canResize = false;
};

/** 컴포넌트 속성 하나의 이름, 읽기와 쓰기 함수 및 표시 정보다. 이 속성을 등록한 ComponentTypeInfo가 소유한다. */
struct ComponentFieldInfo
{
	// 같은 타입과 그 부모, 자손 사이에서 유일한 이름이다. camelCase로 쓰고 단위가 있으면 이름에 붙인다.
	std::string name;
	// component의 현재 값을 반환한다. 이 속성을 등록한 타입이나 그 파생 타입의 컴포넌트만 넘긴다.
	// 컴포넌트 참조(목록이면 원소 하나라도)가 CanReference를 통과하지 못하면 비어 있는 optional을 반환한다.
	std::function<std::optional<ComponentFieldValue>(const Component&)> get;
	// value를 component에 쓴다. 값 타입이 다르거나, 값에 유한하지 않은 수가 있거나, setter가 거부하면 false를 반환한다.
	std::function<bool(Component&, const ComponentFieldValue&)> set;
	ComponentFieldDisplay display;
	// true면 저장과 Play 복제에서 빼고 Inspector 편집에만 쓴다. 이 속성이 바꾸는 상태는 저장되는 다른 속성에도 담겨 있어야 한다.
	bool isTransient = false;
	// 목록 속성일 때 원소 하나의 기본값이다. 원소 타입을 알려 주며, 원소를 추가할 때 처음 값으로 쓴다. 목록이 아니면 비어 있다.
	std::optional<ComponentFieldScalar> elementPrototype;
};

/** 컴포넌트 타입 하나의 등록 정보다. ComponentTypeRegistry가 소유한다. */
struct ComponentTypeInfo
{
	// 등록부 안에서 유일한 타입 이름이다. 클래스 이름을 쓴다.
	std::string name;
	// 부모 타입의 정보다. Component를 직접 상속하면 nullptr이다. 같은 등록부가 소유한다.
	const ComponentTypeInfo* parent = nullptr;
	// 기본 생성자가 공개되지 않아 직접 만들 수 없는 타입이면 비어 있다.
	std::function<std::unique_ptr<Component>()> create;
	// component가 이 타입이거나 이 타입의 파생 타입이면 true를 반환한다. 직접 만들 수 없는 타입도 채운다.
	std::function<bool(const Component&)> isInstance;
	// 이 타입이 직접 등록한 속성이다. 부모의 속성은 parent를 따라가 얻는다.
	std::vector<ComponentFieldInfo> fields;
};

/**
 * 컴포넌트 타입의 이름, 부모, 생성 함수 및 속성을 보관하고 이름 또는 실제 타입으로 찾는다.
 * Engine이 값으로 소유하며 Get으로 어디서든 빌려 쓴다. 한 번에 하나만 존재해야 하고 메인 스레드에서 사용한다.
 */
class ComponentTypeRegistry
{
	// 생성과 소멸
public:
	/** 이 등록부를 Get의 대상으로 등록한다. 이미 등록된 등록부가 있으면 안 된다. */
	ComponentTypeRegistry();

	/** Get의 등록을 해제한다. */
	~ComponentTypeRegistry();

	ComponentTypeRegistry(const ComponentTypeRegistry&) = delete;
	ComponentTypeRegistry& operator=(const ComponentTypeRegistry&) = delete;
	ComponentTypeRegistry(ComponentTypeRegistry&&) = delete;
	ComponentTypeRegistry& operator=(ComponentTypeRegistry&&) = delete;

	// 전역 접근
public:
	/** 등록된 등록부를 빌려준다. 등록부가 살아 있는 동안(엔진 생성부터 소멸까지)만 호출한다. */
	static ComponentTypeRegistry& Get();

private:
	// 비소유. 생성자에서 등록하고 소멸자에서 비운다. 등록된 등록부가 없으면 nullptr이다.
	static ComponentTypeRegistry* instance;

	// 등록
public:
	/**
	 * T를 name으로 등록한다. Parent는 T의 부모이며 Component가 아니면 먼저 등록되어 있어야 한다.
	 * 이름이 비었거나, 이름이나 타입이 이미 있거나, 부모가 등록되지 않았거나,
	 * 등록된 중간 부모를 건너뛰었거나, 이미 등록된 자식보다 늦게 등록하면 로그를 남기고 nullptr을 반환한다.
	 * 상속 검사를 위해 등록 중에 붙이지 않은 임시 객체를 만들고 지우므로, 컴포넌트의 기본 생성과 소멸에는 부작용이 없어야 한다.
	 * 반환한 포인터는 등록부가 소멸할 때까지 유효하다.
	 */
	template <typename T, typename Parent>
	const ComponentTypeInfo* Register(std::string_view name);

	/**
	 * T의 속성 fieldName을 getter와 setter로 등록한다. T는 먼저 Register되어 있어야 한다.
	 * setter는 getter의 값 타입을 받고 void 또는 bool(성공 여부)을 반환해야 한다.
	 * display는 Inspector 표시 정보이며 생략하면 기본값(자동 이름, 편집 가능)을 쓴다.
	 * isTransient는 ComponentFieldInfo::isTransient의 계약을 따른다.
	 * 이름이 비었거나 T와 그 부모, 자손에 같은 이름이 있으면 로그를 남기고 false를 반환한다.
	 */
	template <typename T, typename Getter, typename Setter>
	bool AddField(std::string_view fieldName, Getter getter, Setter setter, ComponentFieldDisplay display = {}, bool isTransient = false);

private:
	/** Register의 타입 독립 부분이다. parentType이 nullptr이면 Component를 직접 상속한 타입이다. */
	const ComponentTypeInfo* AddType(std::string_view name, std::type_index type, const std::type_info* parentType, std::function<std::unique_ptr<Component>()> create, std::function<bool(const Component&)> isInstance);

	/**
	 * 새 타입의 부모 체인이 이미 등록된 타입들과의 실제 상속 관계와 맞는지 임시 객체로 확인한다.
	 * 맞지 않으면 로그를 남기고 false를 반환한다. 임시 객체는 만들 수 있는 타입(create가 있는 타입)으로만 만든다.
	 */
	bool IsHierarchyConsistent(std::string_view name, const ComponentTypeInfo* parent, const std::function<std::unique_ptr<Component>()>& create, const std::function<bool(const Component&)>& isInstance) const;

	/** AddField의 타입 독립 부분이다. 검사를 통과하면 field를 type의 fields에 넣는다. */
	bool AddFieldInfo(std::type_index type, ComponentFieldInfo field);

	/** referenced가 CanReference(*source, referenced)를 통과하면 그 소유 객체의 ID를 반환한다. source가 nullptr이거나 통과하지 못하면 비어 있다. */
	static std::optional<ObjectID> FindReferenceObjectID(const Component* source, const Component& referenced);

	/**
	 * target이 속한 World에서 reference가 가리키는 객체를 찾고, 그 객체에서 componentType이거나 그 파생인 컴포넌트를 찾는다.
	 * target이 객체에 붙어 있지 않거나, 객체나 컴포넌트가 없거나, componentType이 등록되지 않았으면 nullptr를 반환한다.
	 */
	static Component* FindReferencedComponent(Component& target, const ComponentReference& reference);

	/**
	 * 컴포넌트 쪽 값 하나를 속성 표에 담을 값으로 바꾼다. source는 이 값을 가진 컴포넌트이며 nullptr이 아닌 컴포넌트 참조를 담을 때만 쓴다.
	 * 컴포넌트 참조가 CanReference를 통과하지 못하면(source가 nullptr인 경우 포함) 비어 있는 optional을 반환한다.
	 */
	template <typename Value>
	static std::optional<FieldStoredType<Value>> ToStoredValue(const Value& value, const Component* source);

	/**
	 * 속성 표의 값 하나를 컴포넌트 쪽 값으로 되돌린다. target은 컴포넌트 참조를 찾을 World를 정한다.
	 * 유한하지 않은 수가 있거나, 다른 타입의 참조이거나, 참조한 컴포넌트를 찾지 못하면 비어 있는 optional을 반환한다.
	 */
	template <typename Value>
	static std::optional<Value> FromStoredValue(const FieldStoredType<Value>& stored, Component& target);

	// 조회
public:
	/** 이름으로 등록 정보를 찾는다. 없으면 nullptr을 반환한다. */
	const ComponentTypeInfo* FindByName(std::string_view name) const;

	/** component의 실제 타입 정보를 찾는다. 등록되지 않은 타입이면 nullptr을 반환한다. */
	const ComponentTypeInfo* Find(const Component& component) const;

	/** C++ 타입으로 등록 정보를 찾는다. 등록되지 않은 타입이면 nullptr을 반환한다. */
	const ComponentTypeInfo* FindByType(std::type_index type) const;

	/** type의 가장 위 부모 속성부터 type 자신의 속성까지 순서대로 visit을 호출한다. */
	static void ForEachField(const ComponentTypeInfo& type, const std::function<void(const ComponentFieldInfo&)>& visit);

	/**
	 * source의 컴포넌트 참조로 referenced를 담을 수 있으면 true를 반환한다.
	 * referenced가 source와 같은 World에 등록되어 있고 삭제 예정이 아닌 객체에 붙어 있어야 한다. 둘 중 하나라도 객체에 붙어 있지 않으면 false다.
	 */
	static bool CanReference(const Component& source, const Component& referenced);

private:
	// 등록 정보를 소유한다. parent와 색인이 주소를 가리키므로 등록이 늘어도 주소가 바뀌지 않게 따로 할당한다.
	std::vector<std::unique_ptr<ComponentTypeInfo>> types;
	// std::less<>로 string_view 조회 시 임시 문자열을 만들지 않는다.
	std::map<std::string, const ComponentTypeInfo*, std::less<>> typesByName;
	// AddFieldInfo가 찾은 타입의 fields에 추가하므로 수정 가능한 포인터를 담는다.
	std::unordered_map<std::type_index, ComponentTypeInfo*> typesByType;
};

template <typename T, typename Parent>
const ComponentTypeInfo* ComponentTypeRegistry::Register(std::string_view name)
{
	static_assert(std::is_base_of_v<Component, T>, "T must derive from Component.");
	static_assert(std::is_base_of_v<Parent, T>, "Parent must be a base of T.");
	static_assert(!std::is_same_v<T, Parent>, "T cannot be its own parent.");

	// 기본 생성자가 공개되지 않은 타입은 거짓 분기라 make_unique 코드가 만들어지지 않는다.
	std::function<std::unique_ptr<Component>()> create;
	if constexpr (std::is_default_constructible_v<T>)
	{
		create = [] { return std::make_unique<T>(); };
	}

	// dynamic_cast는 실제 객체가 T이거나 T의 파생일 때만 nullptr이 아닌 값을 준다.
	std::function<bool(const Component&)> isInstance = [](const Component& component) { return dynamic_cast<const T*>(&component) != nullptr; };

	const std::type_info* parentType = std::is_same_v<Parent, Component> ? nullptr : &typeid(Parent);

	return AddType(name, std::type_index(typeid(T)), parentType, std::move(create), std::move(isInstance));
}

template <typename T, typename Getter, typename Setter>
bool ComponentTypeRegistry::AddField(std::string_view fieldName, Getter getter, Setter setter, ComponentFieldDisplay display, bool isTransient)
{
	static_assert(std::is_base_of_v<Component, T>, "T must derive from Component.");

	// getter가 const 참조를 반환해도 값 타입만 남긴다.
	using Value = std::remove_cvref_t<std::invoke_result_t<Getter&, const T&>>;
	// vector면 원소 타입이고, 아니면 Value 자신이다.
	using Element = typename FieldElementOf<Value>::Type;
	// vector는 ComponentFieldList로 담고, 그 밖은 값 하나로 담는다.
	using StoredValue = std::conditional_t<IsFieldVector<Value>, ComponentFieldList, FieldStoredType<Value>>;
	static_assert(IsComponentFieldValueType<StoredValue>, "Getter must return a ComponentFieldValue type, shared_ptr<const Asset>, const Component* or std::vector of them.");

	using SetResult = std::invoke_result_t<Setter&, T&, const Value&>;
	static_assert(std::is_void_v<SetResult> || std::is_same_v<SetResult, bool>, "Setter must return void or bool.");

	ComponentFieldInfo field;
	field.name = std::string(fieldName);
	field.display = std::move(display);
	field.isTransient = isTransient;
	if constexpr (IsFieldVector<Value>)
	{
		static_assert(!IsFieldVector<Element>, "Nested vectors are not supported.");
		static_assert(IsVariantAlternative<FieldStoredType<Element>, ComponentFieldScalar>, "Vector element must be a ComponentFieldScalar type, shared_ptr<const Asset> or const Component*.");
		// 기본값의 참조는 nullptr라 담을 컴포넌트가 필요 없고 항상 성공한다.
		field.elementPrototype = ComponentFieldScalar(*ToStoredValue<Element>(Element{}, nullptr));
	}

	// T 이상의 타입에만 등록되므로 넘어온 component는 실제로 T이거나 T의 파생이다.
	field.get = [getter](const Component& component) -> std::optional<ComponentFieldValue>
	{
		const T& source = static_cast<const T&>(component);
		if constexpr (IsFieldVector<Value>)
		{
			// 원소마다 값 하나 변환을 다시 쓴다. 하나라도 담을 수 없으면 목록 전체를 담지 않는다.
			const Value& elements = std::invoke(getter, source);
			ComponentFieldList list;
			list.elements.reserve(elements.size());
			for (const Element& element : elements)
			{
				std::optional<FieldStoredType<Element>> stored = ToStoredValue<Element>(element, &component);
				if (!stored.has_value()) { return std::nullopt; }
				list.elements.push_back(ComponentFieldScalar(std::move(*stored)));
			}
			return ComponentFieldValue(std::move(list));
		}
		else
		{
			std::optional<FieldStoredType<Value>> stored = ToStoredValue<Value>(std::invoke(getter, source), &component);
			if (!stored.has_value()) { return std::nullopt; }
			return ComponentFieldValue(std::move(*stored));
		}
	};
	field.set = [setter](Component& component, const ComponentFieldValue& value)
	{
		const StoredValue* typed = std::get_if<StoredValue>(&value);
		if (typed == nullptr) { return false; }

		// 목록은 원소를 모두 되돌린 뒤에만 쓴다. 하나라도 실패하면 컴포넌트를 바꾸지 않는다.
		std::optional<Value> converted;
		if constexpr (IsFieldVector<Value>)
		{
			Value elements;
			elements.reserve(typed->elements.size());
			for (const ComponentFieldScalar& scalar : typed->elements)
			{
				const FieldStoredType<Element>* typedElement = std::get_if<FieldStoredType<Element>>(&scalar);
				if (typedElement == nullptr) { return false; }

				std::optional<Element> convertedElement = FromStoredValue<Element>(*typedElement, component);
				if (!convertedElement.has_value()) { return false; }
				elements.push_back(std::move(*convertedElement));
			}
			converted = std::move(elements);
		}
		else { converted = FromStoredValue<Value>(*typed, component); }
		if (!converted.has_value()) { return false; }

		T& target = static_cast<T&>(component);
		// SetPosition처럼 void를 반환하는 setter는 항상 성공으로 본다.
		auto invokeSetter = [&setter, &target](const Value& argument)
		{
			if constexpr (std::is_void_v<SetResult>)
			{
				std::invoke(setter, target, argument);
				return true;
			}
			else { return std::invoke(setter, target, argument); }
		};

		return invokeSetter(*converted);
	};
	return AddFieldInfo(std::type_index(typeid(T)), std::move(field));
}

template <typename Value>
std::optional<FieldStoredType<Value>> ComponentTypeRegistry::ToStoredValue(const Value& value, const Component* source)
{
	if constexpr (IsAssetPointer<Value>)
	{
		return AssetReference(value);
	}
	else if constexpr (IsComponentPointer<Value>)
	{
		// 주소 대신 소유 객체 ID와 타입으로 담는다. 무효 ID는 의도적으로 가리키지 않는 nullptr에만 쓴다.
		using Referenced = std::remove_cv_t<std::remove_pointer_t<Value>>;
		const std::type_index referencedType(typeid(Referenced));
		if (value == nullptr) { return ComponentReference{ ObjectID{}, referencedType }; }

		// 다른 World나 등록되지 않은 객체의 컴포넌트는 ID로 나타낼 수 없어 무효 ID로 바꾸지 않고 실패로 돌려준다.
		const std::optional<ObjectID> objectID = FindReferenceObjectID(source, *value);
		if (!objectID.has_value()) { return std::nullopt; }
		return ComponentReference{ *objectID, referencedType };
	}
	else { return value; }
}

template <typename Value>
std::optional<Value> ComponentTypeRegistry::FromStoredValue(const FieldStoredType<Value>& stored, Component& target)
{
	if (!IsFiniteFieldValue(stored)) { return std::nullopt; }

	if constexpr (IsAssetPointer<Value>)
	{
		// 다른 에셋 타입의 참조면 되돌릴 수 없으므로 거부한다. 같은 타입이면 static_pointer_cast가 안전하다.
		if (stored.GetType() != std::type_index(typeid(typename Value::element_type))) { return std::nullopt; }
		return std::static_pointer_cast<typename Value::element_type>(stored.GetAsset());
	}
	else if constexpr (IsComponentPointer<Value>)
	{
		using Referenced = std::remove_cv_t<std::remove_pointer_t<Value>>;
		if (stored.componentType != std::type_index(typeid(Referenced))) { return std::nullopt; }

		// 무효 ID는 "가리키지 않음"이라는 정상 값이므로 nullptr를 담아 돌려준다. 비어 있는 optional(실패)과 다르다.
		if (!stored.objectID.IsValid()) { return Value{ nullptr }; }

		// target의 World에서 다시 찾는다. 찾았다면 isInstance를 통과했으므로 static_cast가 안전하다.
		Component* found = FindReferencedComponent(target, stored);
		if (found == nullptr) { return std::nullopt; }
		return static_cast<Value>(found);
	}
	else { return stored; }
}
