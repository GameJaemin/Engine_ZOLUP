#include "SavedAssetReference.h"

#include "AssetManager.h"
#include "Core/Log.h"
#include "Mesh/StaticMesh.h"
#include "Paths.h"
#include "Renderer/Material.h"

#include <array>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
		AssetReference (*load)(AssetManager& assets, const SavedAssetReference& saved);
	};

	// type_index는 컴파일 시점 상수가 될 수 없어, 처음 부를 때 한 번 만드는 지역 정적 변수로 둔다.
	const std::array<SavedAssetType, 4>& GetSavedAssetTypes()
	{
		static const std::array<SavedAssetType, 4> types = { {
			{ typeid(StaticMesh), "StaticMesh", [](AssetManager& assets, const SavedAssetReference& saved) { return AssetReference(assets.LoadStaticMesh(saved.path)); } },
			{ typeid(Material), "Material", [](AssetManager& assets, const SavedAssetReference& saved) { return AssetReference(assets.LoadMaterial(saved.path)); } },
			{ typeid(Texture), "Texture", [](AssetManager& assets, const SavedAssetReference& saved) { return AssetReference(assets.LoadTexture(saved.path, saved.textureFormat)); } },
			{ typeid(Model), "Model", [](AssetManager& assets, const SavedAssetReference& saved) { return AssetReference(assets.LoadModel(saved.path)); } },
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

	/** 에셋 경로의 파일 부분을 디스크 표기로 바꾸고 하위 이름은 그대로 둔다. 파일이 디스크에 없으면 nullopt를 반환한다. */
	std::optional<std::string> FindDiskAssetPath(std::string_view path)
	{
		const std::optional<std::pair<std::string_view, std::string_view>> parts = Paths::SplitSubAssetPath(path);
		if (!parts.has_value()) { return Paths::FindDiskPath(path); }

		const std::optional<std::string> filePath = Paths::FindDiskPath(parts->first);
		if (!filePath.has_value()) { return std::nullopt; }
		return Paths::MakeSubAssetPath(*filePath, parts->second);
	}
}

std::string_view GetSavedAssetTypeName(std::type_index type)
{
	const SavedAssetType* found = FindSavedAssetType(type);
	return found != nullptr ? found->name : std::string_view();
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

	// 에셋에 남은 경로는 처음 요청한 표기일 수 있어, 파일에는 디스크를 조회한 실제 표기를 기록한다(PathConvention).
	// 아래의 다시 얻기 확인은 캐시만 보므로 지워진 파일도 통과한다. 여기서 파일이 실제로 있는지도 함께 확인된다.
	const std::optional<std::string> diskPath = FindDiskAssetPath(path);
	if (!diskPath.has_value())
	{
		LogFailure(context, std::format("{} '{}' has no file on disk for '{}'", assetType->name, asset->GetName(), path));
		return std::nullopt;
	}
	saved.path = *diskPath;

	// 파일 안의 번호로 가리키는 에셋만 이름을 남겨, 다시 내보낸 파일에서 번호가 바뀐 것을 알아챈다.
	// 파일 하나가 에셋 하나면 이름이 파일 이름에서 나오므로 남기지 않는다.
	if (Paths::SplitSubAssetPath(saved.path).has_value()) { saved.name = asset->GetName(); }
	if (assetType->type == std::type_index(typeid(Texture)))
	{
		// AssetReference 생성자가 보장하므로 type이 Texture면 asset도 Texture다.
		saved.textureFormat = static_cast<const Texture&>(*asset).GetFormat();
	}

	// 경로 모양으로 추측하지 않고, 저장한 정보로 실제로 다시 얻어 같은 객체인지 확인한다.
	// 경로만 같고 AssetManager가 만들지 않은 객체는 여기서 걸러진다.
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
	if (Paths::SplitSubAssetPath(saved.path).has_value() && asset->GetName() != saved.name)
	{
		LogFailure(context, std::format("{} '{}' name mismatch: saved '{}', loaded '{}'", saved.type, saved.path, saved.name, asset->GetName()));
		return std::nullopt;
	}

	return loaded;
}
