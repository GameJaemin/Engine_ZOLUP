#pragma once

#include "Animation/AnimationClip.h"
#include "GLTFLoader.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

struct Material;
struct Prefab;
class StaticMesh;
class Texture;
enum class TextureFormat;

/**
 * glTF 파일 하나에서 불러온 메시들과 배치표와 애니메이션 클립들이다. 모두 엔진 좌표계(왼손, cm)다.
 * 장면에 놓기 전의 원본이며, 배치는 배치표를 읽어 만든 GameObject가 맡는다.
 */
struct Model
{
	// glTF meshes 번호 순서다. 각 경로는 "파일경로#번호"이며, 렌더러가 경로로 메시를 구분하기 때문이다.
	std::vector<std::shared_ptr<const StaticMesh>> meshes;
	// meshes와 같은 순서이며, 각 메시의 기본 재질이다. nullptr가 아니다.
	// 같은 glTF 재질을 쓰는 메시는 같은 객체를 공유하고, 재질이 없는 메시는 glTF 기본 재질(흰색, 금속성 1, 거칠기 1)을 공유한다.
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

private:
	/**
	 * glTF 재질 설명으로 Material을 만든다. 색 텍스처(sRGB), 노멀 텍스처와 ORM 텍스처(선형)는 modelDirectory(저장소 루트 기준 모델 폴더) 기준으로 불러온다.
	 * 차폐 텍스처는 금속성-거칠기 텍스처와 같은 이미지일 때만 ORM의 R로 쓰이며, 아니면 경고 로그를 남기고 무시한다.
	 * 텍스처를 불러오지 못하면 nullptr를 반환한다.
	 */
	std::shared_ptr<const Material> CreateMaterial(const GLTFModelMaterial& source, const std::filesystem::path& modelDirectory);

	std::unordered_map<std::string, std::shared_ptr<const Model>> models;

	// 텍스처
public:
	/**
	 * 저장소 루트 기준 UTF-8 경로의 PNG를 불러오며, 같은 경로와 형식은 캐시해 재사용한다.
	 * format은 RGBA8UNorm 또는 RGBA8SRGB이다. GPU 준비는 렌더링 단계에서 수행한다.
	 * 잘못된 형식이나 로딩 실패는 nullptr를 반환하며, 경로 변환과 할당 예외는 전파한다.
	 */
	std::shared_ptr<const Texture> LoadTexture(std::string_view relativePath, TextureFormat format);

private:
	std::map<std::pair<std::string, TextureFormat>, std::shared_ptr<const Texture>> textures;

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
