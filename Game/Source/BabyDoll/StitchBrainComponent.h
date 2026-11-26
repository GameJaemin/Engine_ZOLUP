#pragma once

#include "GameplaySpace.h"
#include "AgentMovement.h"
#include "HearingComponent.h"

#include "Component/Component.h"

struct StitchObservation
{
	Vector3 playerPosition;
	bool hidden = false;
	bool breakableTable = false;
	float heartBPM = 72.0f;
	std::optional<SoundStimulus> sound;
};

/** 판단과 이동 데이터만 소유한다. 모델/카메라/입력을 참조하지 않는다. */
class StitchBrainComponent final : public Component
{
public:
	void Reset(const Vector3& spawn);
	void ChangeState(StitchState nextState);
	void Step(const StitchObservation& observation, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space);
	StitchState GetState() const { return state; }
	StitchAction GetAction() const { return action; }
	const Vector3& GetPosition() const { return movement.GetPosition(); }
	const Vector3& GetDestination() const { return destination; }
	const std::vector<Vector3>& GetPath() const { return movement.GetPath(); }
	float GetYaw() const { return movement.GetYaw(); }
	float GetSpeed() const { return movement.GetSpeed(); }
	float GetStateTime() const { return stateTime; }
	bool DidBreakTable() const { return brokeTable; }
	bool HasCaughtPlayer() const { return caught; }

private:
	void BeginAction(StitchAction nextAction);
	void Roar(bool resumeChase);

private:
	StitchState state = StitchState::Patrol;
	StitchAction action = StitchAction::Moving;
	AgentMovement movement;
	Vector3 destination;
	int patrolIndex = 0;
	float stateTime = 0.0f;
	float actionTime = 0.0f;
	float hiddenTime = 0.0f;
	float unreachableTime = 0.0f;
	bool resumeAfterRoar = false;
	bool brokeTable = false;
	bool caught = false;
};
