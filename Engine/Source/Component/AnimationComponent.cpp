#include "AnimationComponent.h"

#include "Animation/AnimationClip.h"
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

void AnimationComponent::SetNodeTransforms(std::vector<TransformComponent*> newNodeTransforms)
{
	nodeTransforms = std::move(newNodeTransforms);
}

void AnimationComponent::SetRestPose(std::vector<Transform> newRestPose)
{
	restPose = std::move(newRestPose);
	pose = restPose;
}

void AnimationComponent::Play(std::shared_ptr<const AnimationClip> newClip, bool newLooping)
{
	clip = std::move(newClip);
	looping = newLooping;
	currentTime = 0.0f;

	// 새 클립은 원래 자세에서 시작한다. 멈춤(nullptr)은 지금 자세를 유지한다.
	if (clip == nullptr)
	{
		return;
	}
	pose = restPose;
	for (std::size_t node = 0; node < pose.size() && node < nodeTransforms.size(); ++node)
	{
		if (nodeTransforms[node] == nullptr) { continue; }
		nodeTransforms[node]->SetTransform(pose[node]);
	}
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

	// 1) 클립 값을 포즈에 채운다. 원래 값을 받지 않았거나 다른 Model의 클립이면 포즈 밖이므로 그 트랙을 건너뛴다.
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
