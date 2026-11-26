#include "CameraComponent.h"

#include "Camera/CameraViewInfo.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"
#include "Paths.h"
#include "World.h"

#include <cmath>
#include <filesystem>
#include <system_error>
#include <utility>

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

bool CameraComponent::SetClipDistances(const Vector2& newClipDistances)
{
	return SetClipDistances(newClipDistances.x, newClipDistances.y);
}

bool CameraComponent::SetSaturation(float newSaturation)
{
	const bool isValid = std::isfinite(newSaturation) && newSaturation >= 0.0f && newSaturation <= 2.0f;
	if (!isValid) { return false; }

	saturation = newSaturation;
	return true;
}

bool CameraComponent::SetPostProcessShaderPath(const std::string& newPath)
{
	// 없는 파일을 받으면 그릴 때마다 등록에 실패하므로 지정할 때 거른다.
	if (!newPath.empty())
	{
		std::filesystem::path absolutePath;
		if (!Paths::GetAbsolutePath(newPath, absolutePath)) { return false; }

		std::error_code error;
		if (!std::filesystem::is_regular_file(absolutePath, error)) { return false; }
	}

	postProcessShaderPath = newPath;
	return true;
}

bool CameraComponent::SetPostProcessParameters(std::vector<float> newParameters)
{
	if (newParameters.size() > MaxPostProcessParameters) { return false; }
	for (const float parameter : newParameters)
	{
		if (!std::isfinite(parameter)) { return false; }
	}

	postProcessParameters = std::move(newParameters);
	return true;
}

std::vector<PostProcessPass> CameraComponent::MakePostProcessPasses() const
{
	if (postProcessShaderPath.empty()) { return {}; }

	return { PostProcessPass{ postProcessShaderPath, postProcessParameters } };
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
