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
	float mouseSensitivity = 0.13f;
	float cameraDistance = 430.0f;
	float runningCameraDistance = 560.0f;
	float cameraFov = 80.0f;
	float runningCameraFov = 96.0f;
	float cameraFollow = 16.0f;
	float cameraLookResponse = 35.0f;
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
