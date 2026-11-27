#include "OrbitCameraComponent.h"

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"

void OrbitCameraComponent::Reset(const Vector3& target, float initialYaw, const PrototypeConfig& config)
{
	view = { 12.0f, initialYaw, 0.0f };
	targetYaw = initialYaw;
	targetPitch = 12.0f;
	pivot = target;
	armLength = config.cameraDistance;
	fov = config.cameraFov;
	runBlend = 0.0f;
	fixedBlend = 0.0f;
}

void OrbitCameraComponent::BlendFixed(CameraComponent& camera, const std::optional<Vector3>& requested, const Vector3& target, float deltaTime, float requestedFov)
{
	if (requested)
	{
		fixedPosition = *requested;
		fixedFov=requestedFov;
		const Vector3 direction = target - fixedPosition;
		fixedRotation = { std::atan2(-direction.y, (std::max)(1.0f, PrototypeMath::Distance(target, fixedPosition))) * 57.2957795f,
			std::atan2(direction.x, direction.z) * 57.2957795f, 0.0f };
	}
	fixedBlend = std::clamp(fixedBlend + (requested ? 1.0f : -1.0f) * deltaTime / 0.5f, 0.0f, 1.0f);
	const float blend = fixedBlend * fixedBlend * (3.0f - 2.0f * fixedBlend);
	if (blend <= 0.0f) { return; }
	TransformComponent* transform = camera.GetGameObject()->GetComponent<TransformComponent>();
	transform->SetPosition(Vector3::Lerp(position, fixedPosition, blend));
	transform->SetRotation({view.pitch+(fixedRotation.pitch-view.pitch)*blend,PrototypeMath::ApproachAngle(view.yaw,fixedRotation.yaw,blend),0.0f});
	camera.SetVerticalFovDegrees(fov+(fixedFov-fov)*blend);
}

void OrbitCameraComponent::Look(float mouseX, float mouseY, float deltaTime, const PrototypeConfig& config)
{
	targetYaw = std::remainder(targetYaw + mouseX * config.mouseSensitivity, 360.0f);
	targetPitch = std::clamp(targetPitch + mouseY * config.mouseSensitivity, -45.0f, 65.0f);
	view.yaw = PrototypeMath::ApproachAngle(view.yaw, targetYaw, PrototypeMath::Response(config.cameraLookResponse, deltaTime));
	view.pitch += (targetPitch - view.pitch) * PrototypeMath::Response(config.cameraLookResponse, deltaTime);
}

void OrbitCameraComponent::Present(CameraComponent& camera, const Vector3& target, float speedRatio, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space, bool inspect)
{
	const float follow = PrototypeMath::Response(config.cameraFollow, deltaTime);
	pivot = Vector3::Lerp(pivot, target, follow);
	const Vector3 lag = pivot - target;
	const float lagLength = PrototypeMath::Length(lag);
	if (lagLength > 60.0f) { pivot = target + lag * (60.0f / lagLength); }
	const float yaw = view.yaw * 0.01745329252f;
	const float pitch = view.pitch * 0.01745329252f;
	const Vector3 backward{ -std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch) };
	// 기본 보행에서는 화각을 유지하고 달리기 구간에서만 구도를 넓힌다.
	float runTarget = std::clamp((speedRatio * config.maxSpeed - 300.0f) / (std::max)(1.0f, config.maxSpeed - 300.0f), 0.0f, 1.0f);
	runTarget = runTarget * runTarget * (3.0f - 2.0f * runTarget);
	runBlend += (runTarget - runBlend) * PrototypeMath::Response(6.0f, deltaTime);
	const Vector3 shoulderDirection = PrototypeMath::Direction(view.yaw + 90.0f);
	const float shoulderLength = inspect ? 0.0f : space.CameraDistance(pivot, shoulderDirection, 35.0f, 18.0f);
	const Vector3 shoulder = shoulderDirection * shoulderLength;
	const Vector3 origin = pivot + shoulder;
	const float desiredLength = inspect ? 780.0f : config.cameraDistance + (config.runningCameraDistance - config.cameraDistance) * runBlend;
	float available = space.CameraDistance(origin, backward, desiredLength, 18.0f);
	// 바닥도 카메라 구체 반경만큼 여유를 둔다.
	if (backward.y < -0.0001f) { available = (std::min)(available, (origin.y - 20.0f) / -backward.y); }
	available = (std::max)(0.0f, available);
	armLength = available < armLength ? available : armLength + (available - armLength) * PrototypeMath::Response(5.0f, deltaTime);
	position = origin + backward * armLength;
	TransformComponent* transform = camera.GetGameObject()->GetComponent<TransformComponent>();
	transform->SetPosition(position);
	transform->SetRotation(view);
	const float desiredFov = inspect ? 62.0f : config.cameraFov + (config.runningCameraFov - config.cameraFov) * runBlend;
	fov += (desiredFov - fov) * PrototypeMath::Response(4.0f, deltaTime);
	camera.SetVerticalFovDegrees(fov);
}
