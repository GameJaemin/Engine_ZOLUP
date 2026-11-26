#include "SavedAssetReference.h"

#include "AssetManager.h"
#include "Core/Log.h"
#include "Mesh/StaticMesh.h"
#include "Renderer/Material.h"

#include <array>
#include <format>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace
{
	// 저장할 수 있는 에셋 타입 하나다. load는 저장한 경로로 AssetManager가 캐시한 같은 에셋을 얻어 참조로 돌려준다.
	struct SavedAssetType
	{
		std::type_index type;
		// 파일에 적는 이름이다. typeid의 이름은 컴파일러마다 달라 직접 정한다.
		std::string_view name;
		// 경로가 파일 안의 번호로 가리키는 타입이면 true다. 에셋 이름을 저장하고 불러올 때 비교한다.
		// 파일 경로로 가리키는 타입은 이름도 파일 이름에서 나오므로 비교할 것이 없다.
		bool checksName;
		AssetReference (*load)(AssetManager& assets, const SavedAssetReference& saved);
	};

	// type_index는 컴파일 시점 상수가 될 수 없어, 처음 부를 때 한 번 만드는 지역 정적 변수로 둔다.
	const std::array<SavedAssetType, 4>& GetSavedAssetTypes()
	{
		static const std::array<SavedAssetType, 4> types = { {
			{ typeid(StaticMesh), "StaticMesh", true, [](AssetManager& assets, const SavedAssetReference& saved) { return AssetReference(assets.LoadStaticMesh(saved.path)); } },
			{ typeid(Material), "Material", true, [](AssetManager& assets, const SavedAssetReference& saved) { return AssetReference(assets.LoadMaterial(saved.path)); } },
			{ typeid(Texture), "Texture", false, [](AssetManager& assets, const SavedAssetReference& saved) { return AssetReference(assets.LoadTexture(saved.path, saved.textureFormat)); } },
			{ typeid(Model), "Model", false, [](AssetManager& assets, const SavedAssetReference& saved) { return AssetReference(assets.LoadModel(saved.path)); } },
		} };
		return types;
	}

	const SavedAssetType* FindSavedAssetType(std::type_index type)
	{
		for (const SavedAssetType& candidate : GetSavedAssetTypes())
		{
			if (candidate.type == type) { return &candidate; }
		}
		return nullptr;
	}

	const SavedAssetType* FindSavedAssetType(std::string_view name)
	{
		for (const SavedAssetType& candidate : GetSavedAssetTypes())
		{
			if (candidate.name == name) { return &candidate; }
		}
		return nullptr;
	}

	void LogFailure(std::string_view context, std::string_view reason)
	{
		Log::Write(LogLevel::Error, "AssetReference", std::format("{}: {}", context, reason));
	}
}

std::optional<SavedAssetReference> SaveAssetReference(const AssetReference& reference, AssetManager& assets, std::string_view context)
{
	const SavedAssetType* assetType = FindSavedAssetType(reference.GetType());
	if (assetType == nullptr)
	{
		LogFailure(context, std::format("asset type '{}' cannot be saved", reference.GetType().name()));
		return std::nullopt;
	}

	SavedAssetReference saved;
	saved.type = assetType->name;
	const std::shared_ptr<const Asset>& asset = reference.GetAsset();
	if (asset == nullptr) { return saved; }

	// 빈 경로는 코드에서 만든 에셋이라 파일에서 다시 만들 수 없다.
	// 빈 참조로 바꾸면 데이터가 조용히 사라지므로 저장 자체를 실패로 둔다.
	const std::string& path = asset->GetPath();
	if (path.empty())
	{
		LogFailure(context, std::format("{} '{}' has no path because it was not loaded from a file", assetType->name, asset->GetName()));
		return std::nullopt;
	}

	saved.path = path;
	if (assetType->checksName) { saved.name = asset->GetName(); }
	if (assetType->type == std::type_index(typeid(Texture)))
	{
		// AssetReference 생성자가 보장하므로 type이 Texture면 asset도 Texture다.
		saved.textureFormat = static_cast<const Texture&>(*asset).GetFormat();
	}

	// 경로 모양으로 추측하지 않고, 저장한 정보로 실제로 다시 얻어 같은 객체인지 확인한다.
	// 합성 텍스처처럼 파일을 가리키지 않는 경로나 AssetManager가 만들지 않은 객체는 여기서 걸러진다.
	// 저장된 에셋은 이미 캐시에 있으므로 보통은 캐시 조회 비용만 든다.
	const AssetReference reloaded = assetType->load(assets, saved);
	if (reloaded.GetAsset() != asset)
	{
		LogFailure(context, std::format("{} '{}' cannot be loaded again as the same asset from '{}'", assetType->name, asset->GetName(), saved.path));
		return std::nullopt;
	}
	return saved;
}

std::optional<AssetReference> LoadAssetReference(const SavedAssetReference& saved, AssetManager& assets, std::string_view context)
{
	const SavedAssetType* assetType = FindSavedAssetType(saved.type);
	if (assetType == nullptr)
	{
		LogFailure(context, std::format("unknown asset type '{}'", saved.type));
		return std::nullopt;
	}

	if (saved.path.empty()) { return AssetReference(assetType->type); }

	if (assetType->type == std::type_index(typeid(Texture)) && saved.textureFormat == TextureFormat::Unknown)
	{
		LogFailure(context, std::format("texture '{}' has no format", saved.path));
		return std::nullopt;
	}

	// 실패 이유는 각 Load 함수가 먼저 로그로 남긴다.
	AssetReference loaded = assetType->load(assets, saved);
	const std::shared_ptr<const Asset>& asset = loaded.GetAsset();
	if (asset == nullptr)
	{
		LogFailure(context, std::format("could not load {} '{}'", saved.type, saved.path));
		return std::nullopt;
	}

	// 번호로 가리키는 경로는 glTF를 다시 내보내 순서가 바뀌어도 오류 없이 다른 에셋을 얻을 수 있어 이름으로 한 번 더 확인한다.
	if (assetType->checksName && asset->GetName() != saved.name)
	{
		LogFailure(context, std::format("{} '{}' name mismatch: saved '{}', loaded '{}'", saved.type, saved.path, saved.name, asset->GetName()));
		return std::nullopt;
	}

	return loaded;
}
