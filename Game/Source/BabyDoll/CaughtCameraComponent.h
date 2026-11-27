#pragma once

#include "PrototypeTypes.h"
#include "Component/Component.h"
#include <optional>

class CameraComponent;

/** 잡힘 동안만 메인 카메라 출력을 소유한다. 게임 판정과 입력은 참조하지 않는다. */
class CaughtCameraComponent final : public Component
{
public:
	void Begin(const Vector3& monster, const Vector3& player, float fallbackYaw);
	void Present(CameraComponent& camera, float elapsed, const PrototypeConfig& config, const std::optional<Vector3>& face = std::nullopt);
	float GetFacingYaw() const { return facingYaw; }
	static float GetFade(float elapsed, const PrototypeConfig& config);

private:
	Vector3 origin;
	Vector3 forward;
	float facingYaw = 0.0f;
};
