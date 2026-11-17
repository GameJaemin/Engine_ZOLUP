#pragma once

#include "Component.h"
#include "Core/Math/Matrix4x4.h"

#include <cstddef>
#include <memory>
#include <vector>

struct Model;
struct Transform;
class AnimationComponent;

/**
 * 같은 객체의 StaticMeshComponent가 그리는 스킨 메시를 모델의 뼈 포즈로 휘게 하는 연결이다.
 * 뼈는 객체가 아니라 포즈 배열의 칸이며, 뼈 행렬은 그릴 때 계산한다.
 */
class SkinComponent : public Component
{
	// 스킨 연결
public:
	/**
	 * model의 nodeIndex번 노드(스킨이 붙은 메시 노드)로 연결한다. 스킨은 그 노드의 skin 번호로 찾는다.
	 * animation은 같은 모델 노드의 포즈를 가진 재생 컴포넌트이며, nullptr면 배치표의 원래 값을 포즈로 쓴다.
	 * animation을 가진 객체는 이 컴포넌트보다 먼저 삭제하지 않는다.
	 */
	void SetSkin(std::shared_ptr<const Model> newModel, std::size_t newNodeIndex, const AnimationComponent* newAnimation);

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
