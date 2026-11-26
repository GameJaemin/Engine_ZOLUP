#include "StitchBrainComponent.h"

namespace
{
	constexpr Vector3 PatrolPoints[]{ { -1300.0f, 0.0f, 1000.0f }, { 1300.0f, 0.0f, 1000.0f }, { 1300.0f, 0.0f, -1000.0f }, { -1300.0f, 0.0f, -1000.0f } };
}

void StitchBrainComponent::Reset(const Vector3& spawn)
{
	movement.Reset(spawn, 180.0f);
	destination = PatrolPoints[0];
	state = StitchState::Patrol;
	action = StitchAction::Moving;
	stateTime = actionTime = hiddenTime = unreachableTime = 0.0f;
	patrolIndex = 0;
	resumeAfterRoar = brokeTable = caught = false;
}

void StitchBrainComponent::BeginAction(StitchAction nextAction)
{
	action = nextAction;
	actionTime = 0.0f;
}

void StitchBrainComponent::ChangeState(StitchState nextState)
{
	if (state == nextState) { return; }
	// 이전 상태의 경로와 관찰 타이머를 폐기하고 새 상태의 진입 조건을 설정한다.
	movement.InvalidatePath();
	hiddenTime = unreachableTime = stateTime = 0.0f;
	state = nextState;
	BeginAction(StitchAction::Moving);
	if (state == StitchState::Patrol) { destination = PatrolPoints[patrolIndex]; }
}

void StitchBrainComponent::Roar(bool resumeChase)
{
	resumeAfterRoar = resumeChase;
	BeginAction(StitchAction::Roaring);
}

void StitchBrainComponent::Step(const StitchObservation& observation, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space)
{
	brokeTable = caught = false;
	stateTime += deltaTime;
	actionTime += deltaTime;
	const float playerDistance = PrototypeMath::Distance(movement.GetPosition(), observation.playerPosition);
	if (action == StitchAction::Smashing)
	{
		movement.Stop(deltaTime, config, space);
		if (actionTime >= 1.4f) { brokeTable = true; Roar(true); }
		return;
	}
	if (action == StitchAction::Roaring)
	{
		movement.Stop(deltaTime, config, space);
		if (actionTime >= 2.0f)
		{
			if (resumeAfterRoar) { ChangeState(StitchState::Chase); BeginAction(StitchAction::Moving); }
			else { ChangeState(StitchState::Patrol); BeginAction(StitchAction::Moving); }
		}
		return;
	}
	if (observation.sound)
	{
		const SoundStimulus& sound = *observation.sound;
		const bool protectedHeartbeat = observation.hidden && sound.kind == SoundKind::Heart;
		if (!(protectedHeartbeat && state == StitchState::Suspicious))
		{
			const bool chase = sound.player && !(sound.kind == SoundKind::Mistake && playerDistance > 1000.0f);
			if (chase) { ChangeState(StitchState::Chase); }
			else if (state != StitchState::Chase) { ChangeState(StitchState::Suspicious); }
			destination = sound.position;
			if (action != StitchAction::Moving) { BeginAction(StitchAction::Moving); }
		}
	}
	else if (state == StitchState::Chase && !observation.hidden) { ChangeState(StitchState::Suspicious); }
	if (state == StitchState::Chase && observation.hidden)
	{
		hiddenTime = playerDistance <= 300.0f ? hiddenTime + deltaTime : 0.0f;
		const bool reached = movement.HasArrived();
		unreachableTime = reached ? unreachableTime + deltaTime : 0.0f;
		if (hiddenTime >= 2.0f || unreachableTime >= 3.0f)
		{
			if (observation.breakableTable) { movement.Stop(deltaTime, config, space); BeginAction(StitchAction::Smashing); return; }
			ChangeState(StitchState::Suspicious);
			BeginAction(StitchAction::Listening);
		}
	}
	// 숨기는 이미 들은 소리를 지우지 않는다. 목적지 조사 후에만 심박으로 수색 종료를 판단한다.
	if (state == StitchState::Suspicious && observation.hidden && action != StitchAction::Moving)
	{
		if (observation.heartBPM < 100.0f) { ChangeState(StitchState::Patrol); }
		else { movement.Stop(deltaTime, config, space); BeginAction(StitchAction::Listening); return; }
	}
	if (action == StitchAction::Moving)
	{
		float targetSpeed = state == StitchState::Patrol ? config.patrolSpeed : config.chaseSpeed;
		if (state == StitchState::Suspicious && playerDistance > 500.0f) { targetSpeed = config.suspiciousSpeed; }
		movement.Step(destination, targetSpeed, deltaTime, config, space);
		const bool arrived = movement.HasArrived();
		if (arrived)
		{
			if (state == StitchState::Patrol) { BeginAction(StitchAction::Waiting); }
			else if (state == StitchState::Suspicious) { BeginAction(StitchAction::Listening); }
		}
	}
	else
	{
		movement.Stop(deltaTime, config, space);
		if (action == StitchAction::Waiting && actionTime >= 2.2f)
		{
			patrolIndex = (patrolIndex + 1) % 4;
			destination = PatrolPoints[patrolIndex];
			movement.InvalidatePath();
			BeginAction(StitchAction::Moving);
		}
		else if (action == StitchAction::Listening && actionTime >= 1.3f) { BeginAction(StitchAction::Looking); }
		else if (action == StitchAction::Looking && actionTime >= 2.2f) { Roar(false); }
	}
	if (!observation.hidden && playerDistance <= config.catchDistance) { caught = true; }
}
