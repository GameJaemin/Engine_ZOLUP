#pragma once

#include "Core/Math/Transform.h"
#include "Core/Math/Vector3.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Material;
class Model;
class StaticMesh;
class Texture;
enum class CameraProjectionMode : unsigned char;

enum class PrefabLightType : unsigned char { Directional, Point, Spot, Ambient };

struct PrefabLightDefinition
{
	PrefabLightType type;
	// 아래 세 값은 JSON에서 생략하면 LightComponent 기본값(흰색, 1, true)이 들어간다.
	// 선형 RGB다.
	Vector3 color;
	float intensity;
	bool enabled;
	// Point, Spot에서만 쓴다. 단위는 cm다.
	float rangeCm = 0.0f;
	// Spot에서만 쓴다. 원뿔 전체 각도이며 단위는 도다.
	float outerConeDegrees = 0.0f;
};

struct PrefabCameraDefinition
{
	CameraProjectionMode projection;
	// Perspective면 세로 시야각(도), Orthographic이면 보이는 영역의 세로 길이(cm)다.
	float projectionValue;
	// JSON에서 생략하면 CameraComponent 기본값이 들어간다. 단위는 cm다.
	float nearClipCm;
	float farClipCm;
};

struct PrefabBoxColliderDefinition
{
	// 객체 로컬 좌표이며 단위는 cm다. 반크기는 축마다 양수여야 한다.
	Vector3 centerCm;
	Vector3 halfExtentsCm;
};

struct PrefabRigidBodyDefinition
{
	// 생략하면 RigidBodyComponent 기본값과 같은 true다.
	bool simulatePhysics = true;
};

struct PrefabSkybox
{
	// +X, -X, +Y, -Y, +Z, -Z 순서로 로드한 면이다.
	std::array<std::shared_ptr<const Texture>, 6> faces;
	bool enabled = true;
};

struct PrefabStaticMesh
{
	std::shared_ptr<const StaticMesh> mesh;
	std::shared_ptr<const Material> material;
};

struct PrefabNodeBinding
{
	// 모델 배치표의 노드 번호다.
	std::size_t sourceNode;
	// 같은 프리팹 안에서 이 노드의 값을 받을 객체의 id다.
	std::string targetID;
};

struct PrefabAnimation
{
	// 클립과 노드 배치표를 가진 모델이다.
	std::shared_ptr<const Model> model;
	// 연결하지 않은 노드는 객체 없이 포즈 배열에만 값이 남는다.
	std::vector<PrefabNodeBinding> nodeBindings;
};

struct PrefabSkin
{
	// 스킨과 노드 배치표를 가진 모델이다.
	std::shared_ptr<const Model> model;
	// 이 객체가 된 메시 노드의 배치표 번호다. 그 노드에는 스킨이 있다.
	std::size_t sourceNode;
};

struct PrefabObject
{
	// 같은 프리팹 안에서 유일한 객체 이름표다. 표시 이름과 별개다.
	std::optional<std::string> id;
	std::optional<std::string> name;
	// 부모 기준 엔진 좌표계이며 위치 단위는 cm다.
	Transform localTransform;
	std::optional<PrefabStaticMesh> staticMesh;
	std::optional<PrefabSkybox> skybox;
	std::optional<PrefabAnimation> animation;
	std::optional<PrefabSkin> skin;
	// 에셋이 없어 읽은 값을 그대로 쓴다.
	std::optional<PrefabLightDefinition> light;
	std::optional<PrefabCameraDefinition> camera;
	std::optional<PrefabBoxColliderDefinition> boxCollider;
	std::optional<PrefabRigidBodyDefinition> rigidBody;
	std::vector<PrefabObject> children;
};

struct Prefab
{
	PrefabObject root;
	// 기존 형식의 최상위 model로 전개할 glTF 모델이다.
	std::shared_ptr<const Model> model;

	// 비어 있으면 모든 메시가 기본 재질을 쓴다. 그 외에는 모델 메시와 같은 순서이며 nullptr는 기본 재질을 뜻한다.
	std::vector<std::shared_ptr<const Material>> meshMaterialOverrides;
};

/** 프리팹 루트의 배치 값이다. 비어 있는 항목은 프리팹 기본값을 쓴다. */
struct PrefabPlacement
{
	std::optional<std::string> name;
	// 루트의 월드 Transform이다. 위치 단위는 cm다.
	std::optional<Transform> transform;
};

struct PrefabMaterialOverride
{
	// 0부터 시작하는 모델 메시 번호다. 실제 메시 범위는 모델을 읽은 뒤 확인한다.
	std::size_t meshIndex;
	// 이 메시에 쓸 재질의 경로다.
	std::string materialPath;
};

struct PrefabSkyboxDefinition
{
	// +X, -X, +Y, -Y, +Z, -Z 순서다.
	std::array<std::string, 6> facePaths;
	bool enabled = true;
};

struct PrefabStaticMeshDefinition
{
	std::string modelPath;
	// 0부터 시작하는 모델 메시 번호다.
	std::size_t meshIndex;
	// 생략하면 모델의 기본 재질을 사용한다.
	std::optional<std::string> materialPath;
};

struct PrefabAnimationDefinition
{
	std::string modelPath;
	// 노드 번호와 대상 id는 각각 한 번씩만 나온다. 노드 번호 범위는 모델을 읽은 뒤 확인한다.
	std::vector<PrefabNodeBinding> nodeBindings;
};

struct PrefabSkinDefinition
{
	std::string modelPath;
	// 메시 노드의 배치표 번호다. 범위와 스킨 유무는 모델을 읽은 뒤 확인한다.
	std::size_t sourceNode;
};

struct PrefabObjectDefinition
{
	std::optional<std::string> id;
	std::optional<std::string> name;
	Transform localTransform;
	std::optional<PrefabStaticMeshDefinition> staticMesh;
	std::optional<PrefabSkyboxDefinition> skybox;
	std::optional<PrefabAnimationDefinition> animation;
	std::optional<PrefabSkinDefinition> skin;
	std::optional<PrefabLightDefinition> light;
	std::optional<PrefabCameraDefinition> camera;
	std::optional<PrefabBoxColliderDefinition> boxCollider;
	std::optional<PrefabRigidBodyDefinition> rigidBody;
	std::vector<PrefabObjectDefinition> children;
};

struct PrefabDefinition
{
	PrefabObjectDefinition root;
	std::optional<std::string> modelPath;
	std::vector<PrefabMaterialOverride> materialOverrides;
};

/**
 * 절대 경로에서 프리팹 JSON을 읽고 루트 객체와 자식 계층 또는 기존 모델 설정을 검증한다.
 * 모델과 텍스처 경로는 저장소 루트 기준 UTF-8 상대 경로다.
 * 읽기 또는 검증에 실패하면 outDefinition을 유지하고 false를 반환한다.
 */
[[nodiscard]] bool ReadPrefabDefinition(const std::filesystem::path& absolutePath, PrefabDefinition& outDefinition);
