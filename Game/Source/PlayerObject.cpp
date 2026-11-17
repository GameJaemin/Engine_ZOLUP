#include "PlayerObject.h"

#include "Component/CameraComponent.h"
#include "Component/InputComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Math/MathUtility.h"
#include "Input/Key.h"
#include "Input/KeyBinding.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <utility>

namespace
{
	// 단위는 cm/s다. 1 unit = 1cm 기준으로 초당 3m 이동한다.
	constexpr float MoveSpeed = 300.0f;
	// 초당 회전 각도와 pitch의 최대 절댓값을 도 단위로 지정한다.
	constexpr float LookSpeed = 90.0f;
	constexpr float MaxPitch = 89.0f;
	// 마우스가 1픽셀 움직일 때 회전하는 각도다.
	constexpr float MouseLookSensitivity = 0.15f;
}

PlayerObject::PlayerObject(World& targetWorld)
	: GameObject(targetWorld)
{
	transform = GetComponent<TransformComponent>();
	viewRotation = transform->GetRotation();

	std::unique_ptr<InputComponent> newInput = std::make_unique<InputComponent>();
	InputComponent* input = newInput.get();
	// 부착 시 OnRegister가 호출되어 소속 World의 전달자에 추가된다.
	AddComponent(std::move(newInput));

	input->BindKey(Key::W, KeyTrigger::Held, [this](float) { moveInput.z += 1.0f; });
	input->BindKey(Key::S, KeyTrigger::Held, [this](float) { moveInput.z -= 1.0f; });
	input->BindKey(Key::A, KeyTrigger::Held, [this](float) { moveInput.x -= 1.0f; });
	input->BindKey(Key::D, KeyTrigger::Held, [this](float) { moveInput.x += 1.0f; });
	input->BindKey(Key::E, KeyTrigger::Held, [this](float) { moveInput.y += 1.0f; });
	input->BindKey(Key::Q, KeyTrigger::Held, [this](float) { moveInput.y -= 1.0f; });

	input->BindKey(Key::Left, KeyTrigger::Held, [this](float) { yawInput -= 1.0f; });
	input->BindKey(Key::Right, KeyTrigger::Held, [this](float) { yawInput += 1.0f; });
	// 양수 pitch는 카메라의 전방을 아래로 돌린다.
	input->BindKey(Key::Up, KeyTrigger::Held, [this](float) { pitchInput -= 1.0f; });
	input->BindKey(Key::Down, KeyTrigger::Held, [this](float) { pitchInput += 1.0f; });

	input->BindKey(Key::Space, KeyTrigger::Pressed, [this](float) { PrintPosition("Space Pressed"); });
	input->BindKey(Key::Space, KeyTrigger::Released, [this](float) { PrintPosition("Space Released"); });

	// 플레이어 위치와 회전을 그대로 쓴다. 활성 지정은 World 등록 뒤 GameApplication이 한다.
	AddComponent(std::make_unique<CameraComponent>());
}

void PlayerObject::AddMouseLookInput(int deltaX, int deltaY)
{
	mouseYawDeltaDegrees += static_cast<float>(deltaX) * MouseLookSensitivity;
	mousePitchDeltaDegrees += static_cast<float>(deltaY) * MouseLookSensitivity;
}

void PlayerObject::OnTick(float deltaTime)
{
	const Vector3 frameMove = std::exchange(moveInput, Vector3{});
	const float frameYaw = std::exchange(yawInput, 0.0f);
	const float framePitch = std::exchange(pitchInput, 0.0f);
	// 방향키 회전은 누른 시간에 비례하고 마우스 회전은 이미 픽셀 이동량으로 계산했다.
	const float lookDelta = LookSpeed * deltaTime;
	const float yawDelta = frameYaw * lookDelta + std::exchange(mouseYawDeltaDegrees, 0.0f);
	const float pitchDelta = framePitch * lookDelta + std::exchange(mousePitchDeltaDegrees, 0.0f);
	const bool hasMove = frameMove.x != 0.0f || frameMove.y != 0.0f || frameMove.z != 0.0f;
	const bool hasLook = yawDelta != 0.0f || pitchDelta != 0.0f;
	if (!hasMove && !hasLook) { return; }

	if (hasLook)
	{
		viewRotation.yaw = std::remainder(viewRotation.yaw + yawDelta, 360.0f);
		viewRotation.pitch = std::clamp(viewRotation.pitch + pitchDelta, -MaxPitch, MaxPitch);
		transform->SetRotation(viewRotation);
	}
	if (!hasMove) { return; }

	// XZ 평면에서 yaw만 적용해 위아래를 볼 때도 WASD로 고도가 바뀌지 않게 한다.
	const float yawRadians = Math::DegreesToRadians(viewRotation.yaw);
	const float sinYaw = std::sin(yawRadians);
	const float cosYaw = std::cos(yawRadians);
	const Vector3 forward{ sinYaw, 0.0f, cosYaw };
	const Vector3 right{ cosYaw, 0.0f, -sinYaw };
	const Vector3 up{ 0.0f, 1.0f, 0.0f };
	const Vector3 direction = forward * frameMove.z + right * frameMove.x + up * frameMove.y;
	transform->SetPosition(transform->GetPosition() + direction * (MoveSpeed * deltaTime));
}

void PlayerObject::PrintPosition(const char* label) const
{
	const Vector3 position = transform->GetPosition();
	std::printf("%s: (%.1f, %.1f, %.1f) cm\n", label, position.x, position.y, position.z);
}
