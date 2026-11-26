#pragma once

#include "GameplaySpace.h"
#include "AgentMovement.h"
#include "HearingComponent.h"

#include "Component/Component.h"
#include <array>

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
	// 잠긴 출구 구역 안에 순찰 목표를 두지 않는다. 창고와 전시홀은 북쪽 문으로 연결된다.
	static constexpr std::array<Vector3, 4> PatrolRoute{{ {-1450.0f, 0.0f, 1000.0f}, {-1450.0f, 0.0f, -800.0f}, {0.0f, 0.0f, 1000.0f}, {300.0f, 0.0f, -1000.0f} }};
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
	std::optional<SoundStimulus> deferredSound;
};
