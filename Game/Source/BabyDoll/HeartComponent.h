#pragma once

#include "PrototypeTypes.h"

#include "Component/Component.h"

class HeartComponent final : public Component
{
public:
	struct Snapshot { float bpm=72.0f, sprintTime=0.0f, calmingTime=0.0f, calmingStart=150.0f; bool wasCalming=false; };
	Snapshot Capture() const { return {bpm,sprintTime,calmingTime,calmingStart,wasCalming}; }
	void Restore(const Snapshot& state) { bpm=state.bpm; sprintTime=state.sprintTime; calmingTime=state.calmingTime; calmingStart=state.calmingStart; wasCalming=state.wasCalming; }
	void Reset(const PrototypeConfig& config = {});
	void SetTuning(const PrototypeConfig& config) { audible=config.heartAudible; maximum=config.heartMaximum; soundMinimum=config.heartRadiusMinimum; soundMaximum=config.heartRadiusMaximum; exponent=config.soundCurveExponent; }
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
	float audible=100.0f, maximum=150.0f, soundMinimum=50.0f, soundMaximum=1500.0f, exponent=2.0f;
};
