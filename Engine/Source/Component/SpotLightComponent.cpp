#include "SpotLightComponent.h"

#include "Component/TransformComponent.h"
#include "GameObject.h"

#include <cmath>

bool SpotLightComponent::GetLightPose(Vector3& outPosition, Vector3& outDirection) const
{
	const GameObject* owner = GetGameObject();
	if (owner == nullptr) { return false; }

	const TransformComponent* transform = owner->GetComponent<TransformComponent>();
	if (transform == nullptr) { return false; }

	outPosition = transform->GetWorldTransform().position;
	outDirection = transform->GetForwardVector();

	return true;
}

bool SpotLightComponent::SetRange(float newRangeCm)
{
	if (!std::isfinite(newRangeCm) || newRangeCm <= 0.0f) { return false; }

	rangeCm = newRangeCm;

	return true;
}

bool SpotLightComponent::SetOuterConeAngleDegrees(float newAngleDegrees)
{
	if (!std::isfinite(newAngleDegrees) || newAngleDegrees <= 0.0f || newAngleDegrees > 90.0f) { return false; }

	outerConeAngleDegrees = newAngleDegrees;

	return true;
}
