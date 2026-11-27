#pragma once

// Prefab 데이터(PrefabObjects), 놓기(InstantiatePrefab), 생성기(CreateModelPrefab, CreatePrefabFromWorld), 파일 쓰기와 읽기를 담는다.
// 옛 Prefab 구조체를 지울 때 PrefabObjects를 Prefab으로, 이 파일을 Prefab.h로 바꾼다.

#include "Core/Math/Transform.h"
#include "ObjectID.h"
#include "Scene/SceneSnapshot.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class AssetManager;
class GameObject;
class Material;
class Model;
class World;

/** Prefab 하나의 완성된 객체 상태다. 놓을 때 사본의 ID를 새로 뽑아 World에 복원한다. */
struct PrefabObjects
{
	// 등록 순서다. 루트도 이 안에 있다.
	std::vector<GameObjectSnapshot> objects;
	// objects 안에서 루트 객체의 ID다.
	ObjectID rootObjectID;
};

/** 프리팹 루트의 배치 값이다. 비어 있는 항목은 프리팹 기본값을 쓴다. */
struct PrefabPlacement
{
	std::optional<std::string> name;
	// 루트의 월드 Transform이다. 위치 단위는 cm다.
	std::optional<Transform> transform;
};

/**
 * prefab의 사본을 새 ObjectID로 world에 만들고 루트를 반환한다. placement에서 지정한 루트 이름과 월드 Transform만 덮어쓴다.
 * World 갱신 중에는 호출하지 않는다. 실패하면 로그를 남기고 이번에 만든 객체를 삭제 예정으로 표시한 뒤 nullptr를 반환한다.
 */
GameObject* InstantiatePrefab(World& world, const PrefabObjects& prefab, const PrefabPlacement& placement);

/**
 * model의 노드 계층을 root 아래에 객체로 만든다. 메시 노드에 StaticMesh, 클립이 있으면 root에 Animation, 스킨 노드에 Skin을 붙인다.
 * materialOverrides는 비어 있거나 모델 메시 수와 같아야 하며 nullptr 원소는 모델 기본 재질이다.
 * 실패하면 이번에 만든 노드 중 root 아래에 연결하지 못한 것만 삭제 예정으로 표시하고 false를 반환한다. 나머지 정리는 호출자가 한다.
 */
bool BuildModelHierarchy(GameObject& root, const std::shared_ptr<const Model>& model, const std::vector<std::shared_ptr<const Material>>& materialOverrides);

/**
 * world의 객체를 모두 찍어 root를 루트로 하는 Prefab 객체 상태로 만든다. world에는 root와 그 자손만 있어야 한다.
 * 실패하면 로그를 남기고 nullopt를 반환한다.
 */
std::optional<PrefabObjects> CreatePrefabFromWorld(const World& world, const GameObject& root);

/**
 * glTF model을 Prefab 객체 상태로 만든다. 모델 이름의 루트 아래에 모델 노드 계층이 붙는다.
 * 임시 World에 객체를 만든 뒤 찍으므로 JoltRuntime 초기화 뒤 메인 스레드에서 부른다. 실패하면 로그를 남기고 nullopt를 반환한다.
 */
std::optional<PrefabObjects> CreateModelPrefab(const std::shared_ptr<const Model>& model, const std::vector<std::shared_ptr<const Material>>& materialOverrides);

/**
 * prefab을 path(절대 경로)에 저장한다. 맨 위 항목은 rootObject와 objects이며 UTF-8, CRLF, 탭 들여쓰기로 쓴다.
 * 에셋 참조를 저장할 수 없거나 파일을 쓰지 못하면 로그를 남기고 false를 반환하며, 기존 파일은 그대로다.
 */
bool SavePrefabFile(const std::filesystem::path& path, const PrefabObjects& prefab, AssetManager& assets);

/**
 * path(절대 경로)의 Prefab 파일을 읽는다.
 * JSON이 아니거나, 맨 위에 rootObject와 objects 외의 항목이 있거나, 루트가 objects 안에 없거나 다른 객체의 자식이거나, 객체를 읽지 못하면 로그를 남기고 nullopt를 반환한다.
 */
std::optional<PrefabObjects> LoadPrefabFile(const std::filesystem::path& path, AssetManager& assets);
