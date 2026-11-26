#include "CameraComponent.h"

#include "Camera/CameraViewInfo.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"
#include "World.h"

CameraComponent::~CameraComponent()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr) { return; }

	World& world = owner->GetWorld();
	if (world.GetActiveCamera() == this)
	{
		world.SetActiveCamera(nullptr);
	}
}

bool CameraComponent::SetVerticalFovDegrees(float newVerticalFovDegrees)
{
	if (!CameraProjection::IsValidVerticalFovDegrees(newVerticalFovDegrees)) { return false; }

	verticalFovDegrees = newVerticalFovDegrees;
	return true;
}

bool CameraComponent::SetOrthographicHeight(float newOrthographicHeight)
{
	if (!CameraProjection::IsValidOrthographicHeight(newOrthographicHeight)) { return false; }

	orthographicHeight = newOrthographicHeight;
	return true;
}

bool CameraComponent::SetNearClipDistance(float newNearClipDistance)
{
	return SetClipDistances(newNearClipDistance, farClipDistance);
}

bool CameraComponent::SetFarClipDistance(float newFarClipDistance)
{
	return SetClipDistances(nearClipDistance, newFarClipDistance);
}

bool CameraComponent::SetClipDistances(float newNearClipDistance, float newFarClipDistance)
{
	if (!CameraProjection::AreValidClipDistances(newNearClipDistance, newFarClipDistance)) { return false; }

	nearClipDistance = newNearClipDistance;
	farClipDistance = newFarClipDistance;
	return true;
}

bool CameraComponent::GetCameraViewInfo(CameraViewInfo& outCameraViewInfo) const
{
	const GameObject* owner = GetGameObject();
	if (owner == nullptr) { return false; }

	const TransformComponent* transformComponent = owner->GetComponent<TransformComponent>();
	if (transformComponent == nullptr) { return false; }

	const Transform transform = transformComponent->GetWorldTransform();
	outCameraViewInfo.position = transform.position;
	outCameraViewInfo.rotation = transform.rotation;
	outCameraViewInfo.projectionMode = projectionMode;
	outCameraViewInfo.verticalFovDegrees = verticalFovDegrees;
	outCameraViewInfo.orthographicHeight = orthographicHeight;
	outCameraViewInfo.nearClipDistance = nearClipDistance;
	outCameraViewInfo.farClipDistance = farClipDistance;
	return true;
}
