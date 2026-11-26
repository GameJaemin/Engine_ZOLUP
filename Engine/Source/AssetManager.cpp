#include "AssetManager.h"

#include "Core/Log.h"
#include "GLTF/GLTFLoader.h"
#include "Image/Image.h"
#include "Image/ImageLoader.h"
#include "Mesh/StaticMesh.h"
#include "Paths.h"
#include "Prefab/Prefab.h"
#include "Prefab/PrefabComponents.h"
#include "Renderer/Material.h"
#include "Texture/ORMImageBuilder.h"
#include "Texture/Texture.h"

#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
	// 생성 식별자가 경로 정규화를 거쳐도 원본 키의 구분자와 문자가 해석되지 않게 한다.
	std::string EncodeGeneratedPathKey(std::string_view key)
	{
		constexpr char HexDigits[] = "0123456789abcdef";
		std::string encoded;
		for (const char character : key)
		{
			const unsigned char value = static_cast<unsigned char>(character);
			encoded += HexDigits[value >> 4];
			encoded += HexDigits[value & 15];
		}
		return encoded;
	}

	/** 8비트 sRGB 값마다 선형 계수를 곱한 뒤 다시 sRGB로 저장할 바이트를 계산한다. */
	std::array<std::byte, 256> MakeEmissiveChannelTable(float factor)
	{
		std::array<std::byte, 256> table{};
		for (std::size_t index = 0; index < table.size(); ++index)
		{
			const double srgb = static_cast<double>(index) / 255.0;
			const double linear = srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
			const double scaled = linear * factor;
			const double encoded = scaled <= 0.0031308 ? scaled * 12.92 : 1.055 * std::pow(scaled, 1.0 / 2.4) - 0.055;
			table[index] = static_cast<std::byte>(std::lround(encoded * 255.0));
		}
		return table;
	}

	/** 모델 폴더 기준 UTF-8 uri를 modelDirectory와 합쳐 저장소 루트 기준 UTF-8 상대 경로로 바꾼다. */
	std::string MakeModelImagePath(const std::filesystem::path& modelDirectory, const std::string& uri)
	{
		const std::u8string uriUtf8(uri.begin(), uri.end());
		const std::filesystem::path imagePath = (modelDirectory / std::filesystem::path(uriUtf8)).lexically_normal();
		const std::u8string imagePathUtf8 = imagePath.generic_u8string();
		return std::string(imagePathUtf8.begin(), imagePathUtf8.end());
	}

	/** 파일 안 에셋 경로를 마지막 '#'로 파일 경로와 표시로 나눈다. 파일 이름에 '#'가 있어도 된다. '#'가 없으면 nullopt를 반환한다. */
	std::optional<std::pair<std::string_view, std::string_view>> SplitSubAssetPath(std::string_view path)
	{
		const std::size_t separator = path.rfind('#');
		if (separator == std::string_view::npos) { return std::nullopt; }
		return std::pair{ path.substr(0, separator), path.substr(separator + 1) };
	}

	/** 표시 전체가 십진 숫자면 그 값을, 비었거나 숫자 뒤에 다른 글자가 있으면 nullopt를 반환한다. */
	std::optional<std::size_t> ParseSubAssetIndex(std::string_view label)
	{
		std::size_t index = 0;
		const auto [end, error] = std::from_chars(label.data(), label.data() + label.size(), index);
		if (error != std::errc() || end != label.data() + label.size()) { return std::nullopt; }
		return index;
	}

	// 저장소 루트 기준 경로를 정리한 절대 경로와 캐시 키다.
	struct ResolvedAssetPath
	{
		std::filesystem::path absolutePath;
		std::string cacheKey;
	};

	/** 저장소 루트 기준 UTF-8 경로를 정리한 절대 경로와 캐시 키로 바꾼다. 파일은 읽지 않는다. 루트 폴더를 찾지 못하면 nullopt를 반환한다. */
	std::optional<ResolvedAssetPath> ResolveAssetPath(std::string_view relativePath)
	{
		std::filesystem::path absolutePath;
		if (!Paths::GetAbsolutePath(relativePath, absolutePath)) { return std::nullopt; }

		absolutePath = absolutePath.lexically_normal();
		absolutePath.make_preferred();

		const std::u8string absolutePathUtf8 = absolutePath.u8string();
		std::string cacheKey = Paths::MakeAssetKey(std::string(absolutePathUtf8.begin(), absolutePathUtf8.end()));
		return ResolvedAssetPath{ std::move(absolutePath), std::move(cacheKey) };
	}

	/** model에서 '#' 뒤 표시가 가리키는 재질을 고른다. "default"는 기본 재질, 십진 숫자는 그 번호의 재질이다. 없으면 로그 없이 nullptr를 반환한다. */
	std::shared_ptr<const Material> SelectModelMaterial(const Model& model, std::string_view label)
	{
		if (label == "default") { return model.defaultMaterial; }

		const std::optional<std::size_t> index = ParseSubAssetIndex(label);
		if (!index.has_value() || *index >= model.materials.size()) { return nullptr; }
		return model.materials[*index];
	}
}

