#pragma once

#include "GameplaySpace.h"

#include "Component/Component.h"
#include "Core/Math/EulerAngles.h"

class CameraComponent;

/** 마우스는 프레임 변위다. deltaTime을 곱하지 않고 목표 각도에 누적한다. */
class OrbitCameraComponent final : public Component
{
public:
	void Reset(const Vector3& target, float initialYaw, const PrototypeConfig& config);
	void Look(float mouseX, float mouseY, float deltaTime, const PrototypeConfig& config);
	void Present(CameraComponent& camera, const Vector3& target, float speedRatio, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space, bool inspect);
	float GetYaw() const { return view.yaw; }
	const Vector3& GetPosition() const { return position; }
	float GetFov() const { return fov; }
	void BlendFixed(CameraComponent& camera, const std::optional<Vector3>& fixedPosition, const Vector3& target, float deltaTime, float fixedFov=65.0f, const GameplaySpace* space=nullptr, float framingHeight=0.0f);

private:
	EulerAngles view{ 12.0f, 0.0f, 0.0f };
	float targetYaw = 0.0f;
	float targetPitch = 12.0f;
	Vector3 pivot;
	Vector3 position;
	float armLength = 430.0f;
	float fov = 80.0f;
	float runBlend = 0.0f;
	float fixedBlend = 0.0f;
	float fixedArmLength = 0.0f;
	Vector3 fixedPosition;
	EulerAngles fixedRotation;
	float fixedFov=65.0f;
};
