#pragma once

#include "Component.h"

#include <memory>
#include <vector>

class AnimationClip;
class TransformComponent;

/**
 * 클립 하나의 재생 시각을 재고, 매 프레임 트랙 값을 노드 객체의 TransformComponent에 넣는다.
 * 무엇을 재생할지는 사용하는 쪽이 Play로 정하며, 부착만으로는 재생하지 않는다.
 */
class AnimationComponent : public Component
{
	// 노드 연결
public:
	/**
	 * 클립 node 번호 순서의 노드 TransformComponent 목록을 받는다.
	 * 노드 객체는 이 컴포넌트를 붙인 객체의 자손이어야 하고 따로 삭제하지 않는다.
	 */
	void SetNodeTransforms(std::vector<TransformComponent*> newNodeTransforms);

private:
	// 클립 node 번호 순서. 비소유이며 자손 객체의 컴포넌트를 가리킨다.
	std::vector<TransformComponent*> nodeTransforms;

	// 재생
public:
	/** newClip을 처음부터 재생한다. nullptr면 멈춘다. 클립은 노드 목록과 같은 Model에서 불러온 것이어야 한다. */
	void Play(std::shared_ptr<const AnimationClip> newClip, bool newLooping);

private:
	std::shared_ptr<const AnimationClip> clip;
	// 재생 시각(초). 0부터 clip의 duration까지다.
	float currentTime = 0.0f;
	// true면 duration을 넘을 때 처음으로 돌아가고, false면 마지막 키에서 멈춘다.
	bool looping = false;

	// 갱신
public:
	/** 재생 중이면 시각을 deltaTime(초)만큼 진행하고 노드 값을 갱신한다. */
	void Tick(float deltaTime) override;
};
