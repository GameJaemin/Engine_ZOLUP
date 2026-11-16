#include "AnimationComponent.h"

#include "Animation/AnimationClip.h"
#include "Component/TransformComponent.h"

#include <algorithm>
#include <cmath>
#include <utility>

void AnimationComponent::SetNodeTransforms(std::vector<TransformComponent*> newNodeTransforms)
{
	nodeTransforms = std::move(newNodeTransforms);
}

void AnimationComponent::Play(std::shared_ptr<const AnimationClip> newClip, bool newLooping)
{
	clip = std::move(newClip);
	looping = newLooping;
	currentTime = 0.0f;
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

	for (const AnimationNodeTrack& track : clip->tracks)
	{
		// 노드 목록을 받지 않았거나 다른 Model의 클립이면 목록 밖이므로 그 트랙을 건너뛴다.
		if (track.node >= nodeTransforms.size())
		{
			continue;
		}
		nodeTransforms[track.node]->SetTransform(track.Sample(currentTime));
	}
}
