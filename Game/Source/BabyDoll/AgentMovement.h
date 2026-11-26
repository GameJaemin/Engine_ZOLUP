#pragma once

#include "GameplaySpace.h"

/** AI 판단/모델과 독립인 평지 경로 추종기다. 단위는 cm, 초, 도다. */
class AgentMovement
{
public:
	void Reset(const Vector3& spawn, float facingYaw);
	void InvalidatePath();
	void Step(const Vector3& goal, float targetSpeed, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space);
	void Stop(float deltaTime, const PrototypeConfig& config, const GameplaySpace& space);
	const Vector3& GetPosition() const { return position; }
	const std::vector<Vector3>& GetPath() const { return path; }
	float GetYaw() const { return yaw; }
	float GetSpeed() const { return actualSpeed; }
	bool HasArrived() const { return arrived; }

private:
	void Turn(float targetYaw, float deltaTime, const PrototypeConfig& config);
	void Move(float targetSpeed, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space);
	void Plan(const Vector3& goal, const GameplaySpace& space);

private:
	Vector3 position;
	Vector3 plannedGoal;
	std::vector<Vector3> path;
	std::size_t pathIndex = 0;
	float yaw = 180.0f;
	float turnVelocity = 0.0f;
	float linearSpeed = 0.0f;
	float actualSpeed = 0.0f;
	float replanTime = 0.0f;
	bool pathValid = false;
	bool arrived = false;
};
