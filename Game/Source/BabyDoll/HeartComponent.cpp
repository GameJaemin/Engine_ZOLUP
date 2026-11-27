#include "HeartComponent.h"

void HeartComponent::Reset(const PrototypeConfig& config)
{
	SetTuning(config);
	bpm = config.heartRest;
	sprintTime = calmingTime = 0.0f;
	wasCalming = false;
}

void HeartComponent::OnDetected() { bpm = maximum; }

float HeartComponent::GetSoundRadius() const
{
	if (bpm < audible) { return 0.0f; }
	const float t = std::clamp((bpm-audible)/(maximum-audible),0.0f,1.0f);
	return soundMinimum+(soundMaximum-soundMinimum)*std::pow(t,exponent);
}

void HeartComponent::Step(float speed, bool hidden, bool chasing, bool suspicious, float monsterDistance, float deltaTime, const PrototypeConfig& config)
{
	SetTuning(config);
	const bool calming = hidden && suspicious;
	if (calming && !wasCalming) { calmingStart = bpm; calmingTime = 0.0f; }
	wasCalming = calming;
	if (chasing) { bpm = config.heartMaximum; return; }
	if (calming)
	{
		calmingTime += deltaTime;
		const float alpha = std::clamp(calmingTime / config.calmTime, 0.0f, 1.0f);
		// 기획의 "2초 뒤 100 미만"을 만족하도록 완료 지점을 99.9로 둔다.
		bpm = (std::min)(bpm, calmingStart + (config.heartAudible-0.1f - calmingStart) * alpha);
		return;
	}
	if (hidden) { bpm = (std::max)(config.heartRest, bpm - config.heartHideDrop * deltaTime); return; }
	const float target = config.heartRest + (config.heartRunPeak-config.heartRest) * std::clamp(speed / config.maxSpeed, 0.0f, 1.0f);
	float rise = speed > 1.0f && bpm < target ? (config.heartRunPeak-config.heartRest) / config.accelerationTime : 0.0f;
	if (speed >= config.maxSpeed * 0.95f)
	{
		sprintTime += deltaTime;
		if (bpm >= config.heartRunPeak) { rise = (std::max)(rise, (config.heartMaximum-config.heartRunPeak)/3.0f); }
	}
	else { sprintTime = 0.0f; }
	if (monsterDistance < config.heartFearRadius) { rise = (std::max)(rise, config.heartFearRise * (1.0f - monsterDistance / config.heartFearRadius)); }
	if (rise > 0.0f) { bpm = (std::min)(config.heartMaximum, bpm + rise * deltaTime); }
	else if (speed < 1.0f && monsterDistance >= config.heartFearRadius) { bpm = (std::max)(config.heartRest, bpm - config.heartIdleDrop * deltaTime); }
}
