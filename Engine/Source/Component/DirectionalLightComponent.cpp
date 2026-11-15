#include "DirectionalLightComponent.h"

#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "Renderer/RenderScene.h"
#include "World.h"

DirectionalLightComponent::~DirectionalLightComponent()
{
	if (registeredRenderScene != nullptr)
	{
		registeredRenderScene->RemoveDirectionalLightComponent(this);
	}
}

void DirectionalLightComponent::OnRegister()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr) { return; }

	RenderScene& renderScene = owner->GetWorld().GetRenderScene();
	renderScene.AddDirectionalLightComponent(this);
	registeredRenderScene = &renderScene;
}

bool DirectionalLightComponent::GetLightDirection(Vector3& outDirection) const
{
	const GameObject* owner = GetGameObject();
	if (owner == nullptr) { return false; }

	const TransformComponent* transform = owner->GetComponent<TransformComponent>();
	if (transform == nullptr) { return false; }

	outDirection = transform->GetForwardVector();
	return true;
}
