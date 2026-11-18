#include "EditorCameraController.h"

#include "Core/Math/Matrix4x4.h"
#include "EditorCamera.h"
#include "Input/InputState.h"

#include <algorithm>
#include <cmath>

namespace
{
	// 직교 줌으로 조절할 수 있는 세로 범위의 최솟값과 최댓값(cm)이다.
	constexpr float MinOrthographicHeight = 1.0f;
	constexpr float MaxOrthographicHeight = 100000.0f;

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
	const Matrix4x4 rotation = Matrix4x4::CreateFromQuaternion(view.rotation);
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
