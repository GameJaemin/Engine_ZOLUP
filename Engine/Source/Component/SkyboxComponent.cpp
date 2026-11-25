#include "SkyboxComponent.h"

#include "GameObject.h"
#include "Renderer/RenderScene.h"
#include "World.h"

#include <cstddef>
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

std::shared_ptr<const Texture> SkyboxComponent::GetFace(SkyboxFace face) const
{
	const std::size_t index = static_cast<std::size_t>(face);
	if (index >= faces.size()) { return nullptr; }
	return faces[index];
}

bool SkyboxComponent::SetFace(SkyboxFace face, std::shared_ptr<const Texture> newFace)
{
	const std::size_t index = static_cast<std::size_t>(face);
	if (index >= faces.size()) { return false; }

	faces[index] = std::move(newFace);
	return true;
}

void SkyboxComponent::SetEnabled(bool enabled)
{
	isEnabled = enabled;
}
