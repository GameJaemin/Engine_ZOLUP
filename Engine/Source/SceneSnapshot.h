#pragma once

#include "CameraViewInfo.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector3.h"
#include "ObjectID.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct Material;
struct Model;
class StaticMesh;
class Texture;
class World;

struct StaticMeshSnapshot
{
	// 에셋은 공유 참조로 담는다. 공유하는 동안 데이터를 바꾸지 않는다는 에셋 계약에 기댄다.
	// nullptr는 컴포넌트와 같이 "그리지 않음" 또는 "재질 미지정"이다.
	std::shared_ptr<const StaticMesh> mesh;
	std::shared_ptr<const Material> material;
};

struct SkyboxSnapshot
{
	// +X, -X, +Y, -Y, +Z, -Z 순서다. 빈 면이 있으면 배경 후보에서 건너뛴다.
	std::array<std::shared_ptr<const Texture>, 6> faces;
	bool enabled;
};

enum class LightSnapshotType : unsigned char { Directional, Point, Spot, Ambient };

struct LightSnapshot
{
	LightSnapshotType type;
	// 선형 RGB다.
	Vector3 color;
	float intensity;
	bool enabled;
	// Point와 Spot에서만 쓴다. 단위는 cm다.
	float rangeCm = 0.0f;
	// Spot에서만 쓴다. 중심축에서 원뿔 가장자리까지의 각도(도)이며 컴포넌트 값 그대로다.
	float outerConeAngleDegrees = 0.0f;
};

struct CameraSnapshot
{
	CameraProjectionMode projectionMode;
	// 두 모드의 값을 모두 담는다. 컴포넌트가 모드를 바꿔도 각 값을 유지하기 때문이다.
	float verticalFovDegrees;
	float orthographicHeight;
	// 단위는 cm다.
	float nearClipDistance;
	float farClipDistance;
};

struct AnimationSnapshot
{
	// 클립 node 번호 순서다. 주소 대신 노드 객체의 ID를 담으며, 무효 ID는 객체로 만들지 않은 노드(뼈)다.
	std::vector<ObjectID> nodeObjectIDs;
	// 노드 번호 순서의 원래 부모 기준 값이다. 재생 상태(현재 자세, 클립, 시각)는 담지 않는다.
	std::vector<Transform> restPose;
};

struct SkinSnapshot
{
	std::shared_ptr<const Model> model;
	// 이 객체가 된 메시 노드의 배치표 번호다.
	std::size_t nodeIndex;
	// 포즈를 읽을 Animation을 가진 객체다. 무효면 배치표의 원래 값을 쓴다.
	ObjectID animationObjectID;
};

/** 객체 하나의 상태다. 컴포넌트는 종류마다 한 칸이며, 다른 객체를 가리키는 참조는 ObjectID로 담는다. */
struct GameObjectSnapshot
{
	ObjectID objectID;
	std::string name;
	// 무효면 루트다. 스냅샷 배열에서 부모가 자식보다 먼저 나온다.
	ObjectID parentID;
	// 부모 기준 엔진 좌표계이며 위치 단위는 cm다.
	Transform localTransform;
	std::optional<StaticMeshSnapshot> staticMesh;
	std::optional<SkyboxSnapshot> skybox;
	// 조명 4종은 한 칸이다. 한 객체에 조명은 하나만 담는다.
	std::optional<LightSnapshot> light;
	std::optional<CameraSnapshot> camera;
	std::optional<AnimationSnapshot> animation;
	std::optional<SkinSnapshot> skin;
};

/** World 하나의 객체 상태를 메모리에 담는다. */
struct SceneSnapshot
{
	// 부모를 먼저 담는 순서다. 루트끼리는 등록 순서, 자식은 기존 자식 순서를 따른다.
	std::vector<GameObjectSnapshot> objects;
	// 활성 카메라를 가진 객체다. 무효면 활성 카메라가 없다.
	ObjectID activeCameraObjectID;
};

/**
 * 삭제 예정이 아닌 객체를 담는다.
 * 파생 GameObject, 지원하지 않는 컴포넌트, 같은 종류 컴포넌트 중복, 스냅샷 밖을 가리키는 참조가 있으면
 * 로그를 남기고 nullopt를 반환한다.
 */
[[nodiscard]] std::optional<SceneSnapshot> CaptureWorld(const World& world);

/**
 * 객체가 없는 World에 ObjectID를 유지해 복원한다.
 * 실패하면 로그를 남기고 false를 반환한다. 이미 만든 객체가 남으므로 호출자는 그 World를 버린다.
 */
[[nodiscard]] bool RestoreWorld(const SceneSnapshot& snapshot, World& world);
