#include "Prefab.h"

#include "Core/FileHelper.h"
#include "Core/Log.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	std::string_view ValidateAssetPath(const rapidjson::Value& value)
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

	bool ReadSkyboxProperties(const rapidjson::Value& properties, std::string_view context, PrefabSkyboxDefinition& skybox)
	{
		const auto enabled = properties.FindMember("enabled");
		if (enabled != properties.MemberEnd())
		{
			if (!enabled->value.IsBool())
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}: enabled must be a boolean", context));
				return false;
			}
			skybox.enabled = enabled->value.GetBool();
		}

		const auto faces = properties.FindMember("faces");
		if (faces == properties.MemberEnd() || !faces->value.IsObject())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: faces must be an object", context));
			return false;
		}

		constexpr std::array<const char*, 6> faceKeys{
			"positiveX", "negativeX", "positiveY", "negativeY", "positiveZ", "negativeZ"
		};

		for (std::size_t index = 0; index < faceKeys.size(); ++index)
		{
			const char* key = faceKeys[index];
			const auto face = faces->value.FindMember(key);
			if (face == faces->value.MemberEnd())
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}: faces.{} is required", context, key));
				return false;
			}

			const std::string_view error = ValidateAssetPath(face->value);
			if (!error.empty())
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}: faces.{} {}", context, key, error));
				return false;
			}

			skybox.facePaths[index].assign(face->value.GetString(), face->value.GetStringLength());
		}

		return true;
	}

	bool ReadPrefabComponents(const rapidjson::Value& document, std::string_view pathText, PrefabDefinition& definition)
	{
		const auto components = document.FindMember("components");
		if (components == document.MemberEnd()) { return true; }
		if (!components->value.IsArray())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: components must be an array", pathText));
			return false;
		}

		const rapidjson::Value::ConstArray entries = components->value.GetArray();
		for (rapidjson::SizeType index = 0; index < entries.Size(); ++index)
		{
			const rapidjson::Value& entry = entries[index];
			const std::string context = std::format("{}: components[{}]", pathText, index);
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
			if (typeName != "skybox")
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}.type: unsupported component type {}", context, typeName));
				return false;
			}

			if (definition.skybox.has_value())
			{
				Log::Write(LogLevel::Error, "Prefab", context + ": duplicate skybox component");
				return false;
			}

			const auto properties = entry.FindMember("properties");
			if (properties == entry.MemberEnd() || !properties->value.IsObject())
			{
				Log::Write(LogLevel::Error, "Prefab", context + ".properties must be an object");
				return false;
			}

			PrefabSkyboxDefinition skybox;
			if (!ReadSkyboxProperties(properties->value, context + ".properties", skybox)) { return false; }
			definition.skybox = std::move(skybox);
		}

		return true;
	}
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
	const auto model = document.FindMember("model");
	if (model != document.MemberEnd())
	{
		const std::string_view error = ValidateAssetPath(model->value);
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
				: ValidateAssetPath(baseColorTexture->value);
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

	if (!ReadPrefabComponents(document, pathText, definition)) { return false; }

	outDefinition = std::move(definition);
	return true;
}
