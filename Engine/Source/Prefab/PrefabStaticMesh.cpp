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

	const auto material = properties.FindMember("material");
	if (material != properties.MemberEnd())
	{
		const std::string_view materialError = ValidatePrefabAssetPath(material->value);
		if (!materialError.empty())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}.material {}", context, materialError));
			return false;
		}
		mesh.materialPath.emplace(material->value.GetString(), material->value.GetStringLength());
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
	if (definition.materialPath.has_value())
	{
		// 실패 이유는 LoadMaterial이 먼저 남긴다.
		material = assets.LoadMaterial(*definition.materialPath);
		if (material == nullptr)
		{
			Log::Write(LogLevel::Error, "AssetManager", std::format("{}: staticMesh could not load material {}", context, *definition.materialPath));
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
