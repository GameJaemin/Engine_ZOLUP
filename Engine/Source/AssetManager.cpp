#include "AssetManager.h"

#include "Core/Log.h"
#include "GLTFLoader.h"
#include "Image.h"
#include "ImageLoader.h"
#include "Mesh/StaticMesh.h"
#include "Paths.h"
#include "Prefab.h"
#include "Renderer/Material.h"
#include "Texture/Texture.h"

#include <cassert>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace
{
	/** 모델 폴더 기준 UTF-8 uri를 modelDirectory와 합쳐 저장소 루트 기준 UTF-8 상대 경로로 바꾼다. */
	std::string MakeModelImagePath(const std::filesystem::path& modelDirectory, const std::string& uri)
	{
		const std::u8string uriUtf8(uri.begin(), uri.end());
		const std::filesystem::path imagePath = (modelDirectory / std::filesystem::path(uriUtf8)).lexically_normal();
		const std::u8string imagePathUtf8 = imagePath.generic_u8string();
		return std::string(imagePathUtf8.begin(), imagePathUtf8.end());
	}

	std::shared_ptr<const Material> LoadMaterialWithBaseColorTexture(AssetManager& assets, const Material& baseMaterial, std::string_view texturePath)
	{
		std::shared_ptr<const Texture> texture = assets.LoadTexture(texturePath, TextureFormat::RGBA8SRGB);
		if (texture == nullptr) { return nullptr; }

		Material material = baseMaterial;
		material.baseColorTexture = std::move(texture);
		return std::make_shared<const Material>(std::move(material));
	}

	bool LoadPrefabObject(AssetManager& assets, const PrefabObjectDefinition& definition, std::string_view context, PrefabObject& object)
	{
		object.id = definition.id;
		object.name = definition.name;
		object.localTransform = definition.localTransform;

		if (definition.staticMesh.has_value())
		{
			const PrefabStaticMeshDefinition& mesh = *definition.staticMesh;
			const std::shared_ptr<const Model> model = assets.LoadModel(mesh.modelPath);
			if (model == nullptr)
			{
				Log::Write(LogLevel::Error, "AssetManager", std::format("{}: could not load staticMesh model {}", context, mesh.modelPath));
				return false;
			}
			if (mesh.meshIndex >= model->meshes.size() || mesh.meshIndex >= model->meshMaterials.size())
			{
				Log::Write(LogLevel::Error, "AssetManager", std::format("{}: staticMesh meshIndex {} is out of range for {}", context, mesh.meshIndex, mesh.modelPath));
				return false;
			}

			std::shared_ptr<const Material> material = model->meshMaterials[mesh.meshIndex];
			if (mesh.baseColorTexturePath.has_value())
			{
				material = LoadMaterialWithBaseColorTexture(assets, *material, *mesh.baseColorTexturePath);
				if (material == nullptr)
				{
					Log::Write(LogLevel::Error, "AssetManager", std::format("{}: staticMesh could not load baseColorTexture {}", context, *mesh.baseColorTexturePath));
					return false;
				}
			}

			object.staticMesh = PrefabStaticMesh{ model->meshes[mesh.meshIndex], std::move(material) };
		}

		if (definition.skybox.has_value())
		{
			const PrefabSkyboxDefinition& source = *definition.skybox;
			PrefabSkybox& skybox = object.skybox.emplace();
			skybox.enabled = source.enabled;
			for (std::size_t index = 0; index < skybox.faces.size(); ++index)
			{
				const std::string& facePath = source.facePaths[index];
				skybox.faces[index] = assets.LoadTexture(facePath, TextureFormat::RGBA8SRGB);
				if (skybox.faces[index] == nullptr)
				{
					Log::Write(LogLevel::Error, "AssetManager", std::format("{}: could not load skybox face {}", context, facePath));
					return false;
				}
			}
		}

		if (definition.animation.has_value())
		{
			const PrefabAnimationDefinition& animation = *definition.animation;
			std::shared_ptr<const Model> model = assets.LoadModel(animation.modelPath);
			if (model == nullptr)
			{
				Log::Write(LogLevel::Error, "AssetManager", std::format("{}: could not load animation model {}", context, animation.modelPath));
				return false;
			}

			// 노드 번호는 모델 배치표의 줄 번호라 모델을 읽은 뒤에 범위를 확인한다.
			for (const PrefabNodeBinding& binding : animation.nodeBindings)
			{
				if (binding.sourceNode < model->sceneNodes.size()) { continue; }

				Log::Write(LogLevel::Error, "AssetManager", std::format("{}: animation sourceNode {} is out of range for {}", context, binding.sourceNode, animation.modelPath));
				return false;
			}

			object.animation = PrefabAnimation{ std::move(model), animation.nodeBindings };
		}

		if (definition.skin.has_value())
		{
			const PrefabSkinDefinition& skin = *definition.skin;
			std::shared_ptr<const Model> model = assets.LoadModel(skin.modelPath);
			if (model == nullptr)
			{
				Log::Write(LogLevel::Error, "AssetManager", std::format("{}: could not load skin model {}", context, skin.modelPath));
				return false;
			}

			// SkinComponent는 이 노드의 skin 번호로 스킨을 찾으므로, 범위 안이고 스킨이 붙은 노드여야 한다.
			if (skin.sourceNode >= model->sceneNodes.size() || !model->sceneNodes[skin.sourceNode].skin.has_value())
			{
				Log::Write(LogLevel::Error, "AssetManager", std::format("{}: skin sourceNode {} is not a skinned node in {}", context, skin.sourceNode, skin.modelPath));
				return false;
			}

			object.skin = PrefabSkin{ std::move(model), skin.sourceNode };
		}

		object.children.reserve(definition.children.size());
		for (std::size_t index = 0; index < definition.children.size(); ++index)
		{
			const std::string childContext = std::format("{}.children[{}]", context, index);
			if (!LoadPrefabObject(assets, definition.children[index], childContext, object.children.emplace_back())) { return false; }
		}

		return true;
	}
}

