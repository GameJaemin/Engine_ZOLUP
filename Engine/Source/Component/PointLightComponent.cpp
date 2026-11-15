#include "PointLightComponent.h"

#include "Component/TransformComponent.h"
#include "GameObject.h"

#include <cmath>

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
