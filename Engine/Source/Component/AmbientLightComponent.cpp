#include "AmbientLightComponent.h"

#include "GameObject.h"
#include "Renderer/RenderScene.h"
#include "World.h"

AmbientLightComponent::~AmbientLightComponent()
{
	if (registeredRenderScene != nullptr)
	{
		registeredRenderScene->RemoveAmbientLightComponent(this);
	}
}

void AmbientLightComponent::OnRegister()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr) { return; }

	RenderScene& renderScene = owner->GetWorld().GetRenderScene();
	renderScene.AddAmbientLightComponent(this);
	registeredRenderScene = &renderScene;
}
