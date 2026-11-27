#pragma once

#include "Component/Component.h"
#include "Core/Math/Transform.h"

#include <memory>
#include <string_view>
#include <vector>

struct Model;
class AnimationClip;
class AnimationComponent;

/** 같은 모델의 임의 클립을 교차 재생한다. AI 상태와 클립 이름은 호출자가 연결한다. */
class PoseAnimatorComponent final : public Component
{
public:
	bool Initialize(std::shared_ptr<const Model> source, AnimationComponent& output);
	bool Play(std::string_view name, float blendSeconds = 0.18f, bool loop = true, bool synchronizePhase = false);
	void Step(float deltaTime, float playbackRate = 1.0f);
	std::string_view GetClipName() const;
	bool TryGetNodeWorldPosition(std::string_view name, Vector3& position) const;
	void SetPlaying(bool newPlaying) { playing = newPlaying; }

private:
	std::shared_ptr<const Model> model;
	std::shared_ptr<const AnimationClip> clip;
	AnimationComponent* target = nullptr;
	std::vector<Transform> rest;
	std::vector<Transform> pose;
	std::vector<Transform> transitionStart;
	float time = 0.0f;
	float blendTime = 0.0f;
	float blendDuration = 0.18f;
	bool looping = true;
	bool playing = true;
};
