#include "StitchBrainComponent.h"

bool StitchBrainComponent::RestartPatrol(const Vector3& savedPosition, const Vector3& playerPosition, const PrototypeConfig& config, const GameplaySpace& space)
{
	// 최소 2초의 이동 여유와 심박 공포 반경을 확보한다. 충돌/바닥 검사를 통과한 위치만 사용한다.
	const float clearance = (std::max)(config.heartFearRadius, config.catchDistance + config.chaseSpeed * 2.0f);
	const auto valid = [&](const Vector3& point)
	{
		const auto ground = space.GroundHeight(point, point.y);
		return ground && std::abs(*ground-point.y)<1.0f && space.IsFree(point,72.0f,296.0f)
			&& PrototypeMath::Distance(point,playerPosition)>=clearance;
	};
	std::optional<Vector3> spawn;
	if (valid(savedPosition)) { spawn=savedPosition; }
	else
	{
		const auto choose = [&](const auto& route)
		{
			for (const auto& point : route)
			{
				if (valid(point) && (!spawn || PrototypeMath::Distance(point,playerPosition)>PrototypeMath::Distance(*spawn,playerPosition))) {spawn=point;}
			}
		};
		// 가능하면 저장 당시 층의 순찰 지점을 사용한다.
		if (savedPosition.y>=640.0f) { choose(UpperPatrolRoute); } else { choose(PatrolRoute); }
		if (!spawn) { choose(PatrolRoute); choose(UpperPatrolRoute); }
	}
	if (!spawn) { return false; }
	Reset(*spawn);
	if (spawn->y>=640.0f) { destination=UpperPatrolRoute[0]; }
	return true;
}

void StitchBrainComponent::Reset(const Vector3& spawn)
{
	movement.Reset(spawn, 180.0f);
	destination = PatrolRoute[0];
	state = StitchState::Patrol;
	action = StitchAction::Moving;
	stateTime = actionTime = hiddenTime = unreachableTime = 0.0f;
	patrolIndex = 0;
	resumeAfterRoar = brokeTable = caught = false;
	deferredSound.reset();
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
	if (state == StitchState::Patrol) { destination = PatrolRoute[patrolIndex]; }
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
	const int playerFloor = static_cast<int>(std::floor((observation.playerPosition.y + 1.0f) / 640.0f));
	const int ownFloor = static_cast<int>(std::floor((movement.GetPosition().y + 1.0f) / 640.0f));
	const bool sameFloor = playerFloor == ownFloor;
	if(!sameFloor && state!=StitchState::Patrol && (action==StitchAction::Waiting||action==StitchAction::Listening||action==StitchAction::Looking)) {BeginAction(StitchAction::Moving);}
	if (state == StitchState::Chase && action != StitchAction::Roaring && (observation.inCrawl || (!sameFloor && observation.onLadder)))
	{
		ChangeState(StitchState::Patrol);
		Roar(false);
	}
	if (!observation.hidden) { hiddenTime = unreachableTime = 0.0f; }
	if ((action == StitchAction::Smashing || action == StitchAction::Roaring) && observation.sound
		&& !(state == StitchState::Suspicious && observation.hidden && observation.sound->kind == SoundKind::Heart))
	{
		const auto& sound = *observation.sound;
		if (!deferredSound || sound.player > deferredSound->player || (sound.player == deferredSound->player && sound.sequence >= deferredSound->sequence)) { deferredSound = sound; }
	}
	if (action == StitchAction::Smashing)
	{
		movement.Stop(deltaTime, config, space);
		if (actionTime >= config.smashTime) { brokeTable = true; Roar(true); }
		return;
	}
	if (action == StitchAction::Roaring)
	{
		movement.Stop(deltaTime, config, space);
		if (actionTime >= config.roarTime)
		{
			if (resumeAfterRoar) { ChangeState(StitchState::Chase); BeginAction(StitchAction::Moving); }
			else { ChangeState(StitchState::Patrol); BeginAction(StitchAction::Moving); }
		}
		else { return; }
	}
	std::optional<SoundStimulus> heard = observation.sound;
	if (deferredSound)
	{
		if (!heard || deferredSound->player > heard->player || (deferredSound->player == heard->player && deferredSound->sequence > heard->sequence)) { heard = deferredSound; }
		deferredSound.reset();
	}
	if(observation.inCrawl&&heard&&heard->player) {heard.reset();}
	if (heard)
	{
		const SoundStimulus& sound = *heard;
		const bool protectedHeartbeat = observation.hidden && sound.kind == SoundKind::Heart;
		if (!(protectedHeartbeat && state == StitchState::Suspicious))
		{
			const bool chase = sound.reaction ? *sound.reaction == StitchState::Chase : sound.player && !(sound.kind == SoundKind::Mistake && PrototypeMath::Distance(movement.GetPosition(), sound.position) > config.chaseStartDistance);
			if (chase) { ChangeState(StitchState::Chase); }
			else if (state != StitchState::Chase) { ChangeState(StitchState::Suspicious); }
			destination = sound.position;
			if(observation.hidden&&!observation.inCrawl)
			{
				const auto ground=space.GroundHeight(sound.position,static_cast<float>(playerFloor)*640.0f);
				if(ground) {destination.y=*ground;}
			}
			if (action != StitchAction::Moving) { BeginAction(StitchAction::Moving); }
		}
	}
	else if (state == StitchState::Chase && !observation.hidden) { ChangeState(StitchState::Suspicious); }
	if (state == StitchState::Chase && observation.hidden && sameFloor)
	{
		hiddenTime = playerDistance <= config.hideSuspicionRadius ? hiddenTime + deltaTime : 0.0f;
		const bool reached = movement.HasArrived();
		unreachableTime = reached ? unreachableTime + deltaTime : 0.0f;
		if (hiddenTime >= config.hideSuspicionTime || unreachableTime >= config.searchTimeout)
		{
			if (observation.breakableTable && hiddenTime >= config.hideSuspicionTime) { movement.Stop(deltaTime, config, space); BeginAction(StitchAction::Smashing); return; }
			ChangeState(StitchState::Suspicious);
			BeginAction(StitchAction::Listening);
		}
	}
	// 숨기는 이미 들은 소리를 지우지 않는다. 목적지 조사 후에만 심박으로 수색 종료를 판단한다.
	if (state == StitchState::Suspicious && observation.hidden && action != StitchAction::Moving)
	{
		if (observation.heartBPM < config.heartAudible) { ChangeState(StitchState::Patrol); }
		else { movement.Stop(deltaTime, config, space); BeginAction(StitchAction::Listening); return; }
	}
	if (action == StitchAction::Moving)
	{
		if (!sameFloor && state != StitchState::Patrol) { destination = observation.playerPosition; }
		else if (state == StitchState::Patrol && ownFloor == 1)
		{
			destination = UpperPatrolRoute[patrolIndex];
		}
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
		if (action == StitchAction::Waiting && actionTime >= config.patrolWait)
		{
			patrolIndex = (patrolIndex + 1) % 4;
			destination = PatrolRoute[patrolIndex];
			movement.InvalidatePath();
			BeginAction(StitchAction::Moving);
		}
		else if (action == StitchAction::Listening && actionTime >= config.listenTime) { BeginAction(StitchAction::Looking); }
		else if (action == StitchAction::Looking && actionTime >= config.lookTime) { Roar(false); }
	}
	if (sameFloor && !observation.hidden && playerDistance <= config.catchDistance) { caught = true; }
}
