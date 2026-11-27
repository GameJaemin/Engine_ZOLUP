#include "SnapshotJson.h"

#include "AssetManager.h"
#include "Core/Log.h"
#include "Core/Math/EulerAngles.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "ObjectID.h"
#include "SavedAssetReference.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_set>
#include <utility>
#include <variant>

namespace
{
	const char* ToTextureFormatName(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::RGBA8UNorm: return "RGBA8UNorm";
		case TextureFormat::RGBA8SRGB: return "RGBA8SRGB";
		default: return "Unknown";
		}
	}

	/** 저장하는 텍스처 형식 이름이면 형식을, 아니면 nullopt를 반환한다. Unknown은 저장하지 않으므로 받지 않는다. */
	std::optional<TextureFormat> FindTextureFormat(std::string_view name)
	{
		if (name == "RGBA8UNorm") { return TextureFormat::RGBA8UNorm; }
		if (name == "RGBA8SRGB") { return TextureFormat::RGBA8SRGB; }
		return std::nullopt;
	}

	/**
	 * float를 JSON에 적을 double로 바꾼다. float를 그대로 double로 바꾸면 0.1f가 0.10000000149011612처럼 길게 적히므로,
	 * float를 되돌릴 수 있는 가장 짧은 10진수를 거쳐 바꾼다. 이 double을 다시 float로 바꾸면 원래 값과 같다.
	 */
	double ToJsonNumber(float value)
	{
		std::array<char, 32> text{};
		const std::to_chars_result written = std::to_chars(text.data(), text.data() + text.size(), value);
		double result = value;
		std::from_chars(text.data(), written.ptr, result);
		return result;
	}

	/** text를 문서 할당기에 복사한 JSON 문자열을 만든다. */
	rapidjson::Value MakeString(std::string_view text, rapidjson::Document::AllocatorType& allocator)
	{
		return rapidjson::Value(text.data(), static_cast<rapidjson::SizeType>(text.size()), allocator);
	}

	rapidjson::Value MakeFloatArray(std::initializer_list<float> values, rapidjson::Document::AllocatorType& allocator)
	{
		rapidjson::Value array(rapidjson::kArrayType);
		for (const float value : values)
		{
			array.PushBack(ToJsonNumber(value), allocator);
		}
		return array;
	}

	/** 목록이 아닌 값 하나를 쓴다. Variant는 ComponentFieldValue나 ComponentFieldScalar다. 에셋 참조를 저장할 수 없으면 false다. */
	template <typename Variant>
	bool WriteScalar(const Variant& value, AssetManager& assets, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& out, std::string_view context)
	{
		if (const bool* v = std::get_if<bool>(&value))
		{
			out.SetBool(*v);
			return true;
		}
		if (const int* v = std::get_if<int>(&value))
		{
			out.SetInt(*v);
			return true;
		}
		if (const float* v = std::get_if<float>(&value))
		{
			out.SetDouble(ToJsonNumber(*v));
			return true;
		}
		if (const Vector2* v = std::get_if<Vector2>(&value))
		{
			out = MakeFloatArray({ v->x, v->y }, allocator);
			return true;
		}
		if (const Vector3* v = std::get_if<Vector3>(&value))
		{
			out = MakeFloatArray({ v->x, v->y, v->z }, allocator);
			return true;
		}
		if (const EulerAngles* v = std::get_if<EulerAngles>(&value))
		{
			out = MakeFloatArray({ v->pitch, v->yaw, v->roll }, allocator);
			return true;
		}
		if (const Quaternion* v = std::get_if<Quaternion>(&value))
		{
			out = MakeFloatArray({ v->x, v->y, v->z, v->w }, allocator);
			return true;
		}
		if (const Transform* v = std::get_if<Transform>(&value))
		{
			out.SetObject();
			out.AddMember("position", MakeFloatArray({ v->position.x, v->position.y, v->position.z }, allocator), allocator);
			out.AddMember("rotation", MakeFloatArray({ v->rotation.x, v->rotation.y, v->rotation.z, v->rotation.w }, allocator), allocator);
			out.AddMember("scale", MakeFloatArray({ v->scale.x, v->scale.y, v->scale.z }, allocator), allocator);
			return true;
		}
		if (const std::string* v = std::get_if<std::string>(&value))
		{
			out = MakeString(*v, allocator);
			return true;
		}
		if (const ComponentReference* v = std::get_if<ComponentReference>(&value))
		{
			// 컴포넌트 타입은 읽을 때 견본이 알려 주므로 대상 객체 ID만 적는다.
			if (v->objectID.IsValid())
			{
				out = MakeString(v->objectID.ToString(), allocator);
			}
			else
			{
				out.SetNull();
			}
			return true;
		}

		// 남은 종류는 에셋 참조다. 경로로 같은 에셋을 다시 얻을 수 있을 때만 성공한다.
		const std::optional<SavedAssetReference> saved = SaveAssetReference(std::get<AssetReference>(value), assets, context);
		if (!saved.has_value()) { return false; }
		if (saved->path.empty())
		{
			out.SetNull();
			return true;
		}
		out.SetObject();
		out.AddMember("path", MakeString(saved->path, allocator), allocator);
		// name은 파일 안의 에셋('|'가 있는 경로)만, format은 텍스처만 쓴다.
		if (!saved->name.empty())
		{
			out.AddMember("name", MakeString(saved->name, allocator), allocator);
		}
		if (saved->textureFormat != TextureFormat::Unknown)
		{
			out.AddMember("format", rapidjson::StringRef(ToTextureFormatName(saved->textureFormat)), allocator);
		}
		return true;
	}

	/** 유한하고 float 범위 안의 수면 out에 담는다. */
	bool ReadFloat(const rapidjson::Value& json, float& out)
	{
		if (!json.IsNumber()) { return false; }
		const double value = json.GetDouble();
		if (!std::isfinite(value) || std::abs(value) > static_cast<double>(std::numeric_limits<float>::max())) { return false; }
		out = static_cast<float>(value);
		return true;
	}

	/** 수 Count개의 배열이면 out에 담는다. */
	template <std::size_t Count>
	bool ReadFloatArray(const rapidjson::Value& json, std::array<float, Count>& out)
	{
		if (!json.IsArray() || json.Size() != Count) { return false; }
		for (rapidjson::SizeType i = 0; i < Count; ++i)
		{
			if (!ReadFloat(json[i], out[i])) { return false; }
		}
		return true;
	}

	/** object의 name 멤버를 Count개의 수 배열로 읽는다. 멤버가 없거나 모양이 다르면 false다. */
	template <std::size_t Count>
	bool ReadFloatArrayMember(const rapidjson::Value& object, const char* name, std::array<float, Count>& out)
	{
		const auto member = object.FindMember(name);
		return member != object.MemberEnd() && ReadFloatArray(member->value, out);
	}

	/** 에셋 참조 객체를 읽는다. path는 필수이고 name과 format은 선택이며, 그 밖의 항목이 있으면 실패한다. */
	std::optional<AssetReference> ReadAssetReference(const rapidjson::Value& json, std::type_index assetType, AssetManager& assets, std::string_view context)
	{
		if (json.IsNull()) { return AssetReference(assetType); }
		if (!json.IsObject()) { return std::nullopt; }

		SavedAssetReference saved;
		saved.type = std::string(GetSavedAssetTypeName(assetType));
		bool hasPath = false;
		for (const auto& member : json.GetObject())
		{
			const std::string_view key(member.name.GetString(), member.name.GetStringLength());
			if (!member.value.IsString()) { return std::nullopt; }
			const std::string_view text(member.value.GetString(), member.value.GetStringLength());
			if (key == "path")
			{
				saved.path = text;
				hasPath = true;
			}
			else if (key == "name")
			{
				saved.name = text;
			}
			else if (key == "format")
			{
				const std::optional<TextureFormat> format = FindTextureFormat(text);
				if (!format.has_value()) { return std::nullopt; }
				saved.textureFormat = *format;
			}
			else
			{
				return std::nullopt;
			}
		}
		// 빈 경로는 null로 적으므로 객체 안의 path는 비어 있으면 안 된다.
		if (!hasPath || saved.path.empty()) { return std::nullopt; }
		return LoadAssetReference(saved, assets, context);
	}

	/** json을 prototype과 같은 종류의 값 하나로 읽는다. 종류가 맞지 않으면 nullopt다. 에셋을 얻지 못한 이유는 LoadAssetReference가 로그로 남긴다. */
	template <typename Variant>
	std::optional<Variant> ReadScalar(const rapidjson::Value& json, const Variant& prototype, AssetManager& assets, std::string_view context)
	{
		if (std::holds_alternative<bool>(prototype))
		{
			if (!json.IsBool()) { return std::nullopt; }
			return Variant(json.GetBool());
		}
		if (std::holds_alternative<int>(prototype))
		{
			if (!json.IsInt()) { return std::nullopt; }
			return Variant(json.GetInt());
		}
		if (std::holds_alternative<float>(prototype))
		{
			float value = 0.0f;
			if (!ReadFloat(json, value)) { return std::nullopt; }
			return Variant(value);
		}
		if (std::holds_alternative<Vector2>(prototype))
		{
			std::array<float, 2> a{};
			if (!ReadFloatArray(json, a)) { return std::nullopt; }
			return Variant(Vector2{ a[0], a[1] });
		}
		if (std::holds_alternative<Vector3>(prototype))
		{
			std::array<float, 3> a{};
			if (!ReadFloatArray(json, a)) { return std::nullopt; }
			return Variant(Vector3{ a[0], a[1], a[2] });
		}
		if (std::holds_alternative<EulerAngles>(prototype))
		{
			std::array<float, 3> a{};
			if (!ReadFloatArray(json, a)) { return std::nullopt; }
			return Variant(EulerAngles{ a[0], a[1], a[2] });
		}
		if (std::holds_alternative<Quaternion>(prototype))
		{
			std::array<float, 4> a{};
			if (!ReadFloatArray(json, a)) { return std::nullopt; }
			return Variant(Quaternion{ a[0], a[1], a[2], a[3] });
		}
		if (std::holds_alternative<Transform>(prototype))
		{
			// position, rotation, scale 세 멤버만 있어야 한다.
			std::array<float, 3> position{};
			std::array<float, 4> rotation{};
			std::array<float, 3> scale{};
			if (!json.IsObject() || json.MemberCount() != 3) { return std::nullopt; }
			if (!ReadFloatArrayMember(json, "position", position) || !ReadFloatArrayMember(json, "rotation", rotation) || !ReadFloatArrayMember(json, "scale", scale)) { return std::nullopt; }
			Transform transform;
			transform.position = Vector3{ position[0], position[1], position[2] };
			transform.rotation = Quaternion{ rotation[0], rotation[1], rotation[2], rotation[3] };
			transform.scale = Vector3{ scale[0], scale[1], scale[2] };
			return Variant(transform);
		}
		if (std::holds_alternative<std::string>(prototype))
		{
			if (!json.IsString()) { return std::nullopt; }
			return Variant(std::string(json.GetString(), json.GetStringLength()));
		}
		if (const ComponentReference* reference = std::get_if<ComponentReference>(&prototype))
		{
			// 가리키는 컴포넌트 타입은 견본에서 가져온다.
			if (json.IsNull()) { return Variant(ComponentReference{ ObjectID{}, reference->componentType }); }
			if (!json.IsString()) { return std::nullopt; }
			const std::optional<ObjectID> objectID = ObjectID::Parse(std::string_view(json.GetString(), json.GetStringLength()));
			if (!objectID.has_value()) { return std::nullopt; }
			return Variant(ComponentReference{ *objectID, reference->componentType });
		}

		// 남은 종류는 에셋 참조다. 에셋 타입은 견본에서 가져온다.
		std::optional<AssetReference> asset = ReadAssetReference(json, std::get<AssetReference>(prototype).GetType(), assets, context);
		if (!asset.has_value()) { return std::nullopt; }
		return Variant(std::move(*asset));
	}

	void LogReadFailure(std::string_view context)
	{
		Log::Write(LogLevel::Error, "Snapshot", std::format("{}: value does not match the field type", context));
	}

	void LogFormatError(std::string_view context, std::string_view reason)
	{
		Log::Write(LogLevel::Error, "Snapshot", std::format("{}: {}", context, reason));
	}

	/** object가 JSON 객체이고 names에 있는 이름의 항목만 가지면 true다. 파일 구조의 오타를 조용히 넘기지 않으려고 쓴다. */
	bool HasOnlyMembers(const rapidjson::Value& object, std::initializer_list<std::string_view> names)
	{
		if (!object.IsObject()) { return false; }
		for (const auto& member : object.GetObject())
		{
			const std::string_view key(member.name.GetString(), member.name.GetStringLength());
			if (std::find(names.begin(), names.end(), key) == names.end()) { return false; }
		}
		return true;
	}

	/** json이 유효한 ObjectID 문자열이면 out에 담는다. */
	bool ReadObjectID(const rapidjson::Value& json, ObjectID& out)
	{
		if (!json.IsString()) { return false; }
		const std::optional<ObjectID> objectID = ObjectID::Parse(std::string_view(json.GetString(), json.GetStringLength()));
		if (!objectID.has_value()) { return false; }
		out = *objectID;
		return true;
	}

	/** object의 name 항목이 문자열이면 out에 담는다. 항목이 없으면 false다. */
	bool ReadStringMember(const rapidjson::Value& object, const char* name, std::string& out)
	{
		const auto member = object.FindMember(name);
		if (member == object.MemberEnd() || !member->value.IsString()) { return false; }
		out.assign(member->value.GetString(), member->value.GetStringLength());
		return true;
	}

	/** 컴포넌트 항목 하나를 읽어 outComponent에 담는다. 실패하면 이유를 로그로 남기고 false를 반환한다. */
	bool ReadComponent(const rapidjson::Value& json, AssetManager& assets, std::string_view context, ComponentSnapshot& outComponent)
	{
		if (!HasOnlyMembers(json, { "type", "fields" }))
		{
			LogFormatError(context, "component must be an object with only type and fields");
			return false;
		}
		std::string typeName;
		if (!ReadStringMember(json, "type", typeName))
		{
			LogFormatError(context, "component type must be a string");
			return false;
		}
		// 모르는 컴포넌트 타입은 실패한다. 건너뛰면 다시 저장할 때 객체의 일부가 조용히 사라진다.
		const ComponentTypeInfo* typeInfo = ComponentTypeRegistry::Get().FindByName(typeName);
		if (typeInfo == nullptr)
		{
			LogFormatError(context, std::format("unknown component type {}", typeName));
			return false;
		}
		outComponent.typeName = typeInfo->name;

		const auto fields = json.FindMember("fields");
		if (fields == json.MemberEnd() || !fields->value.IsObject())
		{
			LogFormatError(context, std::format("{} fields must be an object", typeName));
			return false;
		}
		for (const auto& member : fields->value.GetObject())
		{
			const std::string_view fieldName(member.name.GetString(), member.name.GetStringLength());
			const std::string fieldContext = std::format("{}.{}.{}", context, typeName, fieldName);
			const ComponentFieldInfo* field = ComponentTypeRegistry::FindField(*typeInfo, fieldName);
			// 모르는 속성은 경고 후 건너뛴다. 속성 이름 하나를 바꿔도 모든 파일이 열리지 않는 일을 막는다. 다시 저장하면 그 값은 사라진다.
			// 저장 제외 속성은 같은 상태가 저장되는 다른 속성에 담기므로 같은 방식으로 건너뛴다.
			if (field == nullptr || field->isTransient)
			{
				Log::Write(LogLevel::Warning, "Snapshot", std::format("{}: unknown or unsaved field is ignored", fieldContext));
				continue;
			}
			// JSON 문법은 같은 이름을 막지 않는다. 어느 값이 맞는지 알 수 없어 실패한다.
			const bool isDuplicate = std::any_of(outComponent.fields.begin(), outComponent.fields.end(), [field](const std::pair<std::string, ComponentFieldValue>& existing) { return existing.first == field->name; });
			if (isDuplicate)
			{
				LogFormatError(fieldContext, "field appears twice");
				return false;
			}

			std::optional<ComponentFieldValue> value = ReadFieldValue(member.value, *field, assets, fieldContext);
			if (!value.has_value()) { return false; }
			outComponent.fields.emplace_back(field->name, std::move(*value));
		}
		return true;
	}
}

