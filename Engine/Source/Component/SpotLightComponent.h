#pragma once

#include "LightComponent.h"
#include "Core/Math/Vector3.h"

/** 같은 객체의 월드 위치와 전방을 사용하는 원뿔형 조명이다. */
class SpotLightComponent : public LightComponent
{
	// 위치와 방향
public:
	/**
	 * 월드 위치(cm)와 빛이 진행하는 월드 방향을 반환한다.
	 * 부착 전이거나 TransformComponent가 없으면 두 출력값을 유지하고 false를 반환한다.
	 */
	bool GetLightPose(Vector3& outPosition, Vector3& outDirection) const;

	// 범위
public:
	/** 빛이 닿는 최대 거리(cm)를 반환한다. 기본값은 300cm다. */
	float GetRange() const { return rangeCm; }

	/** 유한한 양수만 받는다. 아니면 기존 값을 유지하고 false를 반환한다. */
	bool SetRange(float newRangeCm);

private:
	float rangeCm = 300.0f;

	// 원뿔 각도
public:
	/** 중심축에서 원뿔 가장자리까지의 각도(도)를 반환한다. 기본값은 30도다. */
	float GetOuterConeAngleDegrees() const { return outerConeAngleDegrees; }

	/** 0도보다 크고 90도 이하인 값만 받는다. 아니면 기존 값을 유지하고 false를 반환한다. */
	bool SetOuterConeAngleDegrees(float newAngleDegrees);

private:
	float outerConeAngleDegrees = 30.0f;
};
