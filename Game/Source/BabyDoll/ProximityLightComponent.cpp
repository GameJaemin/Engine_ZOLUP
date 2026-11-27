#include "ProximityLightComponent.h"

#include "Component/ComponentTypeRegistry.h"
#include "Component/PointLightComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "Component/StaticMeshComponent.h"
#include "World.h"

bool ProximityLightComponent::SetDarkRadius(float value)
{
	if (!std::isfinite(value) || value < 50.0f || value > 5000.0f) { return false; }
	darkRadius = value;
	return true;
}
bool ProximityLightComponent::SetFlickerTime(float value)
{
	if (!std::isfinite(value) || value < 0.05f || value > 5.0f) { return false; }
	flickerTime = value;
	return true;
}
bool ProximityLightComponent::SetBaseIntensity(float value)
{
	if (!std::isfinite(value) || value < 0.0f || value > 20.0f) { return false; }
	baseIntensity = value;
	return true;
}
void ProximityLightComponent::Reset()
{
	elapsed = 0.0f;
	dark = false;
	outputIntensity = baseIntensity;
	ApplyOutput();
}

bool ProximityLightComponent::BindEmissiveVisual(GameObject& visual, const std::array<std::shared_ptr<const Material>, 3>& materials)
{
	if (!GetGameObject() || &visual.GetWorld() != &GetGameObject()->GetWorld()) { return false; }
	for (const std::shared_ptr<const Material>& material : materials) { if (!material) { return false; } }
	std::vector<ObjectID> meshes;
	std::vector<GameObject*> objects{ &visual };
	for (std::size_t index = 0; index < objects.size(); ++index)
	{
		GameObject* object = objects[index];
		if (object->GetComponent<StaticMeshComponent>()) { meshes.push_back(object->GetObjectID()); }
		objects.insert(objects.end(), object->GetChildren().begin(), object->GetChildren().end());
	}
	if (meshes.empty()) { return false; }
	visualMeshes = std::move(meshes);
	visualMaterials = materials;
	appliedVisualLevel = -1;
	ApplyOutput();
	return true;
}

void ProximityLightComponent::ApplyOutput()
{
	if (!GetGameObject()) { return; }
	if (PointLightComponent* light = GetGameObject()->GetComponent<PointLightComponent>()) { light->SetIntensity(outputIntensity); }
	const int level = outputIntensity <= 0.0f ? 0 : outputIntensity < baseIntensity * 0.5f ? 1 : 2;
	if (level == appliedVisualLevel || visualMeshes.empty()) { return; }
	World& world = GetGameObject()->GetWorld();
	for (const ObjectID& id : visualMeshes)
	{
		GameObject* object = world.FindGameObject(id);
		if (!object || object->IsPendingDestroy()) { continue; }
		if (StaticMeshComponent* mesh = object->GetComponent<StaticMeshComponent>()) { mesh->SetMaterial(visualMaterials[level]); }
	}
	appliedVisualLevel = level;
}
void ProximityLightComponent::Step(const Vector3& monster, float deltaTime, bool enabled)
{
	if (!GetGameObject()) { return; }
	const Vector3 position = GetGameObject()->GetComponent<TransformComponent>()->GetWorldTransform().position;
	const bool sameFloor = std::floor((position.y + 1.0f) / 640.0f) == std::floor((monster.y + 1.0f) / 640.0f);
	if (!enabled || !sameFloor || PrototypeMath::Distance(position, monster) > darkRadius) { Reset(); }
	else
	{
		elapsed += deltaTime;
		dark = elapsed >= flickerTime;
		const int phase = static_cast<int>(elapsed * 20.0f);
		outputIntensity = dark ? 0.0f : baseIntensity * (phase % 3 == 0 ? 1.0f : 0.12f);
	}
	ApplyOutput();
}
bool RegisterPrototypeLightType()
{
	ComponentTypeRegistry& registry = ComponentTypeRegistry::Get();
	if (registry.FindByName("ProximityLightComponent")) { return true; }
	if (!registry.Register<ProximityLightComponent, Component>("ProximityLightComponent")) { return false; }
	if (!registry.AddField<ProximityLightComponent>("darkRadiusCm", &ProximityLightComponent::GetDarkRadius, &ProximityLightComponent::SetDarkRadius, { .displayName = "Dark Radius (cm)", .dragSpeed = 10.0f, .minValue = 50.0f, .maxValue = 5000.0f })) { return false; }
	if (!registry.AddField<ProximityLightComponent>("flickerTime", &ProximityLightComponent::GetFlickerTime, &ProximityLightComponent::SetFlickerTime, { .minValue = 0.05f, .maxValue = 5.0f })) { return false; }
	return registry.AddField<ProximityLightComponent>("baseIntensity", &ProximityLightComponent::GetBaseIntensity, &ProximityLightComponent::SetBaseIntensity, { .minValue = 0.0f, .maxValue = 20.0f });
}
