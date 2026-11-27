#pragma once

#include "Core/Math/Vector3.h"

#include <algorithm>
#include <cmath>
#include <vector>

enum class StitchState { Patrol, Suspicious, Chase };
enum class StitchAction { Moving, Waiting, Listening, Looking, Roaring, Smashing };
enum class PlayerMode { Moving, Fallen, Hidden, Exiting, Caught };
enum class SoundKind { Foot, Heart, Mistake, Object };

struct PrototypeConfig
{
	float maxSpeed = 680.0f;
	float accelerationTime = 3.3f;
	float rhythmWindow = 0.75f;
	float fastInterval = 0.17f;
	float momentumDecay = 0.9f;
	float playerRadius = 30.0f;
	float playerHeight = 120.0f;
	float fallenTime = 1.5f;
	float patrolSpeed = 170.0f;
	float chaseSpeed = 400.0f;
	float suspiciousSpeed = 600.0f;
	float monsterAcceleration = 650.0f;
	float monsterBraking = 1100.0f;
	float monsterTurnRate = 220.0f;
	float monsterTurnAcceleration = 900.0f;
	float monsterLookAhead = 140.0f;
	// glTF +X 정면은 엔진 로더의 X 반전 뒤 -X가 되어 +90도 보정이 필요하다.
	float monsterVisualYawOffset = 90.0f;
	float catchDistance = 130.0f;
	float calmTime = 2.0f;
	float hideSuspicionRadius = 300.0f;
	float hideSuspicionTime = 2.0f;
	float searchTimeout = 3.0f;
	float smashTime = 1.4f;
	float roarTime = 2.0f;
	float listenTime = 1.3f;
	float lookTime = 2.2f;
	float patrolWait = 2.2f;
	float hideExitTime = 1.1f;
	float wrongSoundRadius = 4500.0f;
	float chaseStartDistance = 1000.0f;
	float mouseSensitivity = 0.13f;
	float cameraDistance = 430.0f;
	float runningCameraDistance = 560.0f;
	float cameraFov = 80.0f;
	float runningCameraFov = 96.0f;
	float cameraFollow = 16.0f;
	float cameraLookResponse = 35.0f;
	float rearWidth = 512.0f;
	float rearHeight = 288.0f;
	float rearSaturation = 0.35f;
	float rearHold = 5.0f;
	float rearFarDistance = 1500.0f;
	float rearNearDistance = 250.0f;
	float rearCameraDistance = 470.0f;
	float rearClosestDistance = 230.0f;
	float rearCameraHeight = 60.0f;
	float rearAimHeight = 45.0f;
	float rearHalfWidth = 260.0f;
	float rearHalfHeight = 230.0f;
	float rearCloseFill = 0.45f;
	float rearFollow = 10.0f;
	float lampDarkRadius = 1500.0f;
	float lampFlickerTime = 0.6f;
	float lampIntensity = 2.8f;
	float lampRange = 1250.0f;
	float propDropHeight = 380.0f;
	float caughtDuration = 1.8f;
	float caughtFaceHeight = 250.0f;
	float caughtCameraDistance = 70.0f;
	float caughtFov = 48.0f;
	float caughtShakeStrength = 2.2f;
	float caughtShakeAngle = 2.0f;
	float caughtFadeStart = 1.15f;
	float caughtFadeTime = 0.35f;
};

struct PrototypeCommand
{
	// 엔진이 기록한 순서대로 보관한다. 0은 왼발, 1은 오른발이다.
	std::vector<int> feet;
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	float forward = 0.0f;
	float right = 0.0f;
	bool run = false;
	bool interact = false;
	bool pulse = false;
};

/** 카메라 수명과 구도를 GPU 없이 검사할 수 있는 값 상태다. */
struct RearViewState
{
	float remaining = 0.0f;
	bool visible = false;
	void Step(bool chasing, float deltaTime, float hold)
	{
		remaining = chasing ? hold : (std::max)(0.0f, remaining - deltaTime);
		visible = chasing || remaining > 0.0f;
	}
	float GetPush(float distance, const PrototypeConfig& config) const
	{
		return std::clamp((config.rearFarDistance - distance) / (config.rearFarDistance - config.rearNearDistance), 0.0f, 1.0f);
	}
};

namespace PrototypeMath
{
	inline float Length(const Vector3& value) { return std::sqrt(value.Dot(value)); }
	inline float Distance(const Vector3& a, const Vector3& b) { return Length({ a.x - b.x, 0.0f, a.z - b.z }); }
	inline float Response(float rate, float deltaTime) { return 1.0f - std::exp(-rate * deltaTime); }
	inline Vector3 Direction(float yaw)
	{
		const float radians = yaw * 0.01745329252f;
		return { std::sin(radians), 0.0f, std::cos(radians) };
	}
	inline float ApproachAngle(float from, float to, float alpha) { return from + std::remainder(to - from, 360.0f) * alpha; }
	inline const char* StateName(StitchState state)
	{
		switch (state)
		{
		case StitchState::Patrol: return "PATROL";
		case StitchState::Suspicious: return "SUSPICIOUS";
		case StitchState::Chase: return "CHASE";
		}
		return "UNKNOWN";
	}
}

/** 실패 시 부분 적용하지 않고 기본값을 보존한다. JSON 경로는 저장소 루트 기준이다. */
bool LoadPrototypeConfig(PrototypeConfig& config);
