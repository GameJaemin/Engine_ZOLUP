#include "Prefab.h"

#include "Core/FileHelper.h"
#include "Core/Log.h"
#include "PrefabComponents.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

std::string_view ValidatePrefabAssetPath(const rapidjson::Value& value)
{
	if (!value.IsString()) { return "must be a string"; }

	const std::string_view pathText(value.GetString(), value.GetStringLength());
	if (pathText.empty() || pathText.find('\0') != std::string_view::npos)
	{
		return "must be non-empty and contain no NUL";
	}

	const std::filesystem::path path(std::u8string(pathText.begin(), pathText.end()));
	const std::filesystem::path normalizedPath = path.lexically_normal();

	const bool goesOutsideRoot = normalizedPath.begin() != normalizedPath.end() && *normalizedPath.begin() == "..";
	if (path.has_root_path() || goesOutsideRoot) { return "must be a repository-relative path"; }

	return {};
}

bool IsPrefabFiniteFloat(const rapidjson::Value& value)
{
	if (!value.IsNumber()) { return false; }

	// float로 변환하기 전에 double로 읽어 범위를 확인한다.
	constexpr double MaxFloat = (std::numeric_limits<float>::max)();
	const double number = value.GetDouble();
	return std::isfinite(number) && std::abs(number) <= MaxFloat;
}

namespace
{
	template <std::size_t Size>
	bool ReadFloatArray(const rapidjson::Value& fields, const char* name, std::string_view context, std::array<float, Size>& out)
	{
		const auto member = fields.FindMember(name);
		if (member != fields.MemberEnd() && member->value.IsArray() && member->value.Size() == Size)
		{
			std::size_t index = 0;
			for (const rapidjson::Value& entry : member->value.GetArray())
			{
				if (!IsPrefabFiniteFloat(entry)) { break; }
				out[index++] = entry.GetFloat();
			}
			if (index == Size) { return true; }
		}

		Log::Write(LogLevel::Error, "Prefab", std::format("{}.{} must contain {} finite floats", context, name, Size));
		return false;
	}

