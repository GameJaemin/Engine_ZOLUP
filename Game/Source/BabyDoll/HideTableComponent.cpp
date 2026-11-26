#include "HideTableComponent.h"

void HideTableComponent::Enter(bool chasing, const Vector3& entry)
{
	entryPosition = entry;
	breakableOccupancy = remainingUses == 0;
	if (chasing && remainingUses > 0) { --remainingUses; }
}

void HideTableComponent::Leave() { breakableOccupancy = false; }
void HideTableComponent::Break() { broken = true; breakableOccupancy = false; }

void HideTableComponent::Restore(const HideTableSnapshot& snapshot)
{
	remainingUses = snapshot.remainingUses;
	broken = snapshot.broken;
	breakableOccupancy = false;
}

bool HideTableComponent::FindExit(float viewYaw, const Vector3& monster, const GameplaySpace& space, Vector3& exit) const
{
	Vector3 away = center - monster;
	away.y = 0.0f;
	const float length = PrototypeMath::Length(away);
	away = length > 0.001f ? away / length : PrototypeMath::Direction(viewYaw);
	const Vector3 candidates[]{ center + PrototypeMath::Direction(viewYaw) * 230.0f, entryPosition, center + away * 230.0f };
	for (const Vector3& candidate : candidates)
	{
		if (!space.IsFree(candidate, 30.0f, 120.0f)) { continue; }
		bool clear = true;
		for (int step = 1; step <= 12; ++step)
		{
			if (!space.IsFree(Vector3::Lerp(center, candidate, static_cast<float>(step) / 12.0f), 30.0f, 45.0f)) { clear = false; break; }
		}
		if (clear) { exit = candidate; return true; }
	}
	return false;
}