bool WriteFieldValue(const ComponentFieldValue& value, AssetManager& assets, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& outJson, std::string_view context)
{
	const ComponentFieldList* list = std::get_if<ComponentFieldList>(&value);
	if (list == nullptr) { return WriteScalar(value, assets, allocator, outJson, context); }

	outJson.SetArray();
	for (const ComponentFieldScalar& element : list->elements)
	{
		rapidjson::Value elementJson;
		if (!WriteScalar(element, assets, allocator, elementJson, context)) { return false; }
		outJson.PushBack(elementJson, allocator);
	}
	return true;
}

std::optional<ComponentFieldValue> ReadFieldValue(const rapidjson::Value& json, const ComponentFieldInfo& field, AssetManager& assets, std::string_view context)
{
	if (!std::holds_alternative<ComponentFieldList>(field.valuePrototype))
	{
		std::optional<ComponentFieldValue> value = ReadScalar(json, field.valuePrototype, assets, context);
		if (!value.has_value())
		{
			LogReadFailure(context);
		}
		return value;
	}

	// 목록이면 원소마다 원소 견본으로 읽는다. 목록 속성은 등록할 때 원소 견본이 항상 채워진다.
	if (!json.IsArray())
	{
		LogReadFailure(context);
		return std::nullopt;
	}
	ComponentFieldList list;
	list.elements.reserve(json.Size());
	for (const rapidjson::Value& elementJson : json.GetArray())
	{
		std::optional<ComponentFieldScalar> element = ReadScalar(elementJson, *field.elementPrototype, assets, context);
		if (!element.has_value())
		{
			LogReadFailure(context);
			return std::nullopt;
		}
		list.elements.push_back(std::move(*element));
	}
	return ComponentFieldValue(std::move(list));
}

