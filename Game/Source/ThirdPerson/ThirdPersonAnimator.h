#pragma once

#include "ThirdPersonMotor.h"

#include "Animation/AnimationClip.h"
#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Quaternion.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

/** 기존 AnimationComponent의 공개 포즈 경로에 블렌딩 결과를 전달하는 게임 측 어댑터다. */
class ThirdPersonAnimator
{
public:
	bool Initialize(std::shared_ptr<const Model> newModel, const ThirdPersonConfig& newConfig)
	{
		model = std::move(newModel);
		config = newConfig;
		if (model == nullptr) { return false; }
		for (const std::shared_ptr<const AnimationClip>& clip : model->animations)
		{
			if (clip->GetName() == config.idleClip) { idle = clip; }
			if (clip->GetName() == config.walkClip) { walk = clip; }
			if (clip->GetName() == config.runClip) { run = clip; }
		}
		if (!idle || !walk || !run || idle->duration <= 0.0f || walk->duration <= 0.0f || run->duration <= 0.0f) { return false; }
		rest.clear();
		for (const GLTFSceneNode& node : model->sceneNodes)
		{
			rest.push_back(node.localTransform);
		}
		idlePose = walkPose = runPose = output = rest;
		return true;
	}

	/** 섞은 포즈를 AnimationComponent::SetPose로 전달한다. 엔진의 클립 재생(Play)은 쓰지 않는다. */
	void Update(AnimationComponent& target, float actualSpeed, float deltaTime)
	{
		const float elapsed = std::clamp(deltaTime, 0.0f, 0.1f);
		speed += (actualSpeed - speed) * (1.0f - std::exp(-14.0f * elapsed));
		if (speed < 0.05f) { speed = 0.0f; }
		idleTime = std::fmod(idleTime + elapsed, idle->duration);
		const float runWeight = std::clamp((speed - config.walkSpeed) / (config.runSpeed - config.walkSpeed), 0.0f, 1.0f);
		const float movingWeight = std::clamp(speed / config.walkSpeed, 0.0f, 1.0f);
		const float referenceSpeed = config.walkSpeed * (1.0f - runWeight) + config.runSpeed * runWeight;
		const float period = walk->duration * (1.0f - runWeight) + run->duration * runWeight;
		phase = std::fmod(phase + elapsed * std::clamp(speed / referenceSpeed, 0.0f, 1.5f) / period, 1.0f);
		Sample(*idle, idleTime, idlePose);
		Sample(*walk, phase * walk->duration, walkPose);
		Sample(*run, std::fmod(phase + config.runPhaseOffset + 1.0f, 1.0f) * run->duration, runPose);
		for (std::size_t node = 0; node < rest.size(); ++node)
		{
			const Transform locomotion = Blend(walkPose[node], runPose[node], runWeight);
			output[node] = Blend(idlePose[node], locomotion, movingWeight);
		}
		// 원래 기준 자세는 rest에 보관하고, 현재 포즈와 노드 반영은 엔진이 맡는다.
		target.SetPose(output);
	}

private:
	void Sample(const AnimationClip& clip, float time, std::vector<Transform>& pose)
	{
		pose = rest;
		for (const AnimationNodeTrack& track : clip.tracks)
		{
			if (track.node < pose.size())
			{
				pose[track.node] = track.Sample(time);
			}
		}
	}

	static Transform Blend(const Transform& from, const Transform& to, float alpha)
	{
		return Transform{ Vector3::Lerp(from.position, to.position, alpha), Quaternion::Slerp(from.rotation, to.rotation, alpha), Vector3::Lerp(from.scale, to.scale, alpha) };
	}

private:
	ThirdPersonConfig config;
	std::shared_ptr<const Model> model;
	std::shared_ptr<const AnimationClip> idle;
	std::shared_ptr<const AnimationClip> walk;
	std::shared_ptr<const AnimationClip> run;
	std::vector<Transform> rest;
	std::vector<Transform> idlePose;
	std::vector<Transform> walkPose;
	std::vector<Transform> runPose;
	std::vector<Transform> output;
	float speed = 0.0f;
	float phase = 0.0f;
	float idleTime = 0.0f;
};
