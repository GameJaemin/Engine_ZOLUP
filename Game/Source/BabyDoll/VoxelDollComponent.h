#pragma once

#include "PrototypeTypes.h"
#include "Component/Component.h"
#include "ObjectID.h"

#include <array>

/** 표현만 담당한다. 이동/충돌/AI를 수정하지 않고 이 컴포넌트와 외형을 교체할 수 있다. */
class VoxelDollComponent final : public Component
{
public:
	bool Initialize();
	void Reset(const Vector3& position);
	void Present(const Vector3& position, float speed, float bodyYaw, float viewYaw, PlayerMode mode,
		float deltaTime, const PrototypeConfig& config, float fear = 0.0f, int lastFoot = -1, bool crawlConstrained = false);
	void UseHand(int hand) { actionHand=hand; actionTime=0.45f; }
	GameObject* GetHandAnchor(int hand);
	float GetPhase() const { return phase; }

private:
	std::array<ObjectID, 6> parts{};
	Vector3 previousPosition;
	float phase = 0.0f;
	float elapsed = 0.0f;
	float movement = 0.0f;
	float crouch = 0.0f;
	float fall = 0.0f;
	float lean = 0.0f;
	float previousYaw = 0.0f;
	bool initializedPosition = false;
	int previousFoot=-1, actionHand=1;
	float stepPulse=0.0f, actionTime=0.0f, wakeTime=0.0f;
	float crawlAmount=0.0f;
};
