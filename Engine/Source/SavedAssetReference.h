#pragma once

#include "AssetReference.h"
#include "Texture/Texture.h"

#include <optional>
#include <string>
#include <string_view>

class AssetManager;

/** 파일에 적을 수 있는 에셋 참조다. path가 비어 있으면 "지정 안 함"이다. */
struct SavedAssetReference
{
	// 에셋 타입 이름이다. "StaticMesh", "Material", "Texture", "Model" 중 하나다.
	std::string type;
	// 에셋 경로(Asset::GetPath)의 파일 부분을 디스크의 실제 표기로 바꾼 값이다. 비어 있으면 가리키지 않는 참조다.
	std::string path;
	// 파일 안의 에셋(경로에 '|'가 있는 에셋)만 쓰는 에셋의 이름(Asset::GetName)이다. 불러온 에셋의 이름과 다르면 실패해, 다시 내보낸 glTF의 번호가 바뀐 것을 알아챈다.
	// 파일 하나가 에셋 하나면 이름도 파일 이름에서 나오므로 비워 두고 비교하지 않는다.
	std::string name;
	// Texture만 쓴다. 같은 파일도 형식이 다르면 다른 Texture라서 경로와 함께 정체성이 된다. 다른 타입은 Unknown이다.
	TextureFormat textureFormat = TextureFormat::Unknown;
};

/**
 * 저장할 수 있는 참조로 바꾼다. 가리키지 않는 참조는 빈 경로로 성공한다.
 * 저장한 정보로 assets에서 다시 얻은 에셋이 원래와 같은 객체일 때만 성공하므로, 성공한 결과는 LoadAssetReference로 같은 객체를 돌려준다.
 * 지원하지 않는 타입이거나, 경로가 없거나, 경로가 가리키는 파일이 디스크에 없거나, 경로로 같은 에셋을 다시 얻을 수 없으면 context와 이유를 로그로 남기고 nullopt를 반환한다.
 */
[[nodiscard]] std::optional<SavedAssetReference> SaveAssetReference(const AssetReference& reference, AssetManager& assets, std::string_view context);

/**
 * 저장한 참조로 에셋을 다시 얻는다. 같은 경로면 AssetManager가 캐시한 같은 객체다. 빈 경로는 가리키지 않는 참조로 성공한다.
 * 타입 이름이 틀리거나, 에셋을 얻지 못하거나, 파일 안의 에셋의 이름이 저장한 이름과 다르면 context와 이유를 로그로 남기고 nullopt를 반환한다.
 */
[[nodiscard]] std::optional<AssetReference> LoadAssetReference(const SavedAssetReference& saved, AssetManager& assets, std::string_view context);