bool LoadPrefabObject(AssetManager& assets, const PrefabObjectDefinition& definition, std::string_view context, PrefabObject& object)
{
	object.id = definition.id;
	object.name = definition.name;
	object.localTransform = definition.localTransform;

	if (definition.staticMesh.has_value() && !LoadPrefabStaticMesh(assets, *definition.staticMesh, context, object.staticMesh.emplace())) { return false; }

	if (definition.skybox.has_value() && !LoadPrefabSkybox(assets, *definition.skybox, context, object.skybox.emplace())) { return false; }

	if (definition.animation.has_value() && !LoadPrefabAnimation(assets, *definition.animation, context, object.animation.emplace())) { return false; }

	if (definition.skin.has_value() && !LoadPrefabSkin(assets, *definition.skin, context, object.skin.emplace())) { return false; }

	object.light = definition.light;
	object.camera = definition.camera;
	object.boxCollider = definition.boxCollider;
	object.rigidBody = definition.rigidBody;

	object.children.reserve(definition.children.size());
	for (std::size_t index = 0; index < definition.children.size(); ++index)
	{
		const std::string childContext = std::format("{}.children[{}]", context, index);
		if (!LoadPrefabObject(assets, definition.children[index], childContext, object.children.emplace_back())) { return false; }
	}

	return true;
}

Model::Model(std::string newName, std::string newPath)
	: Asset(std::move(newName), std::move(newPath))
{
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
	const std::optional<ResolvedAssetPath> resolved = ResolveAssetPath(relativePath);
	if (!resolved.has_value())
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to resolve model path: " + std::string(relativePath));
		return nullptr;
	}

	const auto cachedModel = models.find(resolved->cacheKey);
	if (cachedModel != models.end()) { return cachedModel->second; }

	GLTFModel gltfModel;
	if (!GLTFLoader::LoadModel(resolved->absolutePath, gltfModel))
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to load model: " + std::string(relativePath));
		return nullptr;
	}

	// UTF-8 경로를 정규화하고 구분자를 /로 통일해 메시 경로의 앞부분으로 쓴다.
	const std::filesystem::path assetPath = std::filesystem::path(std::u8string(relativePath.begin(), relativePath.end())).lexically_normal();
	const std::u8string assetPathUtf8 = assetPath.generic_u8string();
	const std::string modelPath(assetPathUtf8.begin(), assetPathUtf8.end());

	const std::u8string modelNameUtf8 = assetPath.stem().u8string();
	Model model(std::string(modelNameUtf8.begin(), modelNameUtf8.end()), modelPath);
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

	// glTF 재질마다 "파일경로#번호" 경로의 Material을 만든다. 텍스처 경로의 기준은 모델 파일이 있는 폴더다.
	const std::filesystem::path modelDirectory = assetPath.parent_path();
	model.materials.reserve(gltfModel.materials.size());
	for (std::size_t i = 0; i < gltfModel.materials.size(); ++i)
	{
		std::shared_ptr<const Material> material = CreateMaterial(gltfModel.materials[i], modelDirectory, modelPath + "#" + std::to_string(i));
		if (material == nullptr)
		{
			Log::Write(LogLevel::Error, "AssetManager", "Failed to create material for model: " + std::string(relativePath));
			return nullptr;
		}

		model.materials.push_back(std::move(material));
	}

	// 메시마다 재질을 정한다. 재질 번호는 로더가 범위를 확인했다.
	// 재질이 없는 메시는 glTF 기본 재질을 쓰며, MaterialParameters의 기본값(흰색, 금속성 1, 거칠기 1, 텍스처 없음)이 이와 같다.
	// 기본 재질은 필요할 때 한 번만 "파일경로#default" 경로로 만들어 공유한다.
	model.meshMaterials.reserve(gltfModel.meshMaterials.size());
	for (const std::optional<std::size_t>& materialIndex : gltfModel.meshMaterials)
	{
		if (materialIndex.has_value())
		{
			model.meshMaterials.push_back(model.materials[materialIndex.value()]);
			continue;
		}

		if (model.defaultMaterial == nullptr) { model.defaultMaterial = std::make_shared<const Material>("DefaultMaterial", modelPath + "#default", MaterialParameters{}); }
		model.meshMaterials.push_back(model.defaultMaterial);
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
	models.emplace(resolved->cacheKey, result);
	return result;
}

