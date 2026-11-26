#include "HearingComponent.h"

void HearingComponent::Emit(const Vector3& position, float radius, SoundKind kind, bool player, int floor)
{
	if (radius <= 0.0f) { return; }
	stimuli.push_back({ position, radius, kind, floor, player, ++sequence });
}

std::optional<SoundStimulus> HearingComponent::Hear(const Vector3& listener, int floor) const
{
	std::optional<SoundStimulus> selected;
	for (const SoundStimulus& stimulus : stimuli)
	{
		if (stimulus.floor != floor || PrototypeMath::Distance(listener, stimulus.position) > stimulus.radius) { continue; }
		if (!selected || stimulus.player > selected->player || (stimulus.player == selected->player && stimulus.sequence > selected->sequence)) { selected = stimulus; }
	}
	return selected;
}

float HearingComponent::FootRadius(float speed, float maximumSpeed)
{
	if (speed < 1.0f) { return 0.0f; }
	const float t = std::clamp(speed / maximumSpeed, 0.0f, 1.0f);
	return 50.0f + 358.0f * t * t;
}
