#include "EngineComponentTypes.h"

#include "Component/AmbientLightComponent.h"
#include "Component/AnimationComponent.h"
#include "Component/BoxColliderComponent.h"
#include "Component/CameraComponent.h"
#include "Component/Component.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/GravityComponent.h"
#include "Component/InputComponent.h"
#include "Component/LightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/RigidBodyComponent.h"
#include "Component/SceneCapture2DComponent.h"
#include "Component/SkinComponent.h"
#include "Component/SkyboxComponent.h"
#include "Component/SpotLightComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "ComponentTypeRegistry.h"
// 에셋 참조 속성은 typeid로 에셋 타입을 구분하므로 전방 선언이 아닌 전체 정의가 필요하다. Model은 AssetManager.h에 있다.
#include "AssetManager.h"
#include "Mesh/StaticMesh.h"
#include "Renderer/Material.h"
#include "Texture/Texture.h"

#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <memory>

// 속성 표가 담을 수 있는 int로 컴포넌트 값을 바꿔 읽고 쓰는 함수들이다.
namespace
{
	/** 투영 모드를 int로 읽는다. 0은 Perspective, 1은 Orthographic이다. */
	int GetCameraProjectionMode(const CameraComponent& camera)
	{
		return static_cast<int>(camera.GetProjectionMode());
	}

	/** int 값을 투영 모드로 쓴다. SetProjectionMode는 값을 검사하지 않으므로 0과 1이 아니면 거부한다. */
	bool SetCameraProjectionMode(CameraComponent& camera, int value)
	{
		if (value != static_cast<int>(CameraProjectionMode::Perspective) && value != static_cast<int>(CameraProjectionMode::Orthographic)) { return false; }
		camera.SetProjectionMode(static_cast<CameraProjectionMode>(value));
		return true;
	}

	/** 메시 노드 번호를 int로 읽는다. */
	int GetSkinNodeIndex(const SkinComponent& skin)
	{
		return static_cast<int>(skin.GetNodeIndex());
	}

	/** int 값을 메시 노드 번호로 쓴다. 음수는 size_t로 바꾸면 아주 큰 번호가 되므로 거부한다. */
	bool SetSkinNodeIndex(SkinComponent& skin, int value)
	{
		if (value < 0) { return false; }
		skin.SetNodeIndex(static_cast<std::size_t>(value));
		return true;
	}

	/** 캡처 해상도를 int로 읽는다. 상한이 4096이라 int 범위를 넘지 않는다. */
	int GetCaptureWidth(const SceneCapture2DComponent& capture)
	{
		return static_cast<int>(capture.GetWidth());
	}

	/** int 값을 캡처 너비로 쓴다. 0 이하는 부호 없는 값으로 바꾸기 전에 거부한다. */
	bool SetCaptureWidth(SceneCapture2DComponent& capture, int value)
	{
		return value > 0 && capture.SetWidth(static_cast<std::uint32_t>(value));
	}

	int GetCaptureHeight(const SceneCapture2DComponent& capture)
	{
		return static_cast<int>(capture.GetHeight());
	}

	bool SetCaptureHeight(SceneCapture2DComponent& capture, int value)
	{
		return value > 0 && capture.SetHeight(static_cast<std::uint32_t>(value));
	}
}

