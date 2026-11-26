#pragma once

#include "Animation/AnimationClip.h"
#include "Asset.h"
#include "GLTF/GLTFLoader.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class Material;
struct Prefab;
class StaticMesh;
class Texture;
enum class TextureFormat;

/**
 * glTF 파일 하나에서 불러온 메시들과 배치표와 애니메이션 클립들과 스킨들이다. 모두 엔진 좌표계(왼손, cm)다.
 * 장면에 놓기 전의 원본이며, 배치는 배치표를 읽어 만든 GameObject가 맡는다.
 */
class Model : public Asset
{
public:
	/**
	 * 파일 이름에서 확장자를 뺀 UTF-8 이름과 저장소 루트 기준 UTF-8 상대 경로(구분자 /)를 받는다.
	 * 경로는 메시와 클립 경로의 "#번호" 앞부분과 같으며, AssetManager의 캐시 키(정규화한 절대 경로로 만든 키)와는 다르다.
	 */
	Model(std::string newName, std::string newPath);

public:
	// glTF meshes 번호 순서다. 각 경로는 "파일경로#번호"이며, 렌더러가 경로로 메시를 구분하기 때문이다.
	std::vector<std::shared_ptr<const StaticMesh>> meshes;
	// glTF 파일의 재질 목록과 같은 순서다. i번 재질의 경로는 "파일경로#i"다. 어느 메시도 쓰지 않는 재질도 들어 있다.
	std::vector<std::shared_ptr<const Material>> materials;
	// 재질을 지정하지 않은 메시가 쓰는 glTF 기본 재질(흰색, 금속성 1, 거칠기 1)이다. 경로는 "파일경로#default"다.
	// 그런 메시가 없으면 nullptr이다.
	std::shared_ptr<const Material> defaultMaterial;
	// meshes와 같은 순서로 각 메시가 쓰는 재질이며 nullptr가 아니다.
	// 새 객체가 아니라 materials나 defaultMaterial의 객체를 함께 가리키므로, 같은 재질을 쓰는 메시끼리 같은 객체를 공유한다.
	std::vector<std::shared_ptr<const Material>> meshMaterials;
	// glTF nodes 번호 순서다. 각 노드의 mesh 번호로 meshes를 찾는다.
	std::vector<GLTFSceneNode> sceneNodes;
	// glTF animations 번호 순서다. 각 경로는 "파일경로#번호"다. 클립의 node 번호로 sceneNodes를 찾는다.
	std::vector<std::shared_ptr<const AnimationClip>> animations;
	// glTF skins 번호 순서다. sceneNodes의 skin 번호로 찾고, 각 스킨의 joints는 sceneNodes 번호다.
	std::vector<std::shared_ptr<const Skin>> skins;
};

/**
 * 불러온 CPU 자산을 경로별로 보관하고 재사용한다. Engine이 값으로 소유하며 Get으로 어디서든 빌려 쓴다.
 * 한 번에 하나만 존재해야 하고 메인 스레드에서 사용한다.
 */
class AssetManager
{
	// 생성과 소멸
public:
	/** 이 매니저를 Get의 대상으로 등록한다. 이미 등록된 매니저가 있으면 안 된다. */
	AssetManager();

	/** Get의 등록을 해제한다. */
	~AssetManager();

	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;
	AssetManager(AssetManager&&) = delete;
	AssetManager& operator=(AssetManager&&) = delete;

	// 전역 접근
public:
	/** 등록된 매니저를 빌려준다. 매니저가 살아 있는 동안(엔진 생성부터 소멸까지)만 호출한다. */
	static AssetManager& Get();

private:
	// 비소유. 생성자에서 등록하고 소멸자에서 비운다. 등록된 매니저가 없으면 nullptr이다.
	static AssetManager* instance;

	// 모델
public:
	/**
	 * 메인 스레드에서 저장소 루트 기준 UTF-8 경로의 glTF 파일에서 모든 메시, 재질, 배치표와 애니메이션 클립을 불러온다.
	 * 파일명과 폴더명의 대소문자는 실제 자산 경로에 맞춘다.
	 * 이 매니저의 수명 동안 보관하고, 같은 경로 요청에 재사용한다. 실패하면 nullptr를 반환한다.
	 */
	std::shared_ptr<const Model> LoadModel(std::string_view relativePath);

	/**
	 * "모델경로#번호" 경로의 메시를 돌려준다. 모델을 아직 불러오지 않았으면 불러온다.
	 * 모델을 불러오지 못하거나, 경로 형식이 틀리거나, 번호가 범위를 벗어나면 로그를 남기고 nullptr를 반환한다.
	 */
	[[nodiscard]] std::shared_ptr<const StaticMesh> LoadStaticMesh(std::string_view path);

	/**
	 * "모델경로#번호", "모델경로#default" 또는 그 뒤에 "|baseColorTexture=텍스처경로"가 붙은 경로의 재질을 돌려준다.
	 * 모델이나 텍스처를 아직 불러오지 않았으면 불러온다.
	 * 모델이나 텍스처를 불러오지 못하거나, 경로 형식이 틀리거나, 번호가 범위를 벗어나거나, 기본 재질을 쓰는 메시가 없으면 로그를 남기고 nullptr를 반환한다.
	 */
	[[nodiscard]] std::shared_ptr<const Material> LoadMaterial(std::string_view path);

