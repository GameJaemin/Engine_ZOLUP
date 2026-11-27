#include "CaughtCameraComponent.h"

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"

void CaughtCameraComponent::Begin(const Vector3& monster, const Vector3& player, float fallbackYaw)
{
	origin = monster;
	blendEntry = false;
	forward = player - monster;
	forward.y = 0.0f;
	const float length = PrototypeMath::Length(forward);
	forward = length > 1.0f ? forward / length : PrototypeMath::Direction(fallbackYaw);
	facingYaw = std::atan2(forward.x, forward.z) * 57.2957795f;
}

void CaughtCameraComponent::BlendFrom(const CameraComponent& camera)
{
	const auto* transform = camera.GetGameObject()->GetComponent<TransformComponent>();
	entryPosition = transform->GetPosition();
	entryRotation = transform->GetRotation();
	entryFov = camera.GetVerticalFovDegrees();
	blendEntry = true;
}

float CaughtCameraComponent::GetFade(float elapsed, const PrototypeConfig& config)
{
	return std::clamp((elapsed - config.caughtFadeStart) / config.caughtFadeTime, 0.0f, 1.0f);
}

void CaughtCameraComponent::Present(CameraComponent& camera, float elapsed, const PrototypeConfig& config, const std::optional<Vector3>& focus,
	const std::optional<CaughtCameraFraming>& framing)
{
	// 고정 틱 시각으로 계산해 일시정지나 프레임률 차이가 흔들림을 바꾸지 않게 한다.
	const float time = (std::max)(0.0f, elapsed);
	// 곰돌이 구도는 거리/화각을 고정하고 흔들림만 준다. 조준점 주변으로 제한해 몸이 화면에 남게 한다.
	const float push = framing ? 0.0f : 1.0f - std::exp(-time * 14.0f);
	const float envelope = (framing ? 0.8f + 0.2f * std::sin(time * 11.0f) : (0.35f + 0.65f * std::exp(-time * 2.0f))) * (1.0f - GetFade(time, config));
	const Vector3 right{ forward.z, 0.0f, -forward.x };
	const Vector3 target = focus.value_or(origin + Vector3{ 0.0f, config.caughtFaceHeight, 0.0f });
	const CaughtCameraFraming shot = framing.value_or(CaughtCameraFraming{config.caughtCameraDistance,config.caughtCameraDistance*0.72f,config.caughtFov,8.0f});
	Vector3 position = target + forward * (shot.distance + (shot.minimumDistance-shot.distance)*push);
	position += right * (config.caughtShakeStrength * envelope * (std::sin(time * 41.0f) + 0.35f * std::sin(time * 71.0f)));
	position.y += config.caughtShakeStrength * envelope * std::sin(time * 53.0f);
	const Vector3 look = target - position;
	TransformComponent* transform = camera.GetGameObject()->GetComponent<TransformComponent>();
	EulerAngles rotation{
		-std::atan2(look.y, (std::max)(0.01f, std::sqrt(look.x * look.x + look.z * look.z))) * 57.2957795f
			+ (framing ? config.caughtShakeAngle * 0.4f * envelope * std::sin(time * 37.0f) : 0.0f),
		std::atan2(look.x, look.z) * 57.2957795f
			+ (framing ? config.caughtShakeAngle * 0.55f * envelope * std::sin(time * 47.0f) : 0.0f),
		config.caughtShakeAngle * envelope * std::sin(time * 31.0f) };
	float fov = shot.fov - shot.fovTightening * push;
	if (blendEntry)
	{
		const float t = std::clamp(time / 0.22f, 0.0f, 1.0f);
		const float blend = t * t * (3.0f - 2.0f * t);
		position = entryPosition + (position - entryPosition) * blend;
		rotation = {PrototypeMath::ApproachAngle(entryRotation.pitch, rotation.pitch, blend),
			PrototypeMath::ApproachAngle(entryRotation.yaw, rotation.yaw, blend),
			PrototypeMath::ApproachAngle(entryRotation.roll, rotation.roll, blend)};
		fov = entryFov + (fov - entryFov) * blend;
	}
	transform->SetPosition(position);
	transform->SetRotation(rotation);
	camera.SetVerticalFovDegrees(fov);
}
