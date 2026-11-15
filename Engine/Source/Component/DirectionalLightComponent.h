#pragma once

#include "LightComponent.h"
#include "Core/Math/Vector3.h"

/** 같은 객체의 월드 전방을 빛의 진행 방향으로 사용한다. */
class DirectionalLightComponent : public LightComponent
{
	// 방향
public:
	/**
	 * 같은 객체의 전방을 빛이 진행하는 월드 방향으로 반환한다.
	 * 부착 전이거나 TransformComponent가 없으면 출력값을 유지하고 false를 반환한다.
	 */
	bool GetLightDirection(Vector3& outDirection) const;
};