	/**
	 * baseMaterial의 값을 복사해 기본 색 텍스처만 바꾼 재질을 돌려준다. 텍스처를 읽지 못하면 nullptr를 반환한다.
	 * baseMaterial이 이미 불러온 모델의 재질이면 경로는 "기본재질경로|baseColorTexture=텍스처경로"이고, 같은 요청에 같은 객체를 재사용한다.
	 * 경로가 없거나, 이미 교체한 재질이거나, 경로만 같은 다른 객체면 경로 없이 매번 새로 만든다.
	 */
	[[nodiscard]] std::shared_ptr<const Material> LoadMaterialWithBaseColorTexture(const Material& baseMaterial, std::string_view texturePath);

private:
	/**
	 * 이미 불러온 모델에서 "모델경로#번호" 또는 "모델경로#default" 경로의 재질을 찾는다. 교체본 경로는 찾지 않는다.
	 * 파일을 읽지 않고 로그도 남기지 않는다. 모델을 아직 불러오지 않았거나 경로가 맞지 않으면 nullptr를 반환한다.
	 */
	std::shared_ptr<const Material> FindLoadedMaterial(std::string_view path) const;

	/**
	 * glTF 재질 설명으로 path 경로의 Material을 만든다. 색 텍스처(sRGB), 노멀 텍스처와 ORM 텍스처(선형)는 modelDirectory(저장소 루트 기준 모델 폴더) 기준으로 불러온다.
	 * 차폐의 R과 금속성-거칠기의 G, B를 ORM으로 합성하며, 없는 입력의 채널은 1로 채운다.
	 * 두 입력이 모두 없으면 ORM 텍스처 없이 계수만 사용한다.
	 * 색, 노멀 또는 ORM 준비에 실패하면 nullptr를 반환한다. ORM 입력 이미지의 크기는 같아야 한다.
	 * 발광 계수가 0이 아니면 발광 텍스처를 준비한다. 실패하면 경고하고 발광 텍스처를 비워 둔다.
	 */
	std::shared_ptr<const Material> CreateMaterial(const GLTFModelMaterial& source, const std::filesystem::path& modelDirectory, std::string path);

	std::unordered_map<std::string, std::shared_ptr<const Model>> models;
	// 교체본 경로에서 교체본으로 가는 캐시다. 같은 기본 재질과 텍스처 요청에 같은 객체를 돌려준다.
	std::unordered_map<std::string, std::shared_ptr<const Material>> baseColorTextureMaterials;

	// 텍스처
public:
	/**
	 * 저장소 루트 기준 UTF-8 경로의 PNG를 불러오며, 같은 경로와 형식은 캐시해 재사용한다.
	 * format은 RGBA8UNorm 또는 RGBA8SRGB이다. GPU 준비는 렌더링 단계에서 수행한다.
	 * 잘못된 형식이나 로딩 실패는 nullptr를 반환하며, 경로 변환과 할당 예외는 전파한다.
	 */
	std::shared_ptr<const Texture> LoadTexture(std::string_view relativePath, TextureFormat format);

private:
	/**
	 * 저장소 루트 기준 UTF-8 경로의 이미지를 합쳐 선형 ORM 텍스처를 만든다.
	 * 빈 경로는 해당 이미지가 없다는 뜻이며, 같은 입력 조합은 매니저 수명 동안 재사용한다.
	 * 두 경로가 모두 비었거나 로딩, 합성 또는 초기화에 실패하면 nullptr를 반환한다.
	 * 생성 텍스처의 경로는 파일을 가리키지 않는 식별자다. 경로 변환과 할당 예외는 전파한다.
	 */
	std::shared_ptr<const Texture> LoadORMTexture(std::string_view occlusionPath, std::string_view metallicRoughnessPath);

	/** 원본 이미지 또는 흰색 한 픽셀에 선형 RGB 발광 계수(0~1)를 적용한 sRGB 텍스처를 준비해 공유한다. */
	std::shared_ptr<const Texture> LoadEmissiveTexture(std::string_view texturePath, const Vector3& factor);

private:
	std::map<std::pair<std::string, TextureFormat>, std::shared_ptr<const Texture>> textures;
	// 정규화한 절대 경로를 차폐, 금속성-거칠기 순서로 묶는다. 빈 키는 입력 없음이다.
	std::map<std::pair<std::string, std::string>, std::shared_ptr<const Texture>> ormTextures;

	// 프리팹
public:
	/**
	 * 저장소 루트 기준 UTF-8 상대 경로에서 프리팹을 불러오고 같은 경로 요청에 재사용한다.
	 * 지정된 메시의 기본 재질을 복사해 색상 텍스처를 교체한다.
	 * 잘못된 설정이나 참조된 에셋의 로드 실패는 nullptr를 반환한다.
	 * 재질 오버라이드가 없으면 모델을 생략할 수 있다.
	 */
	[[nodiscard]] std::shared_ptr<const Prefab> LoadPrefab(std::string_view relativePath);

private:
	std::unordered_map<std::string, std::shared_ptr<const Prefab>> prefabs;
};
