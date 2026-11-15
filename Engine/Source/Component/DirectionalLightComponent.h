#pragma once

#include "LightComponent.h"
#include "Core/Math/Vector3.h"

class RenderScene;

/**
 * 같은 객체의 월드 전방을 빛의 진행 방향으로 사용한다.
 * 부착 시 소속 World의 RenderScene에 등록되고 소멸 시 해제된다.
 */
class DirectionalLightComponent : public LightComponent
{
	// 생성과 소멸
public:
	/** 등록한 RenderScene이 있으면 그곳에서 이 컴포넌트를 제거한다. */
	~DirectionalLightComponent() override;

	// 등록
public:
	/** 소속 World의 RenderScene에 등록한다. GameObject::AddComponent가 호출한다. */
	void OnRegister() override;

private:
	// 비소유이며 World가 이 컴포넌트보다 오래 산다. 소멸 시 GameObject를 거치지 않고 해제하려고 보관한다.
	RenderScene* registeredRenderScene = nullptr;

	// 방향
public:
	/**
	 * 같은 객체의 전방을 빛이 진행하는 월드 방향으로 반환한다.
	 * 부착 전이거나 TransformComponent가 없으면 출력값을 유지하고 false를 반환한다.
	 */
	bool GetLightDirection(Vector3& outDirection) const;
};
