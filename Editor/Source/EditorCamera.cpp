#include "EditorCamera.h"

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
	if (!CameraProjection::IsValidVerticalFovDegrees(newVerticalFovDegrees)) { return false; }

	cameraViewInfo.verticalFovDegrees = newVerticalFovDegrees;
	return true;
}

bool EditorCamera::SetOrthographicHeight(float newOrthographicHeight)
{
	if (!CameraProjection::IsValidOrthographicHeight(newOrthographicHeight)) { return false; }

	cameraViewInfo.orthographicHeight = newOrthographicHeight;
	return true;
}

bool EditorCamera::SetClipDistances(float newNearClipDistance, float newFarClipDistance)
{
	if (!CameraProjection::AreValidClipDistances(newNearClipDistance, newFarClipDistance)) { return false; }

	cameraViewInfo.nearClipDistance = newNearClipDistance;
	cameraViewInfo.farClipDistance = newFarClipDistance;
	return true;
}