bool WriteGameObjects(const std::vector<GameObjectSnapshot>& objects, AssetManager& assets, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& outJson, std::string_view context)
{
	outJson.SetArray();
	for (const GameObjectSnapshot& object : objects)
	{
		rapidjson::Value objectJson(rapidjson::kObjectType);
		objectJson.AddMember("id", MakeString(object.objectID.ToString(), allocator), allocator);
		objectJson.AddMember("name", MakeString(object.name, allocator), allocator);
		// 자식이 없으면 적지 않아 파일을 짧게 한다.
		if (!object.childObjectIDs.empty())
		{
			rapidjson::Value children(rapidjson::kArrayType);
			for (const ObjectID& child : object.childObjectIDs)
			{
				children.PushBack(MakeString(child.ToString(), allocator), allocator);
			}
			objectJson.AddMember("children", children, allocator);
		}

		rapidjson::Value components(rapidjson::kArrayType);
		for (const ComponentSnapshot& component : object.components)
		{
			rapidjson::Value fields(rapidjson::kObjectType);
			for (const auto& [fieldName, value] : component.fields)
			{
				rapidjson::Value valueJson;
				const std::string fieldContext = std::format("{}: {}.{}.{}", context, object.name, component.typeName, fieldName);
				if (!WriteFieldValue(value, assets, allocator, valueJson, fieldContext)) { return false; }
				fields.AddMember(MakeString(fieldName, allocator), valueJson, allocator);
			}
			// 속성을 fields 안에 따로 두어, type이라는 이름의 속성이 생겨도 겹치지 않게 한다.
			rapidjson::Value componentJson(rapidjson::kObjectType);
			componentJson.AddMember("type", MakeString(component.typeName, allocator), allocator);
			componentJson.AddMember("fields", fields, allocator);
			components.PushBack(componentJson, allocator);
		}
		objectJson.AddMember("components", components, allocator);
		outJson.PushBack(objectJson, allocator);
	}
	return true;
}

