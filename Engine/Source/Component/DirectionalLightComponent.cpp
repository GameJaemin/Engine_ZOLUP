#include "DirectionalLightComponent.h"

#include "Component/TransformComponent.h"
#include "GameObject.h"

bool DirectionalLightComponent::GetLightDirection(Vector3& outDirection) const
{
	const GameObject* owner = GetGameObject();
	if (owner == nullptr) { return false; }

	const TransformComponent* transform = owner->GetComponent<TransformComponent>();
	if (transform == nullptr) { return false; }

	outDirection = transform->GetForwardVector();
	return true;
}
