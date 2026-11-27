#include "PlayerMoveComponent.h"

void PlayerMoveComponent::Reset(const Vector3& spawn)
{
	position = spawn;
	velocity = {};
	bodyYaw = actualSpeed = momentum = stateTime = 0.0f;
	sinceFoot = 10.0f;
	stepInterval = 0.6f;
	lastFoot = -1;
	mode = PlayerMode::Moving;
	mistake = false;
}

void PlayerMoveComponent::SetMode(PlayerMode newMode)
{
	mode = newMode;
	stepInterval = 0.6f;
	stateTime = 0.0f;
	velocity = {};
	actualSpeed = momentum = 0.0f;
	lastFoot = -1;
	sinceFoot = 10.0f;
}

void PlayerMoveComponent::Teleport(const Vector3& destination)
{
	position = destination;
	velocity = {};
	actualSpeed = 0.0f;
}

void PlayerMoveComponent::Step(const PrototypeCommand& command, float viewYaw, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space, bool freeMove)
{
	mistake = false;
	stateTime += deltaTime;
	sinceFoot += deltaTime;
	if (mode == PlayerMode::Fallen && stateTime >= config.fallenTime) { SetMode(PlayerMode::Moving); }
	if (mode != PlayerMode::Moving) { actualSpeed = 0.0f; return; }
	Vector3 direction = PrototypeMath::Direction(viewYaw);
	float targetSpeed = 0.0f;
	if (freeMove)
	{
		direction = direction * command.forward + PrototypeMath::Direction(viewYaw + 90.0f) * command.right;
		const float length = PrototypeMath::Length(direction);
		if (length > 0.001f) { direction = direction / length; targetSpeed = command.run ? config.maxSpeed : 200.0f; }
	}
	else
	{
		for (int foot : command.feet)
		{
			// 박자 콤보만 끊는다. 멈춰도 다음에 써야 할 발은 바뀌지 않는다.
			const bool freshRhythm = sinceFoot > config.rhythmWindow;
			if (freshRhythm) { momentum = 0.0f; }
			if (foot == lastFoot)
			{
				SetMode(PlayerMode::Fallen);
				mistake = true;
				return;
			}
			if(lastFoot>=0&&!freshRhythm) {stepInterval=std::clamp(sinceFoot,config.fastInterval,config.rhythmWindow);}
			const float quality = lastFoot < 0 || freshRhythm ? 0.65f : std::clamp(config.fastInterval / (std::max)(sinceFoot, config.fastInterval), 0.2f, 1.0f);
			momentum = std::clamp(momentum + config.fastInterval / config.accelerationTime * quality, 0.0f, 1.0f);
			lastFoot = foot;
			sinceFoot = 0.0f;
		}
		const float movementWindow=(std::min)(config.rhythmWindow,(std::max)(config.fastInterval*1.35f,stepInterval*1.2f));
		if (sinceFoot > movementWindow) { momentum = (std::max)(0.0f, momentum - config.momentumDecay * deltaTime); }
		targetSpeed = sinceFoot<=movementWindow ? momentum * config.maxSpeed : 0.0f;
		// 교대 입력이 이어지는 동안에는 가속 운동량과 별개로 기본 보행을 보장한다.
		// 입력이 끊기면 하한을 해제하고 기존 감속/속도 보간으로 멈춘다.
		if (lastFoot >= 0 && sinceFoot <= movementWindow)
		{
			targetSpeed = (std::max)(targetSpeed, (std::min)(config.minimumWalkSpeed, config.maxSpeed));
		}
	}
	// 조향은 현재 시야를 따르고 속도만 보간해 회전에 이중 지연을 주지 않는다.
	const float speed=PrototypeMath::Length(velocity);
	velocity = direction*(speed+(targetSpeed-speed)*PrototypeMath::Response(targetSpeed>0.0f?18.0f:20.0f,deltaTime));
	const Vector3 before = position;
	position = space.Move(position, velocity * deltaTime, config.playerRadius, config.playerHeight);
	actualSpeed = PrototypeMath::Distance(before, position) / deltaTime;
	if (actualSpeed > 2.0f)
	{
		const Vector3 movement = position - before;
		const float targetYaw = std::atan2(movement.x, movement.z) * 57.2957795f;
		bodyYaw = PrototypeMath::ApproachAngle(bodyYaw, targetYaw, PrototypeMath::Response(14.0f, deltaTime));
	}
}
