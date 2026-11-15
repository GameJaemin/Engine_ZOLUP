#include "LightComponent.h"

#include <cmath>

bool LightComponent::SetColor(const Vector3& newColor)
{
	if (!std::isfinite(newColor.x) || !std::isfinite(newColor.y) || !std::isfinite(newColor.z)) { return false; }
	if (newColor.x < 0.0f || newColor.y < 0.0f || newColor.z < 0.0f) { return false; }

	color = newColor;
	return true;
}

bool LightComponent::SetIntensity(float newIntensity)
{
	if (!std::isfinite(newIntensity) || newIntensity < 0.0f) { return false; }

	intensity = newIntensity;
	return true;
}

void LightComponent::SetEnabled(bool enabled)
{
	isEnabled = enabled;
}