std::shared_ptr<const Material> AssetManager::FindLoadedMaterial(std::string_view path) const
{
	const std::optional<std::pair<std::string_view, std::string_view>> parts = SplitSubAssetPath(path);
	if (!parts.has_value()) { return nullptr; }

	const std::optional<ResolvedAssetPath> resolved = ResolveAssetPath(parts->first);
	if (!resolved.has_value()) { return nullptr; }

	const auto cachedModel = models.find(resolved->cacheKey);
	if (cachedModel == models.end()) { return nullptr; }

	return SelectModelMaterial(*cachedModel->second, parts->second);
}

std::shared_ptr<const Material> AssetManager::LoadMaterialWithBaseColorTexture(const Material& baseMaterial, std::string_view texturePath)
{
	std::shared_ptr<const Texture> texture = LoadTexture(texturePath, TextureFormat::RGBA8SRGB);
	if (texture == nullptr) { return nullptr; }

	// 기본 재질이 그 경로로 찾은 모델의 재질일 때만 경로를 만들고 캐시한다.
	// 경로만 같은 다른 객체의 값으로 캐시하면 이후 같은 경로의 요청이 그 값을 받으므로, 이미 불러온 재질과 같은 객체인지 확인한다.
	// 코드에서 만든 재질(빈 경로), 이미 교체한 재질('|' 포함), 경로만 같은 다른 객체는 경로 없이 매번 새로 만들어 저장할 수 없는 재질로 둔다.
	// 텍스처 경로는 요청 문자열 대신 불러온 Texture의 경로를 써서 같은 파일이면 같은 문자열이 되게 한다.
	const std::string& basePath = baseMaterial.GetPath();
	const bool canIdentify = !basePath.empty() && basePath.find('|') == std::string::npos && FindLoadedMaterial(basePath).get() == &baseMaterial;
	std::string path;
	if (canIdentify)
	{
		path = basePath + "|baseColorTexture=" + texture->GetPath();
		const auto cached = baseColorTextureMaterials.find(path);
		if (cached != baseColorTextureMaterials.end()) { return cached->second; }
	}

	// 값만 복사하고 정체성은 새로 정한다.
	MaterialParameters parameters = baseMaterial.parameters;
	parameters.baseColorTexture = std::move(texture);
	std::shared_ptr<const Material> material = std::make_shared<const Material>(baseMaterial.GetName(), path, std::move(parameters));
	if (canIdentify) { baseColorTextureMaterials.emplace(std::move(path), material); }
	return material;
}

