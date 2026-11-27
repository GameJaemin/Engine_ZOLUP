#include "SkinComponent.h"

#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"
#include "Mesh/Skin.h"
#include "World.h"

#include <optional>
#include <utility>

SkinComponent::~SkinComponent()
{
	if (destroyingWorld != nullptr)
	{
		destroyingWorld->UnsubscribeGameObjectDestroying(destroyingSubscriptionId);
	}
}

void SkinComponent::OnRegister()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr) { return; }

	World& world = owner->GetWorld();
	// 알림은 실제 파괴 전에 오므로 재생 컴포넌트가 살아 있을 때 비교하고 지운다.
	destroyingSubscriptionId = world.SubscribeGameObjectDestroying([this](const GameObject& destroyed) noexcept
	{
		if (animation != nullptr && animation->GetGameObject() == &destroyed) { animation = nullptr; }
	});
	destroyingWorld = &world;
}

void SkinComponent::SetSkin(std::shared_ptr<const Model> newModel, std::size_t newNodeIndex, const AnimationComponent* newAnimation)
{
	model = std::move(newModel);
	nodeIndex = newNodeIndex;
	animation = newAnimation;
}

void SkinComponent::SetModel(std::shared_ptr<const Model> newModel)
{
	model = std::move(newModel);
}

void SkinComponent::SetNodeIndex(std::size_t newNodeIndex)
{
	nodeIndex = newNodeIndex;
}

void SkinComponent::SetAnimation(const AnimationComponent* newAnimation)
{
	animation = newAnimation;
}

bool SkinComponent::CalculateBoneMatrices(std::vector<Matrix4x4>& outBoneMatrices) const
{
	if (model == nullptr || nodeIndex >= model->sceneNodes.size()) { return false; }

	const std::optional<std::size_t>& skinIndex = model->sceneNodes[nodeIndex].skin;
	if (!skinIndex.has_value()) { return false; }

	// 포즈는 재생 컴포넌트 모델의 노드 번호 순서라, 같은 모델이어야 이 스킨의 노드 번호와 맞는다.
	if (animation != nullptr && animation->GetModel() != model) { return false; }

	// 스킨 번호와 joints 번호의 범위는 로더가 확인했다.
	const Skin& skin = *model->skins[*skinIndex];

	// 모든 뼈에 같이 쓰므로 한 번만 구한다.
	Matrix4x4 meshModelInverse;
	if (!CalculateModelMatrix(nodeIndex).GetAffineInverse(meshModelInverse)) { return false; }

	std::vector<Matrix4x4> boneMatrices;
	boneMatrices.reserve(skin.joints.size());
	for (std::size_t i = 0; i < skin.joints.size(); ++i)
	{
		// 행 벡터 규칙이라 먼저 적용하는 것이 왼쪽이다. 역바인드, 뼈의 모델 기준 행렬, 메시 모델 기준 행렬의 역 순이다.
		boneMatrices.push_back(skin.inverseBindMatrices[i] * CalculateModelMatrix(skin.joints[i]) * meshModelInverse);
	}

	outBoneMatrices = std::move(boneMatrices);
	return true;
}

const Transform& SkinComponent::GetNodeTransform(std::size_t node) const
{
	if (animation != nullptr) { return animation->GetPose()[node]; }
	return model->sceneNodes[node].localTransform;
}

Matrix4x4 SkinComponent::CalculateModelMatrix(std::size_t node) const
{
	// 자기 행렬에 부모, 조부모의 행렬을 차례로 오른쪽에 곱한다. 로더가 부모 사슬에 순환이 없음을 확인했다.
	Matrix4x4 matrix = GetNodeTransform(node).ToMatrix();
	std::optional<std::size_t> parent = model->sceneNodes[node].parent;
	while (parent.has_value())
	{
		matrix = matrix * GetNodeTransform(*parent).ToMatrix();
		parent = model->sceneNodes[*parent].parent;
	}
	return matrix;
}
