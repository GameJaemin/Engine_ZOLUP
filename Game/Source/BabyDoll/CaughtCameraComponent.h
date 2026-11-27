#pragma once

#include "PrototypeTypes.h"
#include "Component/Component.h"
#include "Core/Math/EulerAngles.h"
#include <optional>

class CameraComponent;

/** 모델 대신 구도 값만 전달한다. 거리의 기준은 호출자가 지정한 조준점이다. */
struct CaughtCameraFraming
{
	float distance = 420.0f;
	float minimumDistance = 380.0f;
	float fov = 54.0f;
	float fovTightening = 4.0f;
};

/** 잡힘 동안만 메인 카메라 출력을 소유한다. 게임 판정과 입력은 참조하지 않는다. */
class CaughtCameraComponent final : public Component
{
public:
	void Begin(const Vector3& monster, const Vector3& player, float fallbackYaw);
	void BlendFrom(const CameraComponent& camera);
	void Present(CameraComponent& camera, float elapsed, const PrototypeConfig& config, const std::optional<Vector3>& focus = std::nullopt,
		const std::optional<CaughtCameraFraming>& framing = std::nullopt);
	float GetFacingYaw() const { return facingYaw; }
	static float GetFade(float elapsed, const PrototypeConfig& config);

private:
	Vector3 origin;
	Vector3 forward;
	float facingYaw = 0.0f;
	Vector3 entryPosition;
	EulerAngles entryRotation;
	float entryFov = 66.0f;
	bool blendEntry = false;
};
