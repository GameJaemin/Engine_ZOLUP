#include "AssetManager.h"

#include "Core/Log.h"
#include "GLTFLoader.h"
#include "Image.h"
#include "ImageLoader.h"
#include "Mesh/StaticMesh.h"
#include "Paths.h"
#include "Texture/Texture.h"

#include <cassert>
#include <string>
#include <utility>

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

std::shared_ptr<const StaticMesh> AssetManager::LoadStaticMesh(std::string_view relativePath)
{
	std::filesystem::path absolutePath;
	if (!Paths::GetAbsolutePath(relativePath, absolutePath))
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to resolve static mesh path: " + std::string(relativePath));
		return nullptr;
	}

	absolutePath = absolutePath.lexically_normal();
	absolutePath.make_preferred();

	const auto cachedMesh = staticMeshes.find(absolutePath);
	if (cachedMesh != staticMeshes.end()) { return cachedMesh->second; }

	// UTF-8 경로를 정규화하고 구분자를 /로 통일해 에셋에 보관한다.
	const std::filesystem::path assetPath = std::filesystem::path(std::u8string(relativePath.begin(), relativePath.end())).lexically_normal();
	const std::u8string assetNameUtf8 = assetPath.stem().u8string();
	const std::u8string assetPathUtf8 = assetPath.generic_u8string();
	StaticMesh meshData(std::string(assetNameUtf8.begin(), assetNameUtf8.end()), std::string(assetPathUtf8.begin(), assetPathUtf8.end()));
	if (!GLTFLoader::LoadStaticMesh(absolutePath, meshData))
	{
		Log::Write(LogLevel::Error, "AssetManager", "Failed to load static mesh: " + std::string(relativePath));
		return nullptr;
	}

	std::shared_ptr<const StaticMesh> mesh = std::make_shared<const StaticMesh>(std::move(meshData));
	staticMeshes.emplace(std::move(absolutePath), mesh);
	return mesh;
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

	const std::pair<std::filesystem::path, TextureFormat> cacheKey(absolutePath, format);
	const auto cachedTexture = textures.find(cacheKey);
	if (cachedTexture != textures.end()) { return cachedTexture->second; }

	Image image;
	if (!ImageLoader::LoadPNG(absolutePath, image)) { return nullptr; }

	const std::u8string relativePathUtf8(relativePath.begin(), relativePath.end());
	const std::filesystem::path requestedPath(relativePathUtf8);
	const std::filesystem::path assetPath = requestedPath.lexically_normal();

	// TODO: Texture 생성 및 캐시 저장을 연결한다.
	return nullptr;
}
