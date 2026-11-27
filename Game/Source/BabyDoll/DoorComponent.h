#pragma once
#include "Component/Component.h"
#include "PrototypeTypes.h"

struct DoorState
{
	bool locked = true;
	bool targetOpen = false;
	float opening = 0.0f;
};

/** 잠금/개폐 시간만 소유한다. 충돌과 메시 반영은 조립 계층의 책임이다. */
class DoorComponent final : public Component
{
public:
	int keyID = 1;
	Vector3 position;
	DoorState state;
	bool Use(int heldItem)
	{
		if (state.locked && heldItem != keyID) { return false; }
		state.locked = false;
		state.targetOpen = !state.targetOpen;
		duration = 0.5f;
		return true;
	}
	void OpenForMonster(bool patrol)
	{
		if (state.targetOpen) { return; }
		state.targetOpen = true;
		state.locked = false;
		duration = patrol ? 1.5f : 0.2f;
	}
	void Step(float deltaTime, bool occupied)
	{
		if (!state.targetOpen && state.opening>0.0f && occupied) { state.targetOpen = true; }
		const float target = state.targetOpen ? 1.0f : 0.0f;
		state.opening += std::clamp(target - state.opening, -deltaTime / duration, deltaTime / duration);
	}
	bool IsPassable() const { return state.opening >= 0.999f; }
	void Restore(const DoorState& value) { state = value; duration = 0.5f; }
private:
	float duration = 0.5f;
};
