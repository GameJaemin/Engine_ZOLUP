#include "EditorCameraController.h"

#include "Core/Math/EulerAngles.h"
#include "Core/Math/Matrix4x4.h"
#include "EditorCamera.h"
#include "Input/InputState.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace
{
	// 위아래로 넘어가 화면이 뒤집히지 않도록 pitch를 제한한다.
	constexpr float MaxPitch = 89.0f;
	// 직교 줌으로 조절할 수 있는 세로 범위의 최솟값과 최댓값(cm)이다.
	constexpr float MinOrthographicHeight = 1.0f;
	constexpr float MaxOrthographicHeight = 100000.0f;

	/** 이번 프레임의 Raw 이동량으로 회전을 계산한다. 회전할 것이 없으면 nullopt다. */
	std::optional<Quaternion> CalculateLookRotation(const Quaternion& currentRotation, const InputState& input, float mouseSensitivity)
	{
		const RawMouseDelta mouseDelta = input.GetRawMouseDelta();
		if (mouseDelta.x == 0 && mouseDelta.y == 0) { return std::nullopt; }

		// 우클릭을 새로 시작한 프레임에는 클릭 전 이동량이 섞일 수 있다.
		const std::vector<KeyEvent>& frameEvents = input.GetFrameEvents();
		const bool startedMouseLook = std::any_of(frameEvents.begin(), frameEvents.end(), [](const KeyEvent& event)
		{
			return event.key == Key::MouseRight && event.type == KeyEventType::Pressed;
		});
		if (startedMouseLook) { return std::nullopt; }

		EulerAngles angles = currentRotation.ToEulerAngles();
		angles.yaw = std::remainder(angles.yaw + static_cast<float>(mouseDelta.x) * mouseSensitivity, 360.0f);
		angles.pitch = std::clamp(angles.pitch + static_cast<float>(mouseDelta.y) * mouseSensitivity, -MaxPitch, MaxPitch);
		angles.roll = 0.0f;
		return Quaternion::CreateFromEulerAngles(angles);
	}

	Vector3 CalculateMoveDelta(const InputState& input, const Vector3& forward, const Vector3& right, float deltaTime, float moveSpeed)
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) { return {}; }

		const float forwardInput = static_cast<float>(input.IsKeyDown(Key::W)) - static_cast<float>(input.IsKeyDown(Key::S));
		const float rightInput = static_cast<float>(input.IsKeyDown(Key::D)) - static_cast<float>(input.IsKeyDown(Key::A));
		const float upInput = static_cast<float>(input.IsKeyDown(Key::E)) - static_cast<float>(input.IsKeyDown(Key::Q));
		const Vector3 direction = forward * forwardInput + right * rightInput + Vector3{ 0.0f, upInput, 0.0f };
		const float directionLengthSquared = direction.Dot(direction);
		if (directionLengthSquared <= 0.0f) { return {}; }

		const Vector3 normalizedDirection = direction / std::sqrt(directionLengthSquared);
		return normalizedDirection * (moveSpeed * deltaTime);
	}
}

void EditorCameraController::Update(EditorCamera& camera, const InputState& input, float deltaTime, bool allowInput) const
{
	if (!allowInput || !input.IsKeyDown(Key::MouseRight)) { return; }

	const CameraViewInfo view = camera.GetCameraViewInfo();
	const std::optional<Quaternion> lookRotation = CalculateLookRotation(view.rotation, input, mouseSensitivity);
	if (lookRotation.has_value())
	{
		camera.SetRotation(*lookRotation);
	}

	// 이동 방향은 이번 프레임에 회전한 뒤의 방향을 쓴다.
	const Matrix4x4 rotation = Matrix4x4::CreateFromQuaternion(lookRotation.value_or(view.rotation));
	const Vector3 right{ rotation.m[0][0], rotation.m[0][1], rotation.m[0][2] };
	const Vector3 forward{ rotation.m[2][0], rotation.m[2][1], rotation.m[2][2] };
	Vector3 positionDelta = CalculateMoveDelta(input, forward, right, deltaTime, moveSpeed);

	const float wheelDelta = input.GetMouseWheelDelta();
	if (wheelDelta != 0.0f)
	{
		if (view.projectionMode == CameraProjectionMode::Perspective)
		{
			positionDelta += forward * (wheelDelta * dollyDistancePerNotch);
		}
		else
		{
			const float zoomFactor = std::pow(orthographicZoomFactorPerNotch, wheelDelta);
			const float newHeight = std::clamp(view.orthographicHeight * zoomFactor, MinOrthographicHeight, MaxOrthographicHeight);
			camera.SetOrthographicHeight(newHeight);
		}
	}

	if (positionDelta.x != 0.0f || positionDelta.y != 0.0f || positionDelta.z != 0.0f)
	{
		camera.SetPosition(view.position + positionDelta);
	}
}
