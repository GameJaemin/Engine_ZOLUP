#include "SkyboxComponent.h"

#include "GameObject.h"
#include "Renderer/RenderScene.h"
#include "World.h"

#include <utility>

SkyboxComponent::~SkyboxComponent()
{
	if (registeredRenderScene != nullptr)
	{
		registeredRenderScene->RemoveSkyboxComponent(this);
	}
}

void SkyboxComponent::OnRegister()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr) { return; }

	RenderScene& scene = owner->GetWorld().GetRenderScene();
	scene.AddSkyboxComponent(this);
	registeredRenderScene = &scene;
}

void SkyboxComponent::SetFaces(std::array<std::shared_ptr<const Texture>, 6> newFaces)
{
	faces = std::move(newFaces);
}

void SkyboxComponent::SetEnabled(bool enabled)
{
	isEnabled = enabled;
}