std::shared_ptr<const Material> AssetManager::CreateMaterial(const GLTFModelMaterial& source, const std::filesystem::path& modelDirectory, std::string path)
{
	// 계수는 로더가 범위를 확인했고 의미가 같으므로 그대로 옮긴다.
	MaterialParameters parameters;
	parameters.baseColor = source.baseColor;
	parameters.alphaMode = source.alphaMode;
	parameters.baseColorAlpha = source.baseColorAlpha;
	parameters.roughness = source.roughness;
	parameters.metallic = source.metallic;

	if (!source.baseColorImageUri.empty())
	{
		// glTF 색 텍스처의 RGB는 sRGB 값이다. 같은 이미지는 LoadTexture가 캐시해 공유한다.
		parameters.baseColorTexture = LoadTexture(MakeModelImagePath(modelDirectory, source.baseColorImageUri), TextureFormat::RGBA8SRGB);
		if (parameters.baseColorTexture == nullptr) { return nullptr; }
	}

	if (!source.normalImageUri.empty())
	{
		// 노멀 텍스처의 RGB는 색이 아니라 방향 값이라 sRGB 변환 없이 선형으로 읽는다.
		parameters.normalTexture = LoadTexture(MakeModelImagePath(modelDirectory, source.normalImageUri), TextureFormat::RGBA8UNorm);
		if (parameters.normalTexture == nullptr) { return nullptr; }
	}

	if (!source.occlusionImageUri.empty() || !source.metallicRoughnessImageUri.empty())
	{
		std::string occlusionPath;
		std::string metallicRoughnessPath;

		if (!source.occlusionImageUri.empty())
		{
			occlusionPath = MakeModelImagePath(modelDirectory, source.occlusionImageUri);
		}

		if (!source.metallicRoughnessImageUri.empty())
		{
			metallicRoughnessPath = MakeModelImagePath(modelDirectory, source.metallicRoughnessImageUri);
		}

		parameters.ormTexture = LoadORMTexture(occlusionPath, metallicRoughnessPath);
		if (parameters.ormTexture == nullptr) { return nullptr; }
	}

	const Vector3& factor = source.emissiveFactor;
	if (factor.x > 0.0f || factor.y > 0.0f || factor.z > 0.0f)
	{
		std::string texturePath;
		if (!source.emissiveImageUri.empty())
		{
			texturePath = MakeModelImagePath(modelDirectory, source.emissiveImageUri);
		}

		parameters.emissiveTexture = LoadEmissiveTexture(texturePath, factor);
		if (parameters.emissiveTexture == nullptr)
		{
			Log::Write(LogLevel::Warning, "AssetManager", std::format("Material '{}': could not prepare emissive texture", source.name));
		}
	}

	return std::make_shared<const Material>(source.name, std::move(path), std::move(parameters));
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

std::shared_ptr<const Texture> AssetManager::LoadORMTexture(std::string_view occlusionPath, std::string_view metallicRoughnessPath)
{
	if (occlusionPath.empty() && metallicRoughnessPath.empty()) { return nullptr; }

	std::filesystem::path occlusionAbsolutePath;
	std::filesystem::path metallicRoughnessAbsolutePath;
	std::pair<std::string, std::string> cacheKey;
	if (!occlusionPath.empty())
	{
		if (!Paths::GetAbsolutePath(occlusionPath, occlusionAbsolutePath))
		{
			Log::Write(LogLevel::Error, "AssetManager", "Failed to resolve ORM occlusion path: " + std::string(occlusionPath));
			return nullptr;
		}
		occlusionAbsolutePath = occlusionAbsolutePath.lexically_normal();
		const std::u8string pathUtf8 = occlusionAbsolutePath.generic_u8string();
		cacheKey.first = Paths::MakeAssetKey(std::string(pathUtf8.begin(), pathUtf8.end()));
	}
	if (!metallicRoughnessPath.empty())
	{
		if (!Paths::GetAbsolutePath(metallicRoughnessPath, metallicRoughnessAbsolutePath))
		{
			Log::Write(LogLevel::Error, "AssetManager", "Failed to resolve ORM metallic-roughness path: " + std::string(metallicRoughnessPath));
			return nullptr;
		}
		metallicRoughnessAbsolutePath = metallicRoughnessAbsolutePath.lexically_normal();
		const std::u8string pathUtf8 = metallicRoughnessAbsolutePath.generic_u8string();
		cacheKey.second = Paths::MakeAssetKey(std::string(pathUtf8.begin(), pathUtf8.end()));
	}

	const auto cachedTexture = ormTextures.find(cacheKey);
	if (cachedTexture != ormTextures.end()) { return cachedTexture->second; }

	const bool isSameImage = !cacheKey.first.empty() && cacheKey.first == cacheKey.second;

	Image occlusion;
	Image metallicRoughness;
	if (!occlusionPath.empty() && !ImageLoader::LoadImageFile(occlusionAbsolutePath, occlusion)) { return nullptr; }
	if (!metallicRoughnessPath.empty() && !isSameImage)
	{
		if (!ImageLoader::LoadImageFile(metallicRoughnessAbsolutePath, metallicRoughness)) { return nullptr; }
	}

	const Image* occlusionImage = occlusionPath.empty() ? nullptr : &occlusion;
	const Image* metallicRoughnessImage = nullptr;
	if (isSameImage)
	{
		// 같은 RGBA 이미지를 두 입력이 공유하여 중복 디코딩과 원본 픽셀 복사를 피한다.
		metallicRoughnessImage = &occlusion;
	}
	else if (!metallicRoughnessPath.empty())
	{
		metallicRoughnessImage = &metallicRoughness;
	}

	Image ormImage;
	if (!ORMImageBuilder::Create(occlusionImage, metallicRoughnessImage, ormImage))
	{
		Log::Write(LogLevel::Error, "AssetManager", std::format("Failed to combine ORM images: occlusion='{}', metallicRoughness='{}'", occlusionPath, metallicRoughnessPath));
		return nullptr;
	}

	// ':'와 '|'로 실제 파일 경로와 구분하고, 역할과 원본 바이트 길이를 포함한다.
	const std::string generatedPath = std::format("generated:orm|o={}:{}|m={}:{}", cacheKey.first.size(), EncodeGeneratedPathKey(cacheKey.first), cacheKey.second.size(), EncodeGeneratedPathKey(cacheKey.second));
	Texture textureData("ORM", generatedPath);
	if (!textureData.Initialize(std::move(ormImage), TextureFormat::RGBA8UNorm))
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to initialize ORM texture: " + generatedPath);
		return nullptr;
	}

	std::shared_ptr<const Texture> texture = std::make_shared<const Texture>(std::move(textureData));
	ormTextures.emplace(cacheKey, texture);
	return texture;
}

