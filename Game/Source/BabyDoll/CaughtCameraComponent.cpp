#include "CaughtCameraComponent.h"

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"

void CaughtCameraComponent::Begin(const Vector3& monster, const Vector3& player, float fallbackYaw)
{
	origin = monster;
	forward = player - monster;
	forward.y = 0.0f;
	const float length = PrototypeMath::Length(forward);
	forward = length > 1.0f ? forward / length : PrototypeMath::Direction(fallbackYaw);
	facingYaw = std::atan2(forward.x, forward.z) * 57.2957795f;
}

float CaughtCameraComponent::GetFade(float elapsed, const PrototypeConfig& config)
{
	return std::clamp((elapsed - config.caughtFadeStart) / config.caughtFadeTime, 0.0f, 1.0f);
}

void CaughtCameraComponent::Present(CameraComponent& camera, float elapsed, const PrototypeConfig& config, const std::optional<Vector3>& face)
{
	// 고정 틱 시각으로 계산해 일시정지나 프레임률 차이가 흔들림을 바꾸지 않게 한다.
	const float time = (std::max)(0.0f, elapsed);
	const float push = 1.0f - std::exp(-time * 14.0f);
	const float envelope = (0.35f + 0.65f * std::exp(-time * 2.0f)) * (1.0f - GetFade(time, config));
	const Vector3 right{ forward.z, 0.0f, -forward.x };
	const Vector3 target = face.value_or(origin + Vector3{ 0.0f, config.caughtFaceHeight, 0.0f });
	Vector3 position = target + forward * (config.caughtCameraDistance * (1.0f - 0.28f * push));
	position += right * (config.caughtShakeStrength * envelope * (std::sin(time * 107.0f) + 0.35f * std::sin(time * 193.0f)));
	position.y += config.caughtShakeStrength * envelope * std::sin(time * 139.0f);
	const Vector3 look = target - position;
	TransformComponent* transform = camera.GetGameObject()->GetComponent<TransformComponent>();
	transform->SetPosition(position);
	transform->SetRotation({
		-std::atan2(look.y, (std::max)(0.01f, std::sqrt(look.x * look.x + look.z * look.z))) * 57.2957795f,
		std::atan2(look.x, look.z) * 57.2957795f,
		config.caughtShakeAngle * envelope * std::sin(time * 83.0f) });
	camera.SetVerticalFovDegrees(config.caughtFov - 8.0f * push);
}
