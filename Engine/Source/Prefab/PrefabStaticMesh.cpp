#include "Prefab/PrefabComponents.h"

#include "AssetManager.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Log.h"
#include "GameObject.h"
#include "Renderer/Material.h"

#include <cstddef>
#include <format>
#include <memory>
#include <string_view>
#include <utility>

bool ReadStaticMeshProperties(const rapidjson::Value& properties, std::string_view context, PrefabStaticMeshDefinition& mesh)
{
	const auto model = properties.FindMember("model");
	const std::string_view error = model == properties.MemberEnd() ? "is required" : ValidatePrefabAssetPath(model->value);
	if (!error.empty())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}.model {}", context, error));
		return false;
	}

	const auto meshIndex = properties.FindMember("meshIndex");
	if (meshIndex == properties.MemberEnd() || !meshIndex->value.IsUint64() || !std::in_range<std::size_t>(meshIndex->value.GetUint64()))
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}.meshIndex must be a non-negative integer in range", context));
		return false;
	}

	mesh.modelPath.assign(model->value.GetString(), model->value.GetStringLength());
	mesh.meshIndex = static_cast<std::size_t>(meshIndex->value.GetUint64());

	const auto baseColorTexture = properties.FindMember("baseColorTexture");
	if (baseColorTexture != properties.MemberEnd())
	{
		const std::string_view textureError = ValidatePrefabAssetPath(baseColorTexture->value);
		if (!textureError.empty())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}.baseColorTexture {}", context, textureError));
			return false;
		}
		mesh.baseColorTexturePath.emplace(baseColorTexture->value.GetString(), baseColorTexture->value.GetStringLength());
	}

	return true;
}

bool LoadPrefabStaticMesh(AssetManager& assets, const PrefabStaticMeshDefinition& definition, std::string_view context, PrefabStaticMesh& outMesh)
{
	const std::shared_ptr<const Model> model = assets.LoadModel(definition.modelPath);
	if (model == nullptr)
	{
		Log::Write(LogLevel::Error, "AssetManager", std::format("{}: could not load staticMesh model {}", context, definition.modelPath));
		return false;
	}
	if (definition.meshIndex >= model->meshes.size() || definition.meshIndex >= model->meshMaterials.size())
	{
		Log::Write(LogLevel::Error, "AssetManager", std::format("{}: staticMesh meshIndex {} is out of range for {}", context, definition.meshIndex, definition.modelPath));
		return false;
	}

	std::shared_ptr<const Material> material = model->meshMaterials[definition.meshIndex];
	if (definition.baseColorTexturePath.has_value())
	{
		material = LoadMaterialWithBaseColorTexture(assets, *material, *definition.baseColorTexturePath);
		if (material == nullptr)
		{
			Log::Write(LogLevel::Error, "AssetManager", std::format("{}: staticMesh could not load baseColorTexture {}", context, *definition.baseColorTexturePath));
			return false;
		}
	}

	outMesh = PrefabStaticMesh{ model->meshes[definition.meshIndex], std::move(material) };
	return true;
}

void AttachPrefabStaticMesh(GameObject& object, const PrefabStaticMesh& mesh)
{
	std::unique_ptr<StaticMeshComponent> component = std::make_unique<StaticMeshComponent>();
	component->SetStaticMesh(mesh.mesh);
	component->SetMaterial(mesh.material);
	object.AddComponent(std::move(component));
}