bool RegisterEngineComponentTypes(ComponentTypeRegistry& registry)
{
	if (registry.Register<TransformComponent, Component>("TransformComponent") == nullptr) { return false; }
	if (!registry.AddField<TransformComponent>("position", &TransformComponent::GetPosition, &TransformComponent::SetPosition, { .dragSpeed = 1.0f })) { return false; }
	// 저장과 Play 복제는 내부 값인 쿼터니언을 그대로 옮기고, Inspector는 오일러 각으로 편집한다. 각도를 거쳐 저장하면 변환 오차가 생긴다.
	if (!registry.AddField<TransformComponent>("rotationQuaternion", &TransformComponent::GetRotationQuaternion, &TransformComponent::SetRotationQuaternion, { .visibility = ComponentFieldVisibility::Hidden })) { return false; }
	if (!registry.AddField<TransformComponent>("rotation", &TransformComponent::GetRotation, &TransformComponent::SetRotation, { .dragSpeed = 0.5f }, true)) { return false; }
	if (!registry.AddField<TransformComponent>("scale", &TransformComponent::GetScale, &TransformComponent::SetScale, { .dragSpeed = 0.01f })) { return false; }

	if (registry.Register<CameraComponent, Component>("CameraComponent") == nullptr) { return false; }
	if (!registry.AddField<CameraComponent>("verticalFovDegrees", &CameraComponent::GetVerticalFovDegrees, &CameraComponent::SetVerticalFovDegrees, { .displayName = "Vertical FOV (deg)", .dragSpeed = 0.5f, .minValue = 1.0f, .maxValue = 179.0f })) { return false; }
	if (!registry.AddField<CameraComponent>("orthographicHeight", &CameraComponent::GetOrthographicHeight, &CameraComponent::SetOrthographicHeight, { .displayName = "Orthographic Height (cm)", .dragSpeed = 10.0f, .minValue = 1.0f, .maxValue = FLT_MAX })) { return false; }
	// near와 far는 0 < near < far를 함께 검사하므로, 저장과 Play 복제는 한 번에 쓰는 clipDistances로 하고 Inspector는 near와 far를 따로 편집한다.
	// SetClipDistances는 (near, far)와 Vector2 두 가지가 있어 Vector2를 받는 것을 지정한다.
	if (!registry.AddField<CameraComponent>("clipDistances", &CameraComponent::GetClipDistances, static_cast<bool (CameraComponent::*)(const Vector2&)>(&CameraComponent::SetClipDistances), { .visibility = ComponentFieldVisibility::Hidden })) { return false; }
	if (!registry.AddField<CameraComponent>("nearClipDistance", &CameraComponent::GetNearClipDistance, &CameraComponent::SetNearClipDistance, { .displayName = "Near Clip (cm)", .dragSpeed = 0.1f, .minValue = 0.01f, .maxValue = FLT_MAX }, true)) { return false; }
	if (!registry.AddField<CameraComponent>("farClipDistance", &CameraComponent::GetFarClipDistance, &CameraComponent::SetFarClipDistance, { .displayName = "Far Clip (cm)", .dragSpeed = 100.0f }, true)) { return false; }
	if (!registry.AddField<CameraComponent>("projectionMode", &GetCameraProjectionMode, &SetCameraProjectionMode, { .enumNames = { "Perspective", "Orthographic" } })) { return false; }

	if (registry.Register<SceneCapture2DComponent, Component>("SceneCapture2DComponent") == nullptr) { return false; }
	if (!registry.AddField<SceneCapture2DComponent>("width", &GetCaptureWidth, &SetCaptureWidth, { .displayName = "Width (px)", .dragSpeed = 1.0f, .minValue = 1.0f, .maxValue = 4096.0f })) { return false; }
	if (!registry.AddField<SceneCapture2DComponent>("height", &GetCaptureHeight, &SetCaptureHeight, { .displayName = "Height (px)", .dragSpeed = 1.0f, .minValue = 1.0f, .maxValue = 4096.0f })) { return false; }
	if (!registry.AddField<SceneCapture2DComponent>("saturation", &SceneCapture2DComponent::GetSaturation, &SceneCapture2DComponent::SetSaturation, { .displayName = "Saturation", .dragSpeed = 0.01f, .minValue = 0.0f, .maxValue = 2.0f })) { return false; }

	if (registry.Register<StaticMeshComponent, Component>("StaticMeshComponent") == nullptr) { return false; }
	if (!registry.AddField<StaticMeshComponent>("mesh", &StaticMeshComponent::GetStaticMesh, &StaticMeshComponent::SetStaticMesh)) { return false; }
	if (!registry.AddField<StaticMeshComponent>("material", &StaticMeshComponent::GetMaterial, &StaticMeshComponent::SetMaterial)) { return false; }

	if (registry.Register<SkyboxComponent, Component>("SkyboxComponent") == nullptr) { return false; }
	if (!registry.AddField<SkyboxComponent>("enabled", &SkyboxComponent::IsEnabled, &SkyboxComponent::SetEnabled)) { return false; }

	// 면마다 에셋 참조 속성 하나로 등록한다.
	struct SkyboxFaceField { const char* name; SkyboxFace face; };
	constexpr std::array<SkyboxFaceField, 6> skyboxFaceFields = { {
		{ "facePositiveX", SkyboxFace::PositiveX }, { "faceNegativeX", SkyboxFace::NegativeX },
		{ "facePositiveY", SkyboxFace::PositiveY }, { "faceNegativeY", SkyboxFace::NegativeY },
		{ "facePositiveZ", SkyboxFace::PositiveZ }, { "faceNegativeZ", SkyboxFace::NegativeZ } } };
	for (const SkyboxFaceField& faceField : skyboxFaceFields)
	{
		const SkyboxFace face = faceField.face;
		const bool added = registry.AddField<SkyboxComponent>(faceField.name,
			[face](const SkyboxComponent& skybox) { return skybox.GetFace(face); },
			[face](SkyboxComponent& skybox, const std::shared_ptr<const Texture>& texture) { return skybox.SetFace(face, texture); });
		if (!added) { return false; }
	}

	if (registry.Register<AnimationComponent, Component>("AnimationComponent") == nullptr) { return false; }
	// 클립 노드 번호 순서의 노드 Transform 목록이다. 객체로 만들지 않은 뼈 노드는 가리키지 않는 원소다.
	if (!registry.AddField<AnimationComponent>("nodeTransforms", &AnimationComponent::GetNodeTransforms, &AnimationComponent::SetNodeTransforms, { .displayName = "Nodes" })) { return false; }
	// 노드 번호 순서의 원래 부모 기준 값이다. 노드 목록과 같은 순서와 길이다.
	if (!registry.AddField<AnimationComponent>("restPose", &AnimationComponent::GetRestPose, &AnimationComponent::SetRestPose, { .displayName = "Rest Pose" })) { return false; }

	if (registry.Register<SkinComponent, Component>("SkinComponent") == nullptr) { return false; }
	if (!registry.AddField<SkinComponent>("model", &SkinComponent::GetModel, &SkinComponent::SetModel)) { return false; }
	// 프리팹 구조로 정해지는 번호라 Inspector에서는 읽기 전용이다. Play 복제와 저장은 set으로 그대로 옮긴다.
	if (!registry.AddField<SkinComponent>("nodeIndex", &GetSkinNodeIndex, &SetSkinNodeIndex, { .visibility = ComponentFieldVisibility::ReadOnly })) { return false; }
	if (!registry.AddField<SkinComponent>("animation", &SkinComponent::GetAnimation, &SkinComponent::SetAnimation)) { return false; }
	if (registry.Register<InputComponent, Component>("InputComponent") == nullptr) { return false; }

	if (registry.Register<GravityComponent, Component>("GravityComponent") == nullptr) { return false; }
	if (!registry.AddField<GravityComponent>("gravityEnabled", &GravityComponent::IsGravityEnabled, &GravityComponent::SetGravityEnabled)) { return false; }

	if (registry.Register<BoxColliderComponent, Component>("BoxColliderComponent") == nullptr) { return false; }
	// 객체 원점 기준의 박스 중심과 반크기다. 반크기는 setter가 양수만 받는다.
	if (!registry.AddField<BoxColliderComponent>("localCenter", &BoxColliderComponent::GetLocalCenter, &BoxColliderComponent::SetLocalCenter, { .displayName = "Center (cm)", .dragSpeed = 1.0f })) { return false; }
	if (!registry.AddField<BoxColliderComponent>("localHalfExtents", &BoxColliderComponent::GetLocalHalfExtents, &BoxColliderComponent::SetLocalHalfExtents, { .displayName = "Half Extents (cm)", .dragSpeed = 1.0f, .minValue = 0.1f, .maxValue = FLT_MAX })) { return false; }

	if (registry.Register<RigidBodyComponent, Component>("RigidBodyComponent") == nullptr) { return false; }
	if (!registry.AddField<RigidBodyComponent>("simulatePhysics", &RigidBodyComponent::IsSimulatingPhysics, &RigidBodyComponent::SetSimulatePhysics, { .displayName = "Simulate Physics" })) { return false; }

	// 조명 4종은 부모 정보를 찾으므로 LightComponent를 먼저 등록한다. 공통 속성은 LightComponent에만 등록한다.
	if (registry.Register<LightComponent, Component>("LightComponent") == nullptr) { return false; }
	if (!registry.AddField<LightComponent>("color", &LightComponent::GetColor, &LightComponent::SetColor, { .isColor = true })) { return false; }
	if (!registry.AddField<LightComponent>("intensity", &LightComponent::GetIntensity, &LightComponent::SetIntensity, { .dragSpeed = 0.05f, .minValue = 0.0f, .maxValue = FLT_MAX })) { return false; }
	if (!registry.AddField<LightComponent>("enabled", &LightComponent::IsEnabled, &LightComponent::SetEnabled)) { return false; }

	if (registry.Register<DirectionalLightComponent, LightComponent>("DirectionalLightComponent") == nullptr) { return false; }

	if (registry.Register<PointLightComponent, LightComponent>("PointLightComponent") == nullptr) { return false; }
	if (!registry.AddField<PointLightComponent>("rangeCm", &PointLightComponent::GetRange, &PointLightComponent::SetRange, { .displayName = "Range (cm)", .dragSpeed = 10.0f, .minValue = 1.0f, .maxValue = FLT_MAX })) { return false; }

	if (registry.Register<SpotLightComponent, LightComponent>("SpotLightComponent") == nullptr) { return false; }
	if (!registry.AddField<SpotLightComponent>("rangeCm", &SpotLightComponent::GetRange, &SpotLightComponent::SetRange, { .displayName = "Range (cm)", .dragSpeed = 10.0f, .minValue = 1.0f, .maxValue = FLT_MAX })) { return false; }
	if (!registry.AddField<SpotLightComponent>("outerConeAngleDegrees", &SpotLightComponent::GetOuterConeAngleDegrees, &SpotLightComponent::SetOuterConeAngleDegrees, { .displayName = "Outer Cone Angle (deg)", .dragSpeed = 0.5f, .minValue = 0.1f, .maxValue = 90.0f })) { return false; }

	if (registry.Register<AmbientLightComponent, LightComponent>("AmbientLightComponent") == nullptr) { return false; }

	return true;
}
