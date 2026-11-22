#include "Prefab/PrefabComponents.h"

#include "AssetManager.h"
#include "Component/SkyboxComponent.h"
#include "Core/Log.h"
#include "GameObject.h"
#include "Texture/Texture.h"

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

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

		const std::string_view error = ValidatePrefabAssetPath(face->value);
		if (!error.empty())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: faces.{} {}", context, key, error));
			return false;
		}

		skybox.facePaths[index].assign(face->value.GetString(), face->value.GetStringLength());
	}

	return true;
}

bool LoadPrefabSkybox(AssetManager& assets, const PrefabSkyboxDefinition& definition, std::string_view context, PrefabSkybox& outSkybox)
{
	outSkybox.enabled = definition.enabled;
	for (std::size_t index = 0; index < outSkybox.faces.size(); ++index)
	{
		const std::string& facePath = definition.facePaths[index];
		outSkybox.faces[index] = assets.LoadTexture(facePath, TextureFormat::RGBA8SRGB);
		if (outSkybox.faces[index] == nullptr)
		{
			Log::Write(LogLevel::Error, "AssetManager", std::format("{}: could not load skybox face {}", context, facePath));
			return false;
		}
	}
	return true;
}

void AttachPrefabSkybox(GameObject& object, const PrefabSkybox& skybox)
{
	std::unique_ptr<SkyboxComponent> component = std::make_unique<SkyboxComponent>();
	component->SetFaces(skybox.faces);
	component->SetEnabled(skybox.enabled);
	object.AddComponent(std::move(component));
}
