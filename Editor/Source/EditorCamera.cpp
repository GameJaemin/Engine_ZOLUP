#include "EditorCamera.h"

#include <cmath>

EditorCamera::EditorCamera()
{
	cameraViewInfo.projectionMode = CameraProjectionMode::Perspective;
	cameraViewInfo.verticalFovDegrees = 60.0f;
	cameraViewInfo.orthographicHeight = 1000.0f;
	cameraViewInfo.nearClipDistance = 10.0f;
	cameraViewInfo.farClipDistance = 100000.0f;
}

CameraViewInfo EditorCamera::GetCameraViewInfo() const
{
	return cameraViewInfo;
}

void EditorCamera::SetPosition(const Vector3& newPosition)
{
	cameraViewInfo.position = newPosition;
}

void EditorCamera::SetRotation(const Quaternion& newRotation)
{
	cameraViewInfo.rotation = newRotation;
}

bool EditorCamera::SetProjectionMode(CameraProjectionMode newProjectionMode)
{
	if (newProjectionMode != CameraProjectionMode::Perspective && newProjectionMode != CameraProjectionMode::Orthographic) { return false; }

	cameraViewInfo.projectionMode = newProjectionMode;
	return true;
}

bool EditorCamera::SetVerticalFovDegrees(float newVerticalFovDegrees)
{
	if (!std::isfinite(newVerticalFovDegrees) || newVerticalFovDegrees <= 0.0f || newVerticalFovDegrees >= 180.0f) { return false; }

	cameraViewInfo.verticalFovDegrees = newVerticalFovDegrees;
	return true;
}

bool EditorCamera::SetOrthographicHeight(float newOrthographicHeight)
{
	if (!std::isfinite(newOrthographicHeight) || newOrthographicHeight <= 0.0f) { return false; }

	cameraViewInfo.orthographicHeight = newOrthographicHeight;
	return true;
}

bool EditorCamera::SetClipDistances(float newNearClipDistance, float newFarClipDistance)
{
	if (!std::isfinite(newNearClipDistance) || !std::isfinite(newFarClipDistance)) { return false; }
	if (newNearClipDistance <= 0.0f || newFarClipDistance <= newNearClipDistance) { return false; }

	cameraViewInfo.nearClipDistance = newNearClipDistance;
	cameraViewInfo.farClipDistance = newFarClipDistance;
	return true;
}
