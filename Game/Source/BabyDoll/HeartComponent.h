#pragma once

#include "PrototypeTypes.h"

#include "Component/Component.h"

class HeartComponent final : public Component
{
public:
	void Reset();
	void Step(float speed, bool hidden, bool chasing, bool suspicious, float monsterDistance, float deltaTime, const PrototypeConfig& config);
	void OnDetected();
	float GetBPM() const { return bpm; }
	float GetSoundRadius() const;

private:
	float bpm = 72.0f;
	float sprintTime = 0.0f;
	float calmingTime = 0.0f;
	float calmingStart = 150.0f;
	bool wasCalming = false;
};
