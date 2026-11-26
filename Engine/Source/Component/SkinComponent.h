#pragma once

#include "Component.h"
#include "Core/Math/Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Model;
struct Transform;
class AnimationComponent;
class World;

/**
 * 같은 객체의 StaticMeshComponent가 그리는 스킨 메시를 모델의 뼈 포즈로 휘게 하는 연결이다.
 * 뼈는 객체가 아니라 포즈 배열의 칸이며, 뼈 행렬은 그릴 때 계산한다.
 */
class SkinComponent : public Component
{
	// 생성과 소멸
public:
	SkinComponent() = default;

	/** 구독한 World가 있으면 객체 삭제 알림 구독을 해제한다. */
	~SkinComponent() override;

	// 등록
public:
	/** 소속 World의 객체 삭제 알림을 구독한다. GameObject::AddComponent가 호출하며 World의 삭제 처리 중에는 부착하지 않는다. */
	void OnRegister() override;

private:
	// 소멸 시 구독을 해제하려고 보관한다. 비소유이며 World가 이 컴포넌트보다 오래 산다.
	World* destroyingWorld = nullptr;
	// 0이면 구독하지 않았다.
	std::uint64_t destroyingSubscriptionId = 0;

	// 스킨 연결
public:
	/**
	 * model의 nodeIndex번 노드(스킨이 붙은 메시 노드)로 연결한다. 스킨은 그 노드의 skin 번호로 찾는다.
	 * animation은 같은 World에 있는 같은 모델 노드의 포즈를 가진 재생 컴포넌트이며, nullptr면 배치표의 원래 값을 포즈로 쓴다.
	 * 부착 후 animation을 가진 객체가 삭제되면 nullptr로 바뀌어 원래 값을 쓴다.
	 */
	void SetSkin(std::shared_ptr<const Model> newModel, std::size_t newNodeIndex, const AnimationComponent* newAnimation);

	const std::shared_ptr<const Model>& GetModel() const { return model; }
	std::size_t GetNodeIndex() const { return nodeIndex; }

	/** 모델만 바꾼다. 노드 번호와 재생 컴포넌트는 그대로이며, 새 모델과 맞는지는 뼈 행렬을 계산할 때 확인한다. */
	void SetModel(std::shared_ptr<const Model> newModel);

	/** 메시 노드 번호만 바꾼다. 모델과 맞는지는 뼈 행렬을 계산할 때 확인한다. */
	void SetNodeIndex(std::size_t newNodeIndex);

	/** 포즈를 읽을 재생 컴포넌트만 바꾼다. nullptr면 배치표의 원래 값을 쓴다. 같은 World의 컴포넌트여야 한다. */
	void SetAnimation(const AnimationComponent* newAnimation);
	/** 포즈를 읽는 재생 컴포넌트다. 없으면 nullptr다. */
	const AnimationComponent* GetAnimation() const { return animation; }

private:
	// 배치표(부모 번호, 원래 값)와 스킨을 가진 원본. 같은 프리팹을 놓은 여러 객체가 함께 읽는다.
	std::shared_ptr<const Model> model;
	// 이 객체가 된 메시 노드의 배치표 번호
	std::size_t nodeIndex = 0;
	// 포즈를 읽을 재생 컴포넌트. 비소유이며 없으면 nullptr다.
	const AnimationComponent* animation = nullptr;

	// 뼈 행렬
public:
	/**
	 * joints 순서로 뼈마다 "역바인드 * 뼈의 모델 기준 행렬 * 메시 노드의 모델 기준 행렬의 역"을 계산한다.
	 * 엔진 좌표계(왼손, cm)와 행 벡터 규칙이며, 셰이더가 메시 객체의 월드 행렬을 뒤에 곱하면 정점이 뼈만 따라간다.
	 * 연결 전이거나 스킨, 포즈가 맞지 않거나 메시 노드 행렬의 역이 없으면 출력값을 유지하고 false를 반환한다.
	 */
	bool CalculateBoneMatrices(std::vector<Matrix4x4>& outBoneMatrices) const;

private:
	/** node의 이번 프레임 부모 기준 값이다. 재생 컴포넌트가 있으면 그 포즈, 없으면 배치표의 원래 값이다. */
	const Transform& GetNodeTransform(std::size_t node) const;

	/** node의 값부터 부모 쪽으로 올라가며 곱한 모델 기준 행렬이다. 프리팹 루트의 변환은 들어가지 않는다. */
	Matrix4x4 CalculateModelMatrix(std::size_t node) const;
};