std::optional<std::vector<GameObjectSnapshot>> ReadGameObjects(const rapidjson::Value& json, AssetManager& assets, std::string_view context)
{
	if (!json.IsArray())
	{
		LogFormatError(context, "objects must be an array");
		return std::nullopt;
	}

	std::vector<GameObjectSnapshot> objects;
	objects.reserve(json.Size());
	// 지금까지 읽은 객체 ID다. ID 중복을 찾는 데 쓴다.
	std::unordered_set<ObjectID> objectIDs;
	for (const rapidjson::Value& objectJson : json.GetArray())
	{
		const std::string objectContext = std::format("{}: objects[{}]", context, objects.size());
		GameObjectSnapshot& object = objects.emplace_back();
		// 객체 항목은 id, name, children, components만 받는다. 오타를 조용히 넘기지 않으려고 그 밖의 항목은 실패한다.
		if (!HasOnlyMembers(objectJson, { "id", "name", "children", "components" }))
		{
			LogFormatError(objectContext, "object must be an object with only id, name, children and components");
			return std::nullopt;
		}
		const auto id = objectJson.FindMember("id");
		if (id == objectJson.MemberEnd() || !ReadObjectID(id->value, object.objectID))
		{
			LogFormatError(objectContext, "id must be a valid ObjectID string");
			return std::nullopt;
		}
		// 같은 ID가 두 번이면 실패한다. 놓을 때 ID를 새로 뽑으면 두 객체가 다른 ID를 받아 등록 검사에 걸리지 않고,
		// 그 ID를 가리키던 자식과 참조가 조용히 첫 객체에 연결되기 때문이다.
		if (!objectIDs.insert(object.objectID).second)
		{
			LogFormatError(objectContext, std::format("id {} appears twice", object.objectID.ToString()));
			return std::nullopt;
		}
		if (!ReadStringMember(objectJson, "name", object.name))
		{
			LogFormatError(objectContext, "name must be a string");
			return std::nullopt;
		}

		// children은 생략할 수 있다. 생략하면 자식이 없다.
		const auto children = objectJson.FindMember("children");
		if (children != objectJson.MemberEnd())
		{
			if (!children->value.IsArray())
			{
				LogFormatError(objectContext, "children must be an array");
				return std::nullopt;
			}
			for (const rapidjson::Value& childJson : children->value.GetArray())
			{
				ObjectID childObjectID;
				if (!ReadObjectID(childJson, childObjectID))
				{
					LogFormatError(objectContext, "children must contain valid ObjectID strings");
					return std::nullopt;
				}
				object.childObjectIDs.push_back(childObjectID);
			}
		}

		const auto components = objectJson.FindMember("components");
		if (components == objectJson.MemberEnd() || !components->value.IsArray())
		{
			LogFormatError(objectContext, "components must be an array");
			return std::nullopt;
		}
		const std::string componentContext = std::format("{} {}", objectContext, object.name);
		for (const rapidjson::Value& componentJson : components->value.GetArray())
		{
			ComponentSnapshot component;
			if (!ReadComponent(componentJson, assets, componentContext, component)) { return std::nullopt; }
			// 한 객체에 같은 타입 컴포넌트가 둘이면 실패한다. Capture와 같은 규칙이며, 복원은 이미 붙은 같은 타입에 쓰므로 두 컴포넌트가 조용히 합쳐진다.
			const bool isDuplicateType = std::any_of(object.components.begin(), object.components.end(), [&component](const ComponentSnapshot& existing) { return existing.typeName == component.typeName; });
			if (isDuplicateType)
			{
				LogFormatError(componentContext, std::format("component {} appears twice", component.typeName));
				return std::nullopt;
			}
			object.components.push_back(std::move(component));
		}
	}
	return objects;
}
