#pragma once

#include "LightComponent.h"

class RenderScene;

class AmbientLightComponent : public LightComponent
{
	// 생성과 소멸
public:
	/** 등록한 RenderScene이 있으면 그곳에서 이 컴포넌트를 제거한다. */
	~AmbientLightComponent() override;

	// 등록
public:
	/** 소속 World의 RenderScene에 등록한다. GameObject::AddComponent가 호출한다. */
	void OnRegister() override;

private:
	// 소멸할 때 등록을 해제할 씬의 주소다. 소유권은 World에 있다.
	RenderScene* registeredRenderScene = nullptr;
};
