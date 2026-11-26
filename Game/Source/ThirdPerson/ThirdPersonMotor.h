#pragma once

#include "ThirdPersonScene.h"

#include "Core/Math/EulerAngles.h"
#include "Core/Math/MathUtility.h"

#include <algorithm>
#include <cmath>
#include <string>

struct ThirdPersonConfig
{
	std::string modelPath = "Game/Content/Characters/XBot/XBot.gltf";
	std::string idleClip = "Idle";
	std::string walkClip = "Walk";
	std::string runClip = "Run";
	// 엔진 좌표계: cm, cm/s, 도. 발바닥을 캐릭터 원점으로 사용한다.
	float walkSpeed = 160.0f;
	float runSpeed = 450.0f;
	float acceleration = 1100.0f;
	float braking = 1500.0f;
	float turnSpeed = 540.0f;
	float radius = 26.0f;
	float height = 178.0f;
	float stepHeight = 28.0f;
	float mouseSensitivity = 0.15f;
	float cameraDistance = 330.0f;
	float cameraHeight = 125.0f;
	float cameraFov = 60.0f;
	float visualYawOffset = 0.0f;
	float runPhaseOffset = 0.0f;
};

struct ThirdPersonInput
{
	float right = 0.0f;
	float forward = 0.0f;
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	bool running = false;
};

/** World나 입력 장치를 소유하지 않고 정적 씬 질의로 캐릭터 상태를 계산한다. */
class ThirdPersonMotor
{
public:
	explicit ThirdPersonMotor(const ThirdPersonConfig& newConfig = {})
		: config(newConfig)
	{
	}

	void Reset(const Vector3& footPosition, float yaw)
	{
		position = footPosition;
		velocity = {};
		speed = 0.0f;
		verticalSpeed = 0.0f;
		bodyYaw = yaw;
		view = { 12.0f, yaw, 0.0f };
		grounded = true;
	}

	/** deltaTime은 초다. 긴 프레임은 0.1초까지만 최대 1/120초 간격으로 적분한다. */
	void Update(const ThirdPersonInput& input, float deltaTime, const ThirdPersonScene& scene)
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) { return; }
		view.yaw = std::remainder(view.yaw + input.mouseX * config.mouseSensitivity, 360.0f);
		view.pitch = std::clamp(view.pitch + input.mouseY * config.mouseSensitivity, -65.0f, 65.0f);
		Vector3 movement{ input.right, 0.0f, input.forward };
		const float inputLength = movement.Length();
		if (inputLength > 1.0f)
		{
			movement = movement / inputLength;
		}
		const float yaw = Math::DegreesToRadians(view.yaw);
		const Vector3 forward{ std::sin(yaw), 0.0f, std::cos(yaw) };
		const Vector3 right{ std::cos(yaw), 0.0f, -std::sin(yaw) };
		const Vector3 desired = (forward * movement.z + right * movement.x) * (input.running ? config.runSpeed : config.walkSpeed);
		const float elapsed = (std::min)(deltaTime, 0.1f);
		const int steps = (std::max)(1, static_cast<int>(std::ceil(elapsed * 120.0f)));
		const float stepTime = elapsed / static_cast<float>(steps);
		const Vector3 previous = position;
		for (int step = 0; step < steps; ++step)
		{
			const Vector3 difference = desired - velocity;
			const float differenceLength = difference.Length();
			const float acceleration = desired.Length() > 0.001f ? config.acceleration : config.braking;
			if (differenceLength > 0.0001f)
			{
				velocity += difference * ((std::min)(differenceLength, acceleration * stepTime) / differenceLength);
			}
			Vector3 candidate = position + velocity * stepTime;
			float floor = 0.0f;
			if (grounded && scene.FindGround(candidate, position.y + config.stepHeight, position.y - 12.0f, floor))
			{
				candidate.y = floor + 0.05f;
				verticalSpeed = 0.0f;
			}
			else
			{
				grounded = false;
				verticalSpeed = (std::max)(verticalSpeed - 980.0f * stepTime, -2000.0f);
				candidate.y += verticalSpeed * stepTime;
				if (scene.FindGround(candidate, position.y + 0.1f, candidate.y - 1.0f, floor))
				{
					candidate.y = floor + 0.05f;
					verticalSpeed = 0.0f;
					grounded = true;
				}
			}
			Vector3 resolved = scene.Resolve(candidate, config.radius, config.height);
			// 캡슐이 좁은 틈이나 급경사에서 한 프레임에 크게 밀려나면 수평 이동을 취소한다.
			if (resolved.y - position.y > config.stepHeight + 2.0f || (resolved - candidate).Length() > config.radius)
			{
				resolved = position;
				velocity = {};
			}
			position = resolved;
			grounded = scene.FindGround(position, position.y + 0.1f, position.y - 3.0f, floor);
			if (grounded)
			{
				verticalSpeed = 0.0f;
			}
		}
		const Vector3 actual{ position.x - previous.x, 0.0f, position.z - previous.z };
		speed = actual.Length() / elapsed;
		if (speed > 2.0f)
		{
			const float desiredYaw = Math::RadiansToDegrees(std::atan2(actual.x, actual.z));
			const float difference = std::remainder(desiredYaw - bodyYaw, 360.0f);
			bodyYaw = std::remainder(bodyYaw + std::clamp(difference, -config.turnSpeed * elapsed, config.turnSpeed * elapsed), 360.0f);
		}
	}

	const Vector3& GetPosition() const { return position; }
	const EulerAngles& GetView() const { return view; }
	float GetBodyYaw() const { return bodyYaw; }
	float GetSpeed() const { return speed; }
	bool IsGrounded() const { return grounded; }

private:
	ThirdPersonConfig config;
	Vector3 position;
	Vector3 velocity;
	EulerAngles view;
	float bodyYaw = 0.0f;
	float verticalSpeed = 0.0f;
	float speed = 0.0f;
	bool grounded = false;
};
