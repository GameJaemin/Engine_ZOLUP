#include "PointLightComponent.h"

#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "Renderer/RenderScene.h"
#include "World.h"

#include <cmath>

PointLightComponent::~PointLightComponent()
{
	if (registeredRenderScene != nullptr)
	{
		registeredRenderScene->RemovePointLightComponent(this);
	}
}

void PointLightComponent::OnRegister()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr) { return; }

	RenderScene& renderScene = owner->GetWorld().GetRenderScene();
	renderScene.AddPointLightComponent(this);
	registeredRenderScene = &renderScene;
}

bool PointLightComponent::GetLightPosition(Vector3& outPosition) const
{
	const GameObject* owner = GetGameObject();
	if (owner == nullptr) { return false; }

	const TransformComponent* transform = owner->GetComponent<TransformComponent>();
	if (transform == nullptr) { return false; }

	outPosition = transform->GetWorldTransform().position;
	return true;
}

bool PointLightComponent::SetRange(float newRangeCm)
{
	if (!std::isfinite(newRangeCm) || newRangeCm <= 0.0f) { return false; }

	rangeCm = newRangeCm;
	return true;
}
