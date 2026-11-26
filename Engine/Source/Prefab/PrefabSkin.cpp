#include "Prefab/PrefabComponents.h"

#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Component/SkinComponent.h"
#include "Core/Log.h"
#include "GameObject.h"
#include "GLTF/GLTFLoader.h"

#include <cstddef>
#include <format>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

bool ReadSkinProperties(const rapidjson::Value& properties, std::string_view context, PrefabSkinDefinition& skin)
{
	const auto model = properties.FindMember("model");
	const std::string_view error = model == properties.MemberEnd() ? "is required" : ValidatePrefabAssetPath(model->value);
	if (!error.empty())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}.model {}", context, error));
		return false;
	}

	const auto sourceNode = properties.FindMember("sourceNode");
	if (sourceNode == properties.MemberEnd() || !sourceNode->value.IsUint64() || !std::in_range<std::size_t>(sourceNode->value.GetUint64()))
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}.sourceNode must be a non-negative integer in range", context));
		return false;
	}

	skin.modelPath.assign(model->value.GetString(), model->value.GetStringLength());
	skin.sourceNode = static_cast<std::size_t>(sourceNode->value.GetUint64());
	return true;
}

bool LoadPrefabSkin(AssetManager& assets, const PrefabSkinDefinition& definition, std::string_view context, PrefabSkin& outSkin)
{
	std::shared_ptr<const Model> model = assets.LoadModel(definition.modelPath);
	if (model == nullptr)
	{
		Log::Write(LogLevel::Error, "AssetManager", std::format("{}: could not load skin model {}", context, definition.modelPath));
		return false;
	}

	// SkinComponent는 이 노드의 skin 번호로 스킨을 찾으므로, 범위 안이고 스킨이 붙은 노드여야 한다.
	if (definition.sourceNode >= model->sceneNodes.size() || !model->sceneNodes[definition.sourceNode].skin.has_value())
	{
		Log::Write(LogLevel::Error, "AssetManager", std::format("{}: skin sourceNode {} is not a skinned node in {}", context, definition.sourceNode, definition.modelPath));
		return false;
	}

	outSkin = PrefabSkin{ std::move(model), definition.sourceNode };
	return true;
}

// 포즈는 자신이나 조상 중 같은 모델의 animation 컴포넌트에서 읽는다. 없으면 배치표의 원래 값을 쓴다.
void AttachPrefabSkin(GameObject& object, const PrefabSkin& skin, const std::vector<std::pair<GameObject*, const PrefabAnimation*>>& animations)
{
	const AnimationComponent* animation = nullptr;
	for (GameObject* current = &object; current != nullptr && animation == nullptr; current = current->GetParent())
	{
		for (const auto& [animationObject, animationData] : animations)
		{
			if (animationObject != current || animationData->model != skin.model) { continue; }

			animation = current->GetComponent<AnimationComponent>();
			break;
		}
	}

	std::unique_ptr<SkinComponent> skinComponent = std::make_unique<SkinComponent>();
	skinComponent->SetSkin(skin.model, skin.sourceNode, animation);
	object.AddComponent(std::move(skinComponent));
}