	bool ReadBoxColliderProperties(const rapidjson::Value& properties, std::string_view context, PrefabBoxColliderDefinition& out)
	{
		std::array<float, 3> centerCm;
		std::array<float, 3> halfExtentsCm;
		if (!ReadFloatArray(properties, "centerCm", context, centerCm)) { return false; }
		if (!ReadFloatArray(properties, "halfExtentsCm", context, halfExtentsCm)) { return false; }
		if (halfExtentsCm[0] <= 0.0f || halfExtentsCm[1] <= 0.0f || halfExtentsCm[2] <= 0.0f)
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}.halfExtentsCm must be positive on every axis", context));
			return false;
		}

		out.centerCm = { centerCm[0], centerCm[1], centerCm[2] };
		out.halfExtentsCm = { halfExtentsCm[0], halfExtentsCm[1], halfExtentsCm[2] };
		return true;
	}

	bool ReadRigidBodyProperties(const rapidjson::Value& properties, std::string_view context, PrefabRigidBodyDefinition& out)
	{
		const auto simulate = properties.FindMember("simulatePhysics");
		if (simulate == properties.MemberEnd()) { return true; }
		if (!simulate->value.IsBool())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}.simulatePhysics must be a boolean", context));
			return false;
		}

		out.simulatePhysics = simulate->value.GetBool();
		return true;
	}

	/** 한 객체에 같은 종류의 컴포넌트가 두 번 나오면 실패한다. 처음이면 slot에 initial을 넣은 뒤 read로 채운다. */
	template <typename Definition>
	bool ReadComponentOnce(std::optional<Definition>& slot, bool (*read)(const rapidjson::Value&, std::string_view, Definition&), std::string_view typeName, const rapidjson::Value& properties, const std::string& context, Definition initial = {})
	{
		if (slot.has_value())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: duplicate {} component", context, typeName));
			return false;
		}
		return read(properties, context + ".properties", slot.emplace(std::move(initial)));
	}

	bool ReadPrefabComponents(const rapidjson::Value& object, std::string_view objectContext, PrefabObjectDefinition& definition)
	{
		const auto components = object.FindMember("components");
		if (components == object.MemberEnd()) { return true; }
		if (!components->value.IsArray())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}.components must be an array", objectContext));
			return false;
		}

		const rapidjson::Value::ConstArray entries = components->value.GetArray();
		for (rapidjson::SizeType index = 0; index < entries.Size(); ++index)
		{
			const rapidjson::Value& entry = entries[index];
			const std::string context = std::format("{}.components[{}]", objectContext, index);
			if (!entry.IsObject())
			{
				Log::Write(LogLevel::Error, "Prefab", context + " must be an object");
				return false;
			}

			const auto type = entry.FindMember("type");
			if (type == entry.MemberEnd() || !type->value.IsString())
			{
				Log::Write(LogLevel::Error, "Prefab", context + ".type must be a string");
				return false;
			}

			const std::string_view typeName(type->value.GetString(), type->value.GetStringLength());
			const auto properties = entry.FindMember("properties");
			if (properties == entry.MemberEnd() || !properties->value.IsObject())
			{
				Log::Write(LogLevel::Error, "Prefab", context + ".properties must be an object");
				return false;
			}

			bool isRead = false;
			if (typeName == "skybox") { isRead = ReadComponentOnce(definition.skybox, &ReadSkyboxProperties, typeName, properties->value, context); }
			else if (typeName == "staticMesh") { isRead = ReadComponentOnce(definition.staticMesh, &ReadStaticMeshProperties, typeName, properties->value, context); }
			else if (typeName == "animation") { isRead = ReadComponentOnce(definition.animation, &ReadAnimationProperties, typeName, properties->value, context); }
			else if (typeName == "skin") { isRead = ReadComponentOnce(definition.skin, &ReadSkinProperties, typeName, properties->value, context); }
			else if (typeName == "camera") { isRead = ReadComponentOnce(definition.camera, &ReadCameraProperties, typeName, properties->value, context); }
			else if (typeName == "boxCollider") { isRead = ReadComponentOnce(definition.boxCollider, &ReadBoxColliderProperties, typeName, properties->value, context); }
			else if (typeName == "rigidBody") { isRead = ReadComponentOnce(definition.rigidBody, &ReadRigidBodyProperties, typeName, properties->value, context); }
			else if (const std::optional<PrefabLightType> lightType = FindPrefabLightType(typeName); lightType.has_value())
			{
				// 조명 4종은 한 칸을 쓴다. 중복 오류는 "duplicate light component"로 나온다.
				isRead = ReadComponentOnce(definition.light, &ReadLightProperties, "light", properties->value, context, PrefabLightDefinition{ .type = *lightType });
			}
			else
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}.type: unsupported component type {}", context, typeName));
				return false;
			}
			if (!isRead) { return false; }
		}

		// RigidBody는 같은 객체의 박스로 바디를 만들므로 콜라이더 없이는 동작하지 않는다.
		if (definition.rigidBody.has_value() && !definition.boxCollider.has_value())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}.components: rigidBody requires boxCollider", objectContext));
			return false;
		}

		return true;
	}

	bool ReadLocalTransform(const rapidjson::Value& fields, std::string_view context, Transform& out)
	{
		if (!fields.IsObject())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{} must be an object", context));
			return false;
		}

		std::array<float, 3> position;
		std::array<float, 4> rotation;
		std::array<float, 3> scale;
		if (!ReadFloatArray(fields, "position", context, position)) { return false; }
		if (!ReadFloatArray(fields, "rotation", context, rotation)) { return false; }
		if (!ReadFloatArray(fields, "scale", context, scale)) { return false; }

		// 맵과 같은 허용 오차를 써서 반올림된 단위 쿼터니언을 읽는다.
		constexpr double UnitLengthTolerance = 0.0001;
		double lengthSquared = 0.0;
		for (const double component : rotation)
		{
			lengthSquared += component * component;
		}
		if (std::abs(std::sqrt(lengthSquared) - 1.0) > UnitLengthTolerance)
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}.rotation must be a unit quaternion", context));
			return false;
		}

		out.position = { position[0], position[1], position[2] };
		out.rotation = { rotation[0], rotation[1], rotation[2], rotation[3] };
		out.scale = { scale[0], scale[1], scale[2] };
		return true;
	}

	bool ReadPrefabObject(const rapidjson::Value& object, std::string_view context, PrefabObjectDefinition& definition)
	{
		if (!object.IsObject())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{} must be an object", context));
			return false;
		}

		const auto name = object.FindMember("name");
		if (name != object.MemberEnd())
		{
			if (!name->value.IsString())
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}.name must be a string", context));
				return false;
			}
			definition.name.emplace(name->value.GetString(), name->value.GetStringLength());
		}

		const auto id = object.FindMember("id");
		if (id != object.MemberEnd())
		{
			if (!id->value.IsString() || id->value.GetStringLength() == 0)
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}.id must be a non-empty string", context));
				return false;
			}
			definition.id.emplace(id->value.GetString(), id->value.GetStringLength());
		}

		const auto transform = object.FindMember("transform");
		if (transform != object.MemberEnd() && !ReadLocalTransform(transform->value, std::format("{}.transform", context), definition.localTransform)) { return false; }
		if (!ReadPrefabComponents(object, context, definition)) { return false; }

		const auto member = object.FindMember("children");
		if (member == object.MemberEnd()) { return true; }
		if (!member->value.IsArray())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}.children must be an array", context));
			return false;
		}

		const auto entries = member->value.GetArray();
		definition.children.reserve(entries.Size());
		for (rapidjson::SizeType index = 0; index < entries.Size(); ++index)
		{
			const std::string childContext = std::format("{}.children[{}]", context, index);
			if (!ReadPrefabObject(entries[index], childContext, definition.children.emplace_back())) { return false; }
		}

		return true;
	}

	/** 루트부터 모든 객체의 id를 모은다. 같은 id가 두 번 나오면 false를 반환한다. */
	bool CollectObjectIDs(const PrefabObjectDefinition& object, std::string_view context, std::unordered_set<std::string>& ids)
	{
		if (object.id.has_value() && !ids.insert(*object.id).second)
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: duplicate object id {}", context, *object.id));
			return false;
		}

		for (const PrefabObjectDefinition& child : object.children)
		{
			if (!CollectObjectIDs(child, context, ids)) { return false; }
		}
		return true;
	}

	/** animation 컴포넌트의 연결 대상 id가 모두 같은 프리팹에 있는지 확인한다. */
	bool CheckBindingTargets(const PrefabObjectDefinition& object, std::string_view context, const std::unordered_set<std::string>& ids)
	{
		if (object.animation.has_value())
		{
			for (const PrefabNodeBinding& binding : object.animation->nodeBindings)
			{
				if (ids.contains(binding.targetID)) { continue; }

				Log::Write(LogLevel::Error, "Prefab", std::format("{}: animation target {} is not an object id in this prefab", context, binding.targetID));
				return false;
			}
		}

		for (const PrefabObjectDefinition& child : object.children)
		{
			if (!CheckBindingTargets(child, context, ids)) { return false; }
		}
		return true;
	}

	/** 객체 id가 프리팹 안에서 유일하고 animation 연결 대상이 모두 있는지 확인한다. */
	bool ValidateObjectIDs(const PrefabObjectDefinition& root, std::string_view context)
	{
		std::unordered_set<std::string> ids;
		if (!CollectObjectIDs(root, context, ids)) { return false; }
		return CheckBindingTargets(root, context, ids);
	}
}

