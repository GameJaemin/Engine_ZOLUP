#pragma once

#include "PrototypeTypes.h"

#include "Component/Component.h"
#include "ObjectID.h"
#include <array>
#include <memory>

class Material;
class GameObject;

class ProximityLightComponent final : public Component
{
public:
	void Step(const Vector3& monster, float deltaTime, bool enabled);
	void Reset();
	/** 같은 월드의 발광 메시를 연결한다. 재질 순서는 소등/약광/점등이며 상태 변경 때만 교체한다. */
	bool BindEmissiveVisual(GameObject& visual, const std::array<std::shared_ptr<const Material>, 3>& materials);
	float GetDarkRadius() const { return darkRadius; }
	float GetFlickerTime() const { return flickerTime; }
	float GetBaseIntensity() const { return baseIntensity; }
	bool SetDarkRadius(float value);
	bool SetFlickerTime(float value);
	bool SetBaseIntensity(float value);
	bool IsDark() const { return dark; }
	float GetOutputIntensity() const { return outputIntensity; }

private:
	void ApplyOutput();
	std::vector<ObjectID> visualMeshes;
	std::array<std::shared_ptr<const Material>, 3> visualMaterials;
	int appliedVisualLevel = -1;
	float darkRadius = 1500.0f;
	float flickerTime = 0.6f;
	float baseIntensity = 2.8f;
	float elapsed = 0.0f;
	float outputIntensity = 2.8f;
	bool dark = false;
};

/** 게임 소유 타입의 Inspector/스냅샷 속성을 엔진 등록부에 연결한다. */
bool RegisterPrototypeLightType();
