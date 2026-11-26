#pragma once

#include "GameplaySpace.h"

#include "Component/Component.h"
#include "ObjectID.h"

class SceneCapture2DComponent;

/** 플레이어에 부착한다. 표시 중에만 별도 캡처 객체를 만들고 메인 카메라는 변경하지 않는다. */
class RearCameraComponent final : public Component
{
public:
	bool Step(bool chasing, bool preview, const Vector3& player, const Vector3& monster, float monsterYaw, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space);
	void Clear();
	SceneCapture2DComponent* GetCapture();
	bool IsVisible() const { return state.visible; }
	float GetRemaining() const { return state.remaining; }

private:
	RearViewState state;
	ObjectID cameraID;
	Vector3 cameraPosition;
	float cameraFov = 55.0f;
};
