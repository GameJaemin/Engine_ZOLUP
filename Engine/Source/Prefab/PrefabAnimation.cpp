#include "Prefab/PrefabComponents.h"

#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"
#include "GLTF/GLTFLoader.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

bool ReadAnimationProperties(const rapidjson::Value& properties, std::string_view context, PrefabAnimationDefinition& animation)
{
	const auto model = properties.FindMember("model");
	const std::string_view error = model == properties.MemberEnd() ? "is required" : ValidatePrefabAssetPath(model->value);
	if (!error.empty())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}.model {}", context, error));
		return false;
	}

	const auto bindings = properties.FindMember("nodeBindings");
	if (bindings == properties.MemberEnd() || !bindings->value.IsArray())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}.nodeBindings must be an array", context));
		return false;
	}

	animation.modelPath.assign(model->value.GetString(), model->value.GetStringLength());

	const rapidjson::Value::ConstArray entries = bindings->value.GetArray();
	animation.nodeBindings.reserve(entries.Size());
	for (rapidjson::SizeType index = 0; index < entries.Size(); ++index)
	{
		const rapidjson::Value& entry = entries[index];
		const std::string bindingContext = std::format("{}.nodeBindings[{}]", context, index);
		if (!entry.IsObject())
		{
			Log::Write(LogLevel::Error, "Prefab", bindingContext + " must be an object");
			return false;
		}

		const auto sourceNode = entry.FindMember("sourceNode");
		if (sourceNode == entry.MemberEnd() || !sourceNode->value.IsUint64() || !std::in_range<std::size_t>(sourceNode->value.GetUint64()))
		{
			Log::Write(LogLevel::Error, "Prefab", bindingContext + ".sourceNode must be a non-negative integer in range");
			return false;
		}

		const auto target = entry.FindMember("target");
		if (target == entry.MemberEnd() || !target->value.IsString() || target->value.GetStringLength() == 0)
		{
			Log::Write(LogLevel::Error, "Prefab", bindingContext + ".target must be a non-empty string");
			return false;
		}

		PrefabNodeBinding binding{ static_cast<std::size_t>(sourceNode->value.GetUint64()), std::string(target->value.GetString(), target->value.GetStringLength()) };

		// 한 노드를 두 객체에 넣거나 두 노드가 한 객체를 덮어쓰면 어느 값이 남는지 정해지지 않는다.
		const auto isDuplicate = [&binding](const PrefabNodeBinding& other) { return other.sourceNode == binding.sourceNode || other.targetID == binding.targetID; };
		if (std::ranges::any_of(animation.nodeBindings, isDuplicate))
		{
			Log::Write(LogLevel::Error, "Prefab", bindingContext + ": sourceNode or target is already bound");
			return false;
		}

		animation.nodeBindings.push_back(std::move(binding));
	}

	return true;
}

bool LoadPrefabAnimation(AssetManager& assets, const PrefabAnimationDefinition& definition, std::string_view context, PrefabAnimation& outAnimation)
{
	std::shared_ptr<const Model> model = assets.LoadModel(definition.modelPath);
	if (model == nullptr)
	{
		Log::Write(LogLevel::Error, "AssetManager", std::format("{}: could not load animation model {}", context, definition.modelPath));
		return false;
	}

	// 노드 번호는 모델 배치표의 줄 번호라 모델을 읽은 뒤에 범위를 확인한다.
	for (const PrefabNodeBinding& binding : definition.nodeBindings)
	{
		if (binding.sourceNode < model->sceneNodes.size()) { continue; }

		Log::Write(LogLevel::Error, "AssetManager", std::format("{}: animation sourceNode {} is out of range for {}", context, binding.sourceNode, definition.modelPath));
		return false;
	}

	outAnimation = PrefabAnimation{ std::move(model), definition.nodeBindings };
	return true;
}

// 대상 id는 프리팹 파일 단계에서, 노드 번호 범위는 에셋 단계에서 확인했다.
void AttachPrefabAnimation(GameObject& object, const PrefabAnimation& animation, const std::unordered_map<std::string, GameObject*>& objectsByID)
{
	const Model& model = *animation.model;

	// 연결하지 않은 노드는 nullptr이며, 재생 컴포넌트는 그 노드 값을 포즈에만 적는다.
	std::vector<TransformComponent*> nodeTransforms(model.sceneNodes.size(), nullptr);
	for (const PrefabNodeBinding& binding : animation.nodeBindings)
	{
		nodeTransforms[binding.sourceNode] = objectsByID.at(binding.targetID)->GetComponent<TransformComponent>();
	}

	// 원래 자세는 기존 형식과 같이 모델 배치표의 값이다.
	std::vector<Transform> restPose;
	restPose.reserve(model.sceneNodes.size());
	for (const GLTFSceneNode& sceneNode : model.sceneNodes)
	{
		restPose.push_back(sceneNode.localTransform);
	}

	std::unique_ptr<AnimationComponent> animationComponent = std::make_unique<AnimationComponent>();
	animationComponent->SetNodeTransforms(std::move(nodeTransforms));
	animationComponent->SetRestPose(std::move(restPose));
	object.AddComponent(std::move(animationComponent));
}
