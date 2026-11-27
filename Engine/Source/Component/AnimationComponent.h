#pragma once

#include "Component.h"
#include "Core/Math/Transform.h"

#include <cstdint>
#include <memory>
#include <vector>

class AnimationClip;
class Model;
class TransformComponent;
class World;

/**
 * 클립 하나의 재생 시각을 재고, 매 프레임 트랙 값을 노드 객체의 TransformComponent에 넣는다.
 * 무엇을 재생할지는 사용하는 쪽이 Play로 정하며, 부착만으로는 재생하지 않는다.
 */
class AnimationComponent : public Component
{
	// 생성과 소멸
public:
	AnimationComponent() = default;

	/** 구독한 World가 있으면 객체 삭제 알림 구독을 해제한다. */
	~AnimationComponent() override;

	// 등록
public:
	/** 소속 World의 객체 삭제 알림을 구독한다. GameObject::AddComponent가 호출하며 World의 삭제 처리 중에는 부착하지 않는다. */
	void OnRegister() override;

private:
	// 소멸 시 구독을 해제하려고 보관한다. 비소유이며 World가 이 컴포넌트보다 오래 산다.
	World* destroyingWorld = nullptr;
	// 0이면 구독하지 않았다.
	std::uint64_t destroyingSubscriptionId = 0;

	// 모델
public:
	/**
	 * 노드 배치표와 클립을 가진 모델을 정하고, 원래 자세와 현재 포즈를 그 모델의 노드 값으로 채운다.
	 * 다른 모델이면 노드 연결을 새 노드 수만큼 비우고(전부 nullptr) 재생 중인 클립을 멈춘다. 같은 모델이면 아무것도 바꾸지 않는다.
	 * nullptr면 노드가 없는 상태가 된다. 노드 연결은 이 함수 뒤에 SetNodeTransforms로 정한다.
	 */
	void SetModel(std::shared_ptr<const Model> newModel);

	/** 노드 배치표와 클립을 가진 모델을 빌려준다. 정하지 않았으면 nullptr다. */
	const std::shared_ptr<const Model>& GetModel() const { return model; }

private:
	std::shared_ptr<const Model> model;

	// 노드 연결
public:
	/**
	 * 클립 node 번호 순서의 노드 TransformComponent 목록을 받는다. 길이는 모델의 노드 수와 같아야 하며, 다르면 바꾸지 않고 false를 반환한다.
	 * 노드 객체는 같은 World에 있는 이 컴포넌트를 붙인 객체의 자손이어야 한다.
	 * 객체로 만들지 않은 노드(뼈)는 nullptr이며, 그 노드의 값은 포즈에만 적는다.
	 * 부착 후 노드 객체가 삭제되면 그 칸은 nullptr가 되어 뼈와 같이 다룬다.
	 */
	bool SetNodeTransforms(std::vector<TransformComponent*> newNodeTransforms);

	/** 클립 node 번호 순서의 노드 TransformComponent 목록을 빌려준다. 객체가 없는 노드(뼈)는 nullptr다. */
	const std::vector<TransformComponent*>& GetNodeTransforms() const { return nodeTransforms; }

private:
	// 클립 node 번호 순서. 비소유이며 자손 객체의 컴포넌트를 가리킨다.
	std::vector<TransformComponent*> nodeTransforms;

	// 포즈
public:
	/** 노드 번호 순서의 원래 부모 기준 값을 빌려준다. SetModel이 모델 배치표에서 채운다. */
	const std::vector<Transform>& GetRestPose() const { return restPose; }

	/**
	 * 노드 번호 순서의 이번 프레임 부모 기준 값을 받고 노드 객체의 Transform에 반영한다. 원래 값(restPose)은 바꾸지 않는다.
	 * 여러 클립을 섞는 애니메이터처럼 Play 대신 직접 포즈를 만드는 쪽이 쓴다. 재생 중인 클립이 있으면 다음 Tick이 트랙 노드를 덮어쓴다.
	 */
	void SetPose(std::vector<Transform> newPose);

	/** 노드 번호 순서의 이번 프레임 부모 기준 값을 빌려준다. 재생 전에는 원래 값이다. */
	const std::vector<Transform>& GetPose() const { return pose; }

private:
	/** 현재 포즈를 객체가 있는 노드의 TransformComponent에 쓴다. 두 목록 중 짧은 쪽까지만 쓴다. */
	void ApplyPoseToNodes();

	// 노드 번호 순서의 원래 값(배치표의 localTransform). 엔진 좌표계(왼손, cm)이며 바뀌지 않는다.
	std::vector<Transform> restPose;
	// 노드 번호 순서의 이번 프레임 부모 기준 값. 트랙이 없는 노드는 restPose 값을 유지한다.
	std::vector<Transform> pose;

	// 재생
public:
	/**
	 * newClip을 처음부터 재생하며, 모든 노드를 원래 값으로 되돌린 뒤 시작한다. nullptr면 지금 자세로 멈추고 true를 반환한다.
	 * 클립이 이 컴포넌트 모델의 클립 목록(Model::animations)에 없으면 아무것도 바꾸지 않고 false를 반환한다.
	 */
	bool Play(std::shared_ptr<const AnimationClip> newClip, bool newLooping);

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
