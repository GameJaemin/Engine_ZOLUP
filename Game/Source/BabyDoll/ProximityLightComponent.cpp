#include "ProximityLightComponent.h"

#include "Component/ComponentTypeRegistry.h"
#include "Component/PointLightComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"

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
}
void ProximityLightComponent::Step(const Vector3& monster, float deltaTime, bool enabled)
{
	if (!GetGameObject()) { return; }
	const Vector3 position = GetGameObject()->GetComponent<TransformComponent>()->GetWorldTransform().position;
	if (!enabled || PrototypeMath::Distance(position, monster) > darkRadius) { Reset(); }
	else
	{
		elapsed += deltaTime;
		dark = elapsed >= flickerTime;
		const int phase = static_cast<int>(elapsed * 20.0f);
		outputIntensity = dark ? 0.0f : baseIntensity * (phase % 3 == 0 ? 1.0f : 0.12f);
	}
	if (PointLightComponent* light = GetGameObject()->GetComponent<PointLightComponent>()) { light->SetIntensity(outputIntensity); }
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
