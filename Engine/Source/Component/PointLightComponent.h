#pragma once

#include "LightComponent.h"
#include "Core/Math/Vector3.h"

class RenderScene;

class PointLightComponent : public LightComponent
{
	// 생성과 소멸
public:
	/** 등록한 RenderScene이 있으면 그곳에서 이 컴포넌트를 제거한다. */
	~PointLightComponent() override;

	// 등록
public:
	/** 소속 World의 RenderScene에 등록한다. GameObject::AddComponent가 호출한다. */
	void OnRegister() override;

private:
	// 소멸할 때 등록을 해제할 씬의 주소다. 소유권은 World에 있다.
	RenderScene* registeredRenderScene = nullptr;

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
