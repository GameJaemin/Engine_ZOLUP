#include "RearCameraComponent.h"

#include "Component/CameraComponent.h"
#include "Component/SceneCapture2DComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "World.h"

SceneCapture2DComponent* RearCameraComponent::GetCapture()
{
	if (!GetGameObject()) { return nullptr; }
	GameObject* cameraObject = GetGameObject()->GetWorld().FindGameObject(cameraID);
	return cameraObject ? cameraObject->GetComponent<SceneCapture2DComponent>() : nullptr;
}

void RearCameraComponent::Clear()
{
	if (GetGameObject())
	{
		World& world = GetGameObject()->GetWorld();
		if (GameObject* cameraObject = world.FindGameObject(cameraID)) { world.DestroyGameObject(cameraObject); }
	}
	cameraID = {};
	state = {};
}

bool RearCameraComponent::Step(bool chasing, bool preview, const Vector3& player, const Vector3& monster, float monsterYaw, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space)
{
	state.Step(chasing || preview, deltaTime, config.rearHold);
	if (!state.visible) { Clear(); return true; }
	if (!GetGameObject()) { return false; }
	World& world = GetGameObject()->GetWorld();
	GameObject* cameraObject = world.FindGameObject(cameraID);
	const bool newCamera = cameraObject == nullptr;
	if (newCamera)
	{
		cameraObject = world.CreateGameObject("Rear CCTV", {});
		if (!cameraObject) { return false; }
		cameraID = cameraObject->GetObjectID();
		std::unique_ptr<CameraComponent> camera = std::make_unique<CameraComponent>();
		camera->SetClipDistances(5.0f, 8000.0f);
		if (!camera->SetPostProcessShaderPath("Game/Content/BabyDoll/Shaders/RearLowLight_PS.hlsl")) { Clear(); return false; }
		cameraObject->AddComponent(std::move(camera));
		std::unique_ptr<SceneCapture2DComponent> capture = std::make_unique<SceneCapture2DComponent>();
		if (!capture->SetWidth(static_cast<std::uint32_t>(config.rearWidth)) || !capture->SetHeight(static_cast<std::uint32_t>(config.rearHeight))) { Clear(); return false; }
		cameraObject->AddComponent(std::move(capture));
	}
	CameraComponent* camera = cameraObject->GetComponent<CameraComponent>();
	if (!camera || !camera->SetSaturation(config.rearSaturation)) { Clear(); return false; }
	Vector3 direction = player - monster;
	direction.y = 0.0f;
	const float distance = PrototypeMath::Length(direction);
	direction = distance > 1.0f ? direction / distance : PrototypeMath::Direction(monsterYaw);
	const float push = state.GetPush(distance, config);
	const float desiredLength = config.rearCameraDistance + (config.rearClosestDistance - config.rearCameraDistance) * push;
	const Vector3 target = monster + Vector3{ 0.0f, 148.0f + config.rearAimHeight, 0.0f };
	const Vector3 desired = monster + direction * desiredLength + Vector3{ 0.0f, 148.0f + config.rearCameraHeight, 0.0f };
	Vector3 fromTarget = desired - target;
	const float desiredDistance = PrototypeMath::Length(fromTarget);
	fromTarget = fromTarget / desiredDistance;
	float available = space.CameraDistance(target, fromTarget, desiredDistance, 12.0f);
	// 벽에 몰리면 옆 구도를 선택해 카메라가 얼굴 안으로 들어오는 것을 줄인다.
	if (available < desiredDistance * 0.7f)
	{
		const Vector3 alternatives[]{ { -direction.z, 0.0f, direction.x }, { direction.z, 0.0f, -direction.x }, direction * -1.0f };
		for (const Vector3& alternative : alternatives)
		{
			Vector3 candidate = alternative * desiredLength + Vector3{ 0.0f, config.rearCameraHeight - config.rearAimHeight, 0.0f };
			candidate = candidate / desiredDistance;
			const float room = space.CameraDistance(target, candidate, desiredDistance, 12.0f);
			if (room > available + 40.0f) { fromTarget = candidate; available = room; }
		}
	}
	const Vector3 safePosition = target + fromTarget * (std::max)(5.0f, available);
	cameraPosition = newCamera ? safePosition : Vector3::Lerp(cameraPosition, safePosition, PrototypeMath::Response(config.rearFollow, deltaTime));
	// 보간 중 장애물에 들어가지 않도록 최종 위치도 목표점에서 다시 검사한다.
	Vector3 offset = cameraPosition - target;
	float length = PrototypeMath::Length(offset);
	if (length > 0.001f) { cameraPosition = target + offset / length * space.CameraDistance(target, offset / length, length, 8.0f); }
	const float aspect = config.rearWidth / config.rearHeight;
	const float extent = (std::max)(config.rearHalfHeight, config.rearHalfWidth / aspect);
	const float fill = 1.0f + (config.rearCloseFill - 1.0f) * push;
	const float actualDistance = PrototypeMath::Length(cameraPosition - target);
	const float desiredFov = std::clamp(2.0f * std::atan(extent / ((std::max)(50.0f, actualDistance) * fill)) * 57.2957795f, 18.0f, 115.0f);
	cameraFov = newCamera ? desiredFov : cameraFov + (desiredFov - cameraFov) * PrototypeMath::Response(5.0f, deltaTime);
	const Vector3 look = target - cameraPosition;
	TransformComponent* transform = cameraObject->GetComponent<TransformComponent>();
	transform->SetPosition(cameraPosition);
	transform->SetRotation({ -std::atan2(look.y, (std::max)(0.01f, std::sqrt(look.x * look.x + look.z * look.z))) * 57.2957795f, std::atan2(look.x, look.z) * 57.2957795f, 0.0f });
	camera->SetVerticalFovDegrees(cameraFov);
	return true;
}
