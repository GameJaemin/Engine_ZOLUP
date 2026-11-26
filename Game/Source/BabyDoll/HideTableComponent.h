#pragma once

#include "GameplaySpace.h"

#include "Component/Component.h"

struct HideTableSnapshot
{
	int remainingUses = 1;
	bool broken = false;
};

/** 배치 하나의 수명만 관리한다. 플레이어와 AI를 직접 참조하지 않는다. */
class HideTableComponent final : public Component
{
public:
	Vector3 center;
	std::size_t obstacleIndex = 0;

	void Enter(bool chasing, const Vector3& entry);
	void Leave();
	void Break();
	void Restore(const HideTableSnapshot& snapshot);
	HideTableSnapshot Capture() const { return { remainingUses, broken }; }
	bool FindExit(float viewYaw, const Vector3& monster, const GameplaySpace& space, Vector3& exit) const;
	bool IsBroken() const { return broken; }
	bool IsBreakableOccupancy() const { return breakableOccupancy; }
	int GetRemainingUses() const { return remainingUses; }

private:
	Vector3 entryPosition;
	int remainingUses = 1;
	bool broken = false;
	bool breakableOccupancy = false;
};