AssetManager* AssetManager::instance = nullptr;

AssetManager::AssetManager()
{
	assert(instance == nullptr && "AssetManager는 한 번에 하나만 존재해야 한다.");
	instance = this;
}

AssetManager::~AssetManager()
{
	if (instance == this) { instance = nullptr; }
}

AssetManager& AssetManager::Get()
{
	assert(instance != nullptr && "AssetManager가 없을 때 Get을 호출했다.");
	return *instance;
}

std::shared_ptr<const Model> AssetManager::LoadModel(std::string_view relativePath)
{
	std::filesystem::path absolutePath;
	if (!Paths::GetAbsolutePath(relativePath, absolutePath))
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to resolve model path: " + std::string(relativePath));
		return nullptr;
	}

	absolutePath = absolutePath.lexically_normal();
	absolutePath.make_preferred();

	const std::u8string absolutePathUtf8 = absolutePath.u8string();
	const std::string pathText(absolutePathUtf8.begin(), absolutePathUtf8.end());
	const std::string cacheKey = Paths::MakeAssetKey(pathText);

	const auto cachedModel = models.find(cacheKey);
	if (cachedModel != models.end()) { return cachedModel->second; }

	GLTFModel gltfModel;
	if (!GLTFLoader::LoadModel(absolutePath, gltfModel))
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to load model: " + std::string(relativePath));
		return nullptr;
	}

	// UTF-8 경로를 정규화하고 구분자를 /로 통일해 메시 경로의 앞부분으로 쓴다.
	const std::filesystem::path assetPath = std::filesystem::path(std::u8string(relativePath.begin(), relativePath.end())).lexically_normal();
	const std::u8string assetPathUtf8 = assetPath.generic_u8string();
	const std::string modelPath(assetPathUtf8.begin(), assetPathUtf8.end());

	Model model;
	model.meshes.reserve(gltfModel.meshes.size());
	for (std::size_t i = 0; i < gltfModel.meshes.size(); ++i)
	{
		StaticMesh& source = gltfModel.meshes[i];

		// 경로는 만든 뒤 바꿀 수 없으므로, 이름과 "파일경로#번호" 경로로 메시를 새로 만든다.
		StaticMesh meshData(source.GetName(), modelPath + "#" + std::to_string(i));

		// 정점, 인덱스와 정점별 뼈 가중치는 복사하지 않고 옮겨 담는다.
		meshData.vertices = std::move(source.vertices);
		meshData.indices = std::move(source.indices);
		meshData.skinWeights = std::move(source.skinWeights);

		model.meshes.push_back(std::make_shared<const StaticMesh>(std::move(meshData)));
	}

	// glTF 재질마다 Material을 만든다. 텍스처 경로의 기준은 모델 파일이 있는 폴더다.
	const std::filesystem::path modelDirectory = assetPath.parent_path();
	std::vector<std::shared_ptr<const Material>> materials;
	materials.reserve(gltfModel.materials.size());
	for (const GLTFModelMaterial& source : gltfModel.materials)
	{
		std::shared_ptr<const Material> material = CreateMaterial(source, modelDirectory);
		if (material == nullptr)
		{
			Log::Write(LogLevel::Error, "AssetManager", "Failed to create material for model: " + std::string(relativePath));
			return nullptr;
		}

		materials.push_back(std::move(material));
	}

	// 메시마다 재질을 정한다. 재질 번호는 로더가 범위를 확인했다.
	// 재질이 없는 메시는 glTF 기본 재질을 쓰며, Material의 기본값(흰색, 금속성 1, 거칠기 1, 텍스처 없음)이 이와 같다.
	// 기본 재질은 필요할 때 한 번만 만들어 공유한다.
	std::shared_ptr<const Material> defaultMaterial;
	model.meshMaterials.reserve(gltfModel.meshMaterials.size());
	for (const std::optional<std::size_t>& materialIndex : gltfModel.meshMaterials)
	{
		if (materialIndex.has_value())
		{
			model.meshMaterials.push_back(materials[materialIndex.value()]);
			continue;
		}

		if (defaultMaterial == nullptr) { defaultMaterial = std::make_shared<const Material>(); }
		model.meshMaterials.push_back(defaultMaterial);
	}

	model.animations.reserve(gltfModel.animations.size());
	for (std::size_t i = 0; i < gltfModel.animations.size(); ++i)
	{
		AnimationClip& source = gltfModel.animations[i];

		// 경로는 만든 뒤 바꿀 수 없으므로, 이름과 "파일경로#번호" 경로로 클립을 새로 만든다.
		AnimationClip clip(source.GetName(), modelPath + "#" + std::to_string(i));

		// 트랙은 복사하지 않고 옮겨 담는다.
		clip.tracks = std::move(source.tracks);
		clip.duration = source.duration;

		model.animations.push_back(std::make_shared<const AnimationClip>(std::move(clip)));
	}

	// 스킨은 경로로 찾지 않으므로 메시나 클립과 달리 경로를 붙이지 않고 그대로 옮겨 담는다.
	model.skins.reserve(gltfModel.skins.size());
	for (Skin& source : gltfModel.skins)
	{
		model.skins.push_back(std::make_shared<const Skin>(std::move(source)));
	}

	model.sceneNodes = std::move(gltfModel.sceneNodes);

	std::shared_ptr<const Model> result = std::make_shared<const Model>(std::move(model));
	models.emplace(cacheKey, result);
	return result;
}

