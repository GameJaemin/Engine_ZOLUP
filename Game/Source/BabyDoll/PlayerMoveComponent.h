#pragma once

#include "GameplaySpace.h"

#include "Component/Component.h"

/** 자동 Tick은 사용하지 않는다. 세션이 입력 -> 이동 -> 심박 순서로 Step을 호출한다. */
class PlayerMoveComponent final : public Component
{
public:
	void Reset(const Vector3& spawn);
	void Step(const PrototypeCommand& command, float viewYaw, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space, bool freeMove);
	void SetMode(PlayerMode newMode);
	void Teleport(const Vector3& destination);
	PlayerMode GetMode() const { return mode; }
	const Vector3& GetPosition() const { return position; }
	float GetSpeed() const { return actualSpeed; }
	float GetAttemptedSpeed() const { return PrototypeMath::Length(velocity); }
	float GetYaw() const { return bodyYaw; }
	void SetYaw(float yaw) { if(std::isfinite(yaw)) {bodyYaw=std::remainder(yaw,360.0f);} }
	float GetMomentum() const { return momentum; }
	int GetLastFoot() const { return lastFoot; }
	void RestoreFoot(int foot) { if (foot >= -1 && foot <= 1) { lastFoot = foot; sinceFoot = 10.0f; } }
	bool HasMistake() const { return mistake; }

private:
	Vector3 position;
	Vector3 velocity;
	PlayerMode mode = PlayerMode::Moving;
	float bodyYaw = 0.0f;
	float actualSpeed = 0.0f;
	float momentum = 0.0f;
	float sinceFoot = 10.0f;
	float stepInterval = 0.6f;
	float stateTime = 0.0f;
	int lastFoot = -1;
	bool mistake = false;
};