bool ReadPrefabObjectDefinition(const rapidjson::Value& object, std::string_view context, PrefabObjectDefinition& outDefinition)
{
	PrefabObjectDefinition definition;
	if (!ReadPrefabObject(object, context, definition)) { return false; }
	if (!ValidateObjectIDs(definition, context)) { return false; }
	outDefinition = std::move(definition);
	return true;
}

bool ReadPrefabDefinition(const std::filesystem::path& absolutePath, PrefabDefinition& outDefinition)
{
	const std::u8string utf8Path = absolutePath.u8string();
	const std::string_view pathText(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

	std::vector<std::byte> bytes;
	if (!FileHelper::ReadAllBytes(absolutePath, bytes))
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: could not read prefab file", pathText));
		return false;
	}

	// RapidJSON이 원시 NUL 바이트를 입력 끝으로 처리할 수 있다.
	if (bytes.empty() || std::ranges::find(bytes, std::byte{0}) != bytes.end())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: file is empty or contains a raw NUL byte", pathText));
		return false;
	}

	rapidjson::Document document;
	document.Parse<rapidjson::kParseValidateEncodingFlag>(reinterpret_cast<const char*>(bytes.data()), bytes.size());

	if (document.HasParseError())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: invalid JSON at byte {}: {}", pathText, document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError())));
		return false;
	}

	if (!document.IsObject())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: JSON root must be an object", pathText));
		return false;
	}

	PrefabDefinition definition;
	const std::string rootContext = std::format("{}: root", pathText);
	const auto root = document.FindMember("root");
	if (root != document.MemberEnd())
	{
		if (!ReadPrefabObject(root->value, rootContext, definition.root)) { return false; }
		if (!ValidateObjectIDs(definition.root, pathText)) { return false; }
		outDefinition = std::move(definition);
		return true;
	}

	// 기존 파일은 최상위 컴포넌트와 자식을 루트 객체의 데이터로 읽는다.
	const auto model = document.FindMember("model");
	if (model != document.MemberEnd())
	{
		const std::string_view error = ValidatePrefabAssetPath(model->value);
		if (!error.empty())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: model {}", pathText, error));
			return false;
		}
		definition.modelPath.emplace(model->value.GetString(), model->value.GetStringLength());
	}

	const auto overrides = document.FindMember("materialOverrides");
	if (overrides != document.MemberEnd())
	{
		const rapidjson::Value& overrideArray = overrides->value;
		if (!overrideArray.IsArray())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: materialOverrides must be an array", pathText));
			return false;
		}

		const rapidjson::Value::ConstArray entries = overrideArray.GetArray();
		if (!entries.Empty() && !definition.modelPath.has_value())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: materialOverrides requires a model", pathText));
			return false;
		}

		definition.materialOverrides.reserve(entries.Size());
		for (rapidjson::SizeType i = 0; i < entries.Size(); ++i)
		{
			const rapidjson::Value& entry = entries[i];
			if (!entry.IsObject())
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}: materialOverrides[{}] must be an object", pathText, i));
				return false;
			}

			const auto meshIndex = entry.FindMember("meshIndex");
			if (meshIndex == entry.MemberEnd() || !meshIndex->value.IsUint64() || !std::in_range<std::size_t>(meshIndex->value.GetUint64()))
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}: materialOverrides[{}].meshIndex must be a non-negative integer in range", pathText, i));
				return false;
			}

			const auto baseColorTexture = entry.FindMember("baseColorTexture");
			const std::string_view error = baseColorTexture == entry.MemberEnd()
				? "is required"
				: ValidatePrefabAssetPath(baseColorTexture->value);
			if (!error.empty())
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}: materialOverrides[{}].baseColorTexture {}", pathText, i, error));
				return false;
			}

			definition.materialOverrides.push_back({
				.meshIndex = static_cast<std::size_t>(meshIndex->value.GetUint64()),
				.baseColorTexturePath = std::string(baseColorTexture->value.GetString(), baseColorTexture->value.GetStringLength())
			});
		}
	}

	if (!ReadPrefabObject(document, rootContext, definition.root)) { return false; }
	if (!ValidateObjectIDs(definition.root, pathText)) { return false; }

	outDefinition = std::move(definition);
	return true;
}
