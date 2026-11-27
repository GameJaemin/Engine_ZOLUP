#include "AgentMovement.h"

namespace
{
	constexpr float Radius = 60.0f;
	constexpr float Height = 296.0f;
	constexpr float ArrivalDistance = 18.0f;
}

void AgentMovement::Reset(const Vector3& spawn, float facingYaw)
{
	position = spawn;
	yaw = facingYaw;
	turnVelocity = linearSpeed = actualSpeed = replanTime = 0.0f;
	InvalidatePath();
}

void AgentMovement::InvalidatePath()
{
	path.clear();
	pathIndex = 0;
	pathValid = arrived = false;
	replanTime = 0.0f;
}

void AgentMovement::Plan(const Vector3& goal, const GameplaySpace& space)
{
	// 회전할 여유를 경로에 두되 실제 충돌 반지름은 유지한다.
	const std::vector<Vector3> raw = space.FindPath(position, goal, Radius + 12.0f, Height);
	path.clear();
	Vector3 anchor = position;
	std::size_t first = 0;
	while (first < raw.size())
	{
		std::size_t furthest = first;
		for (std::size_t candidate = first + 1; candidate < raw.size(); ++candidate)
		{
			if (space.CanTraverse(anchor, raw[candidate], Radius + 4.0f, Height)) { furthest = candidate; }
		}
		path.push_back(raw[furthest]);
		anchor = raw[furthest];
		first = furthest + 1;
	}
	pathIndex = 0;
	plannedGoal = goal;
	pathValid = true;
	replanTime = 0.4f;
	arrived = raw.empty() && PrototypeMath::Distance(position, goal) <= ArrivalDistance;
}

void AgentMovement::Turn(float targetYaw, float deltaTime, const PrototypeConfig& config)
{
	const float error = std::remainder(targetYaw - yaw, 360.0f);
	const float desiredRate = std::clamp(error * 6.0f, -config.monsterTurnRate, config.monsterTurnRate);
	const float change = config.monsterTurnAcceleration * deltaTime;
	turnVelocity += std::clamp(desiredRate - turnVelocity, -change, change);
	float rotation = turnVelocity * deltaTime;
	if (rotation * error >= 0.0f && std::abs(rotation) > std::abs(error)) { rotation = error; turnVelocity = 0.0f; }
	yaw = std::remainder(yaw + rotation, 360.0f);
}

void AgentMovement::Move(float targetSpeed, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space)
{
	const float rate = targetSpeed > linearSpeed ? config.monsterAcceleration : config.monsterBraking;
	linearSpeed += std::clamp(targetSpeed - linearSpeed, -rate * deltaTime, rate * deltaTime);
	const Vector3 displacement = PrototypeMath::Direction(yaw) * (linearSpeed * deltaTime);
	const Vector3 before = position;
	position = space.Move(position, displacement, Radius, Height);
	actualSpeed = PrototypeMath::Distance(before, position) / deltaTime;
	if (actualSpeed < linearSpeed * 0.5f && linearSpeed > 10.0f)
	{
		linearSpeed = actualSpeed;
		pathValid = false;
	}
}

void AgentMovement::Stop(float deltaTime, const PrototypeConfig& config, const GameplaySpace& space)
{
	turnVelocity = 0.0f;
	Move(0.0f, deltaTime, config, space);
}

void AgentMovement::Step(const Vector3& goal, float targetSpeed, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space)
{
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) { return; }
	replanTime -= deltaTime;
	const float goalChange = PrototypeMath::Length(plannedGoal - goal);
	if (!pathValid || (replanTime <= 0.0f && goalChange > 60.0f)) { Plan(goal, space); }
	// 같은 목적지의 경로를 주기적으로 초기화하지 않아 격자점 뒤로 되돌아가지 않는다.
	if (!path.empty() && space.IsFree(goal, Radius + 12.0f, Height) && space.CanTraverse(path.back(), goal, Radius + 4.0f, Height)) { path.back() = goal; }
	while (pathIndex + 1 < path.size() && space.CanTraverse(position, path[pathIndex + 1], Radius + 4.0f, Height)) { ++pathIndex; }
	if (pathIndex >= path.size()) { Stop(deltaTime, config, space); return; }
	Vector3 aim = path[pathIndex];
	float remaining = PrototypeMath::Distance(position, aim);
	for (std::size_t index = pathIndex + 1; index < path.size(); ++index) { remaining += PrototypeMath::Distance(path[index - 1], path[index]); }
	if (pathIndex + 1 < path.size())
	{
		const float distance = PrototypeMath::Distance(position, aim);
		if (distance < config.monsterLookAhead)
		{
			const Vector3 next = path[pathIndex + 1] - aim;
			const float length = PrototypeMath::Length(next);
			if (length > 0.001f)
			{
				const Vector3 ahead = aim + next * ((std::min)(config.monsterLookAhead - distance, length) / length);
				if (space.CanTraverse(position, ahead, Radius + 4.0f, Height)) { aim = ahead; }
			}
		}
	}
	const Vector3 difference = aim - position;
	if (PrototypeMath::Length(difference) > 0.1f) { Turn(std::atan2(difference.x, difference.z) * 57.2957795f, deltaTime, config); }
	const float heading = std::atan2(difference.x, difference.z) * 57.2957795f;
	const float angle = std::abs(std::remainder(heading - yaw, 360.0f));
	const float alignment = std::clamp((std::cos(angle * 0.01745329252f) - 0.15f) / 0.85f, 0.0f, 1.0f);
	const float arrivalSpeed = std::sqrt(2.0f * config.monsterBraking * (std::max)(0.0f, remaining - ArrivalDistance));
	Move((std::min)(targetSpeed, arrivalSpeed) * alignment, deltaTime, config, space);
	if (PrototypeMath::Distance(position, path[pathIndex]) < 25.0f && pathIndex + 1 < path.size()) { ++pathIndex; }
	arrived = pathIndex + 1 == path.size() && PrototypeMath::Distance(position, path.back()) <= ArrivalDistance + 3.0f && actualSpeed < 10.0f;
}