std::shared_ptr<const Material> AssetManager::CreateMaterial(const GLTFModelMaterial& source, const std::filesystem::path& modelDirectory)
{
	// 계수는 로더가 범위를 확인했고 의미가 같으므로 그대로 옮긴다.
	Material material;
	material.baseColor = source.baseColor;
	material.alphaMode = source.alphaMode;
	material.baseColorAlpha = source.baseColorAlpha;
	material.roughness = source.roughness;
	material.metallic = source.metallic;

	if (!source.baseColorImageUri.empty())
	{
		// glTF 색 텍스처의 RGB는 sRGB 값이다. 같은 이미지는 LoadTexture가 캐시해 공유한다.
		material.baseColorTexture = LoadTexture(MakeModelImagePath(modelDirectory, source.baseColorImageUri), TextureFormat::RGBA8SRGB);
		if (material.baseColorTexture == nullptr) { return nullptr; }
	}

	if (!source.normalImageUri.empty())
	{
		// 노멀 텍스처의 RGB는 색이 아니라 방향 값이라 sRGB 변환 없이 선형으로 읽는다.
		material.normalTexture = LoadTexture(MakeModelImagePath(modelDirectory, source.normalImageUri), TextureFormat::RGBA8UNorm);
		if (material.normalTexture == nullptr) { return nullptr; }
	}

	if (!source.metallicRoughnessImageUri.empty())
	{
		// glTF 금속성-거칠기 텍스처의 G, B는 엔진 ORM의 거칠기, 금속성 자리와 같아 그대로 ORM으로 쓴다.
		// 차폐가 따로 묶여 있지 않으면 R은 파일에 든 값이 차폐로 읽힌다.
		material.ormTexture = LoadTexture(MakeModelImagePath(modelDirectory, source.metallicRoughnessImageUri), TextureFormat::RGBA8UNorm);
		if (material.ormTexture == nullptr) { return nullptr; }
	}

	if (!source.occlusionImageUri.empty())
	{
		// 엔진은 ORM 한 장만 받으므로 차폐는 금속성-거칠기 텍스처와 같은 이미지(R 채널)일 때만 쓰인다.
		// uri 표기가 달라도(./orm.png와 orm.png) 같은 파일로 보도록 정리한 경로끼리 비교한다.
		const bool isPackedWithMetallicRoughness = !source.metallicRoughnessImageUri.empty()
			&& MakeModelImagePath(modelDirectory, source.occlusionImageUri) == MakeModelImagePath(modelDirectory, source.metallicRoughnessImageUri);
		if (!isPackedWithMetallicRoughness)
		{
			Log::Write(LogLevel::Warning, "AssetManager", std::format("Material '{}': occlusion texture is not packed with the metallic-roughness texture; ignored", source.name));
		}
	}

	return std::make_shared<const Material>(std::move(material));
}

