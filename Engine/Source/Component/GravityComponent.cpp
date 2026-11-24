#include "GravityComponent.h"

#include "Component/TransformComponent.h"
#include "GameObject.h"

void GravityComponent::SetGravityEnabled(bool enabled)
{
	gravityEnabled = enabled;
}

void GravityComponent::Tick(float deltaTime)
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr || owner->GetParent() != nullptr) { return; }

	// 엔진은 cm 단위를 사용하고 월드 +Y가 위쪽이다.
	constexpr float Gravity = -980.0f;
	const float acceleration = gravityEnabled ? Gravity : 0.0f;
	TransformComponent* transform = owner->GetComponent<TransformComponent>();
	Vector3 position = transform->GetPosition();
	position.y += (verticalSpeed + 0.5f * acceleration * deltaTime) * deltaTime;
	verticalSpeed += acceleration * deltaTime;
	transform->SetPosition(position);
}