std::shared_ptr<const Texture> AssetManager::LoadEmissiveTexture(std::string_view texturePath, const Vector3& factor)
{
	constexpr TextureFormat format = TextureFormat::RGBA8SRGB;
	std::shared_ptr<const Texture> source;
	if (!texturePath.empty())
	{
		source = LoadTexture(texturePath, format);
		if (source == nullptr) { return nullptr; }

		const bool usesOriginalColor = factor.x == 1.0f && factor.y == 1.0f && factor.z == 1.0f;
		if (usesOriginalColor) { return source; }
	}

	const std::string sourceKey = source != nullptr ? Paths::MakeAssetKey(source->GetPath()) : std::string{};
	// 계수를 손실 없이 키에 넣고, 양의 0과 음의 0은 같은 키를 쓴다.
	const std::string generatedPath = std::format("generated:emissive|image={}|rgb={:a},{:a},{:a}", EncodeGeneratedPathKey(sourceKey), std::abs(factor.x), std::abs(factor.y), std::abs(factor.z));
	const std::pair<std::string, TextureFormat> cacheKey{ generatedPath, format };
	const auto cachedTexture = textures.find(cacheKey);
	if (cachedTexture != textures.end()) { return cachedTexture->second; }

	constexpr std::byte white{ 255 };
	Image image{ 1, 1, { white, white, white, white } };
	if (source != nullptr)
	{
		image = Image{ source->GetWidth(), source->GetHeight(), source->GetPixels() };
	}

	const std::array<std::byte, 256> red = MakeEmissiveChannelTable(factor.x);
	const std::array<std::byte, 256> green = MakeEmissiveChannelTable(factor.y);
	const std::array<std::byte, 256> blue = MakeEmissiveChannelTable(factor.z);
	for (std::size_t offset = 0; offset < image.pixels.size(); offset += 4)
	{
		image.pixels[offset] = red[std::to_integer<std::size_t>(image.pixels[offset])];
		image.pixels[offset + 1] = green[std::to_integer<std::size_t>(image.pixels[offset + 1])];
		image.pixels[offset + 2] = blue[std::to_integer<std::size_t>(image.pixels[offset + 2])];
	}

	Texture textureData("Emissive", generatedPath);
	if (!textureData.Initialize(std::move(image), format)) { return nullptr; }

	std::shared_ptr<const Texture> texture = std::make_shared<const Texture>(std::move(textureData));
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

			std::shared_ptr<const Material> material = LoadMaterialWithBaseColorTexture(*defaultMaterials[meshIndex], entry.baseColorTexturePath);
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
