#pragma once

#include "LightComponent.h"
#include "Core/Math/Vector3.h"

class PointLightComponent : public LightComponent
{
	// 위치
public:
	/**
	 * 같은 객체의 월드 위치(cm)를 outPosition에 저장하고 true를 반환한다.
	 * 부착 전이거나 TransformComponent가 없으면 출력값을 유지하고 false를 반환한다.
	 */
	bool GetLightPosition(Vector3& outPosition) const;

	// 범위
public:
	/** 빛이 닿는 최대 거리(cm)를 반환한다. 기본값은 300cm다. */
	float GetRange() const { return rangeCm; }

	/** 유한한 양수만 받는다. 아니면 기존 값을 유지하고 false를 반환한다. */
	bool SetRange(float newRangeCm);

private:
	float rangeCm = 300.0f;
};
