#pragma once

#include "Component.h"

/** 부모가 없는 객체의 자유낙하를 갱신한다. 객체 하나에 한 번 부착한다. */
class GravityComponent final : public Component
{
public:
	/** 중력 가속도 적용 여부를 반환한다. 부착 시 기본값은 true다. */
	bool IsGravityEnabled() const { return gravityEnabled; }

	/** 중력 가속도를 켜거나 끈다. 현재 Y축 속도는 유지한다. */
	void SetGravityEnabled(bool enabled);

	/** 월드 -Y 방향으로 이동한다. deltaTime은 초 단위다. */
	void Tick(float deltaTime) override;

private:
	bool gravityEnabled = true;
	// 월드 Y축 속도다. 단위는 cm/s다.
	float verticalSpeed = 0.0f;
};