std::shared_ptr<const Texture> AssetManager::LoadTexture(std::string_view relativePath, TextureFormat format)
{
	const bool isSupportedFormat = format == TextureFormat::RGBA8UNorm || format == TextureFormat::RGBA8SRGB;
	if (!isSupportedFormat)
	{
		Log::Write(LogLevel::Error, "AssetManager", "Unsupported texture format.");
		return nullptr;
	}

	std::filesystem::path absolutePath;
	if (!Paths::GetAbsolutePath(relativePath, absolutePath))
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to resolve texture path: " + std::string(relativePath));
		return nullptr;
	}

	absolutePath = absolutePath.lexically_normal();
	absolutePath.make_preferred();

	const std::u8string absolutePathUtf8 = absolutePath.u8string();
	const std::string pathText(absolutePathUtf8.begin(), absolutePathUtf8.end());
	const std::pair<std::string, TextureFormat> cacheKey(Paths::MakeAssetKey(pathText), format);

	const auto cachedTexture = textures.find(cacheKey);
	if (cachedTexture != textures.end()) { return cachedTexture->second; }

	Image image;
	if (!ImageLoader::LoadImageFile(absolutePath, image)) { return nullptr; }

	const std::u8string relativePathUtf8(relativePath.begin(), relativePath.end());
	const std::filesystem::path requestedPath(relativePathUtf8);
	const std::filesystem::path assetPath = requestedPath.lexically_normal();
	const std::u8string assetNameUtf8 = assetPath.stem().u8string();
	const std::u8string assetPathUtf8 = assetPath.generic_u8string();

	const std::string assetName(assetNameUtf8.begin(), assetNameUtf8.end());
	const std::string assetRelativePath(assetPathUtf8.begin(), assetPathUtf8.end());

	Texture textureData(assetName, assetRelativePath);
	if (!textureData.Initialize(std::move(image), format))
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to initialize texture: " + std::string(relativePath));
		return nullptr;
	}

	std::shared_ptr<const Texture> texture;
	texture = std::make_shared<const Texture>(std::move(textureData));

	textures.emplace(cacheKey, texture);
	return texture;
}

std::shared_ptr<const Prefab> AssetManager::LoadPrefab(std::string_view relativePath)
{
	std::filesystem::path absolutePath;
	if (!Paths::GetAbsolutePath(relativePath, absolutePath))
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to resolve prefab path: " + std::string(relativePath));
		return nullptr;
	}

	const std::u8string absolutePathUtf8 = absolutePath.u8string();
	const std::string pathText(absolutePathUtf8.begin(), absolutePathUtf8.end());
	const std::string cacheKey = Paths::MakeAssetKey(pathText);

	const auto cachedPrefab = prefabs.find(cacheKey);
	if (cachedPrefab != prefabs.end()) { return cachedPrefab->second; }

	PrefabDefinition definition;
	if (!ReadPrefabDefinition(absolutePath, definition)) { return nullptr; }

	Prefab prefab{};
	if (definition.modelPath.has_value())
	{
		prefab.model = LoadModel(*definition.modelPath);
		if (prefab.model == nullptr)
		{
			Log::Write(LogLevel::Error, "AssetManager", std::format("{}: could not load model {}", relativePath, *definition.modelPath));

			return nullptr;
		}
	}

	// 파서는 재질 오버라이드가 있을 때 모델 경로가 있는지 확인한다.
	if (!definition.materialOverrides.empty())
	{
		const auto& defaultMaterials = prefab.model->meshMaterials;
		prefab.meshMaterialOverrides.resize(defaultMaterials.size());

		for (const PrefabMaterialOverride& entry : definition.materialOverrides)
		{
			const std::size_t meshIndex = entry.meshIndex;
			if (meshIndex >= defaultMaterials.size() || prefab.meshMaterialOverrides[meshIndex] != nullptr)
			{
				Log::Write(LogLevel::Error, "AssetManager", std::format("{}: invalid or duplicate material override for mesh {}", relativePath, meshIndex));
				return nullptr;
			}

			std::shared_ptr<const Material> material = LoadMaterialWithBaseColorTexture(*this, *defaultMaterials[meshIndex], entry.baseColorTexturePath);
			if (material == nullptr)
			{
				Log::Write(LogLevel::Error, "AssetManager", std::format("{}: mesh {} could not load texture {}", relativePath, meshIndex, entry.baseColorTexturePath));
				return nullptr;
			}

			prefab.meshMaterialOverrides[meshIndex] = std::move(material);
		}
	}

	if (!LoadPrefabObject(*this, definition.root, std::format("{}: root", relativePath), prefab.root)) { return nullptr; }

	std::shared_ptr<const Prefab> result = std::make_shared<const Prefab>(std::move(prefab));
	prefabs.emplace(cacheKey, result);

	return result;
}
