#include "PoseAnimatorComponent.h"

#include "Animation/AnimationClip.h"
#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Quaternion.h"
#include "GameObject.h"

#include <algorithm>
#include <cmath>
#include <utility>

bool PoseAnimatorComponent::Initialize(std::shared_ptr<const Model> source, AnimationComponent& output)
{
	if (!source || source->animations.empty()) { return false; }
	model = std::move(source);
	target = &output;
	target->Play(nullptr, false);
	for (const GLTFSceneNode& node : model->sceneNodes) { rest.push_back(node.localTransform); }
	pose = transitionStart = rest;
	return true;
}

bool PoseAnimatorComponent::Play(std::string_view name, float blendSeconds, bool loop, bool synchronizePhase)
{
	if (clip && clip->GetName() == name) { return true; }
	for (const std::shared_ptr<const AnimationClip>& candidate : model->animations)
	{
		if (candidate->GetName() != name) { continue; }
		const float phase = synchronizePhase && clip && clip->duration > 0.0f ? std::fmod(time, clip->duration) / clip->duration : 0.0f;
		transitionStart = pose;
		clip = candidate;
		time = phase * clip->duration;
		blendTime = 0.0f;
		blendDuration = blendSeconds;
		looping = loop;
		return true;
	}
	return false;
}

void PoseAnimatorComponent::Step(float deltaTime, float playbackRate)
{
	if (!target || !clip) { return; }
	if (playing) { time += deltaTime * playbackRate; blendTime += deltaTime; }
	const float sampleTime = looping && clip->duration > 0.0f ? std::fmod(time, clip->duration) : (std::min)(time, clip->duration);
	pose = rest;
	for (const AnimationNodeTrack& track : clip->tracks) { pose[track.node] = track.Sample(sampleTime); }
	const float alpha = blendDuration > 0.0f ? std::clamp(blendTime / blendDuration, 0.0f, 1.0f) : 1.0f;
	for (std::size_t node = 0; node < pose.size(); ++node)
	{
		pose[node].position = Vector3::Lerp(transitionStart[node].position, pose[node].position, alpha);
		pose[node].rotation = Quaternion::Slerp(transitionStart[node].rotation, pose[node].rotation, alpha);
		pose[node].scale = Vector3::Lerp(transitionStart[node].scale, pose[node].scale, alpha);
	}
	// 원본 bind pose는 rest에 보존하고 최종 포즈 쓰기는 이 어댑터로 제한한다.
	target->SetPose(pose);
}

std::string_view PoseAnimatorComponent::GetClipName() const { return clip ? clip->GetName() : std::string_view{}; }

bool PoseAnimatorComponent::TryGetNodeWorldPosition(std::string_view name, Vector3& position) const
{
	if (!model || !GetGameObject()) { return false; }
	for (std::size_t index = 0; index < pose.size(); ++index)
	{
		if (model->sceneNodes[index].name != name) { continue; }
		Matrix4x4 matrix = pose[index].ToMatrix();
		std::optional<std::size_t> parent = model->sceneNodes[index].parent;
		while (parent)
		{
			matrix = matrix * pose[*parent].ToMatrix();
			parent = model->sceneNodes[*parent].parent;
		}
		matrix = matrix * GetGameObject()->GetComponent<TransformComponent>()->GetWorldMatrix();
		position = { matrix.m[3][0], matrix.m[3][1], matrix.m[3][2] };
		return true;
	}
	return false;
}
