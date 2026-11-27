#include "AnimationComponent.h"

#include "Animation/AnimationClip.h"
#include "AssetManager.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

AnimationComponent::~AnimationComponent()
{
	if (destroyingWorld != nullptr)
	{
		destroyingWorld->UnsubscribeGameObjectDestroying(destroyingSubscriptionId);
	}
}

void AnimationComponent::OnRegister()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr) { return; }

	World& world = owner->GetWorld();
	// 알림은 실제 파괴 전에 오므로 노드가 살아 있을 때 비교하고 지운다.
	destroyingSubscriptionId = world.SubscribeGameObjectDestroying([this](const GameObject& destroyed) noexcept
	{
		for (TransformComponent*& node : nodeTransforms)
		{
			if (node != nullptr && node->GetGameObject() == &destroyed) { node = nullptr; }
		}
	});
	destroyingWorld = &world;
}

void AnimationComponent::SetModel(std::shared_ptr<const Model> newModel)
{
	if (newModel == model) { return; }
	model = std::move(newModel);

	// 원래 자세는 모델 배치표의 부모 기준 값이다.
	restPose.clear();
	if (model != nullptr)
	{
		restPose.reserve(model->sceneNodes.size());
		for (const GLTFSceneNode& sceneNode : model->sceneNodes) { restPose.push_back(sceneNode.localTransform); }
	}
	pose = restPose;

	// 옛 모델의 노드 번호와 클립은 새 모델에서 뜻이 없으므로 비운다. 연결은 SetNodeTransforms로 다시 정한다.
	nodeTransforms.assign(restPose.size(), nullptr);
	clip = nullptr;
	currentTime = 0.0f;
}

bool AnimationComponent::SetNodeTransforms(std::vector<TransformComponent*> newNodeTransforms)
{
	// 원래 자세가 모델의 노드 수만큼 있으므로 그 길이로 확인한다. 모델이 없으면 0이다.
	if (newNodeTransforms.size() != restPose.size()) { return false; }
	nodeTransforms = std::move(newNodeTransforms);
	return true;
}

void AnimationComponent::SetPose(std::vector<Transform> newPose)
{
	pose = std::move(newPose);
	ApplyPoseToNodes();
}

void AnimationComponent::ApplyPoseToNodes()
{
	// 객체로 만든 노드에만 쓴다. 두 목록 중 짧은 쪽까지만 쓰며, 뼈 노드(nullptr)는 포즈에만 남는다.
	for (std::size_t node = 0; node < pose.size() && node < nodeTransforms.size(); ++node)
	{
		if (nodeTransforms[node] == nullptr) { continue; }
		nodeTransforms[node]->SetTransform(pose[node]);
	}
}

bool AnimationComponent::Play(std::shared_ptr<const AnimationClip> newClip, bool newLooping)
{
	// 트랙의 노드 번호는 그 클립을 가진 모델의 번호라, 같은 모델의 클립만 받는다.
	if (newClip != nullptr)
	{
		const bool isModelClip = model != nullptr && std::ranges::find(model->animations, newClip) != model->animations.end();
		if (!isModelClip) { return false; }
	}

	clip = std::move(newClip);
	looping = newLooping;
	currentTime = 0.0f;

	// 새 클립은 원래 자세에서 시작한다. 멈춤(nullptr)은 지금 자세를 유지한다.
	if (clip == nullptr)
	{
		return true;
	}
	pose = restPose;
	ApplyPoseToNodes();
	return true;
}

void AnimationComponent::Tick(float deltaTime)
{
	if (clip == nullptr)
	{
		return;
	}

	// 반복하면 길이로 나눈 나머지로 처음부터 다시 돌고, 아니면 끝에서 멈춘다. 길이가 0인 클립은 나누지 않는다.
	currentTime += deltaTime;
	if (looping && clip->duration > 0.0f)
	{
		currentTime = std::fmod(currentTime, clip->duration);
	}
	else
	{
		currentTime = std::min(currentTime, clip->duration);
	}

	// 1) 클립 값을 포즈에 채운다. Play가 같은 모델의 클립만 받으므로 보통은 범위 안이며, 범위 밖 트랙은 방어로 건너뛴다.
	for (const AnimationNodeTrack& track : clip->tracks)
	{
		if (track.node >= pose.size())
		{
			continue;
		}
		pose[track.node] = track.Sample(currentTime);
	}

	// 2) 트랙이 있는 노드만 노드 객체에 반영한다. 게임 코드가 다른 노드에 넣은 값은 덮어쓰지 않는다.
	for (const AnimationNodeTrack& track : clip->tracks)
	{
		if (track.node >= pose.size() || track.node >= nodeTransforms.size() || nodeTransforms[track.node] == nullptr)
		{
			continue;
		}
		nodeTransforms[track.node]->SetTransform(pose[track.node]);
	}
}
