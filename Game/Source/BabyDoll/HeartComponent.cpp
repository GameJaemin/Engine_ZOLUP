#include "HeartComponent.h"

void HeartComponent::Reset()
{
	bpm = 72.0f;
	sprintTime = calmingTime = 0.0f;
	wasCalming = false;
}

void HeartComponent::OnDetected() { bpm = 150.0f; }

float HeartComponent::GetSoundRadius() const
{
	if (bpm < 100.0f) { return 0.0f; }
	const float t = (bpm - 100.0f) / 50.0f;
	return 50.0f + 1450.0f * t * t;
}

void HeartComponent::Step(float speed, bool hidden, bool chasing, bool suspicious, float monsterDistance, float deltaTime, const PrototypeConfig& config)
{
	const bool calming = hidden && suspicious;
	if (calming && !wasCalming) { calmingStart = bpm; calmingTime = 0.0f; }
	wasCalming = calming;
	if (chasing) { bpm = 150.0f; return; }
	if (calming)
	{
		calmingTime += deltaTime;
		const float alpha = std::clamp(calmingTime / config.calmTime, 0.0f, 1.0f);
		// 기획의 "2초 뒤 100 미만"을 만족하도록 완료 지점을 99.9로 둔다.
		bpm = (std::min)(bpm, calmingStart + (99.9f - calmingStart) * alpha);
		return;
	}
	if (hidden) { bpm = (std::max)(72.0f, bpm - 4.0f * deltaTime); return; }
	const float target = 72.0f + 48.0f * std::clamp(speed / config.maxSpeed, 0.0f, 1.0f);
	float rise = speed > 1.0f && bpm < target ? 48.0f / config.accelerationTime : 0.0f;
	if (speed >= config.maxSpeed * 0.95f)
	{
		sprintTime += deltaTime;
		if (bpm >= 120.0f) { rise = (std::max)(rise, 10.0f); }
	}
	else { sprintTime = 0.0f; }
	if (monsterDistance < 1000.0f) { rise = (std::max)(rise, 10.0f * (1.0f - monsterDistance / 1000.0f)); }
	if (rise > 0.0f) { bpm = (std::min)(150.0f, bpm + rise * deltaTime); }
	else if (speed < 1.0f && monsterDistance >= 1000.0f) { bpm = (std::max)(72.0f, bpm - 2.5f * deltaTime); }
}
