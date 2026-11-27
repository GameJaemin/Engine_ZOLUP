#pragma once
#include "PrototypeTypes.h"
#include "Component/Component.h"
#include "ObjectID.h"
#include <array>
#include <memory>
class Model;
class GameObject;

/** 모델 본 이름에 의존하지 않는 임시 스티치 외형. AI 상태를 값으로만 받는다. */
class StitchVisualComponent final : public Component
{
public:
	bool Initialize(std::shared_ptr<const Model> box);
	void Present(StitchState state, StitchAction action, float distance, float speed, float deltaTime);
	void PresentCapture(float elapsed);
	float GetOpening() const { return opening; }
	void Reset() { opening=time=phase=0.0f; }
	void RevealForCapture() { opening=1.0f; }
private:
	std::array<ObjectID,8> parts{};
	std::array<ObjectID,18> tentacles{};
	std::array<ObjectID,10> teeth{};
	ObjectID mouth;
	float opening=0.0f,time=0.0f,phase=0.0f;
};
