#pragma once

#include "PrototypeTypes.h"

#include "Component/Component.h"

class ProximityLightComponent final : public Component
{
public:
	void Step(const Vector3& monster, float deltaTime, bool enabled);
	void Reset();
	float GetDarkRadius() const { return darkRadius; }
	float GetFlickerTime() const { return flickerTime; }
	float GetBaseIntensity() const { return baseIntensity; }
	bool SetDarkRadius(float value);
	bool SetFlickerTime(float value);
	bool SetBaseIntensity(float value);
	bool IsDark() const { return dark; }
	float GetOutputIntensity() const { return outputIntensity; }

private:
	float darkRadius = 1500.0f;
	float flickerTime = 0.6f;
	float baseIntensity = 2.8f;
	float elapsed = 0.0f;
	float outputIntensity = 2.8f;
	bool dark = false;
};

/** 게임 소유 타입의 Inspector/스냅샷 속성을 엔진 등록부에 연결한다. */
bool RegisterPrototypeLightType();
