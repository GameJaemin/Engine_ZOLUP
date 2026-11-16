#pragma once

#include "Asset.h"
#include "Core/Math/Transform.h"

#include <cstddef>
#include <string>
#include <vector>

/** 키와 키 사이 값을 채우는 방식이다. */
enum class AnimationInterpolation
{
	// 다음 키 시각까지 앞 키의 값을 그대로 유지한다.
	Step,
	// 앞뒤 키를 시간 비율로 섞는다. 회전은 구면 선형 보간(slerp)을 쓴다.
	Linear
};

/** 노드 하나의 키 목록이다. 키 하나가 그 시각의 위치, 회전, 크기를 함께 가진다. */
struct AnimationNodeTrack
{
	// 값을 바꿀 노드의 glTF nodes 번호. GLTFModel::sceneNodes의 번호와 같다.
	std::size_t node = 0;
	// 위치, 회전, 크기에 함께 쓰는 보간 방식
	AnimationInterpolation interpolation = AnimationInterpolation::Linear;
	// 키 시각(초). 하나 이상이며 0 이상이고 뒤로 갈수록 커진다.
	std::vector<float> times;
	// times와 같은 개수와 순서의 부모 기준 값. 엔진 좌표계(왼손, cm)이며 회전은 단위 쿼터니언이다.
	std::vector<Transform> values;

	/** time초의 부모 기준 값을 반환한다. 첫 키보다 앞(NaN 포함)이면 첫 키, 마지막 키보다 뒤면 마지막 키 값이다. */
	Transform Sample(float time) const;
};

/**
 * glTF animation 하나를 엔진 좌표계(왼손, cm)로 바꾼 클립이다. 같은 파일에서 불러온 Model의 노드에만 쓴다.
 * 시각 0부터 duration까지 재생하며, 첫 키보다 앞이나 마지막 키보다 뒤에서는 가장 가까운 키의 값을 쓴다.
 */
class AnimationClip : public Asset
{
public:
	/** UTF-8 표시 이름과 저장소 루트 기준 상대 경로를 받는다. 경로는 "파일경로#번호" 모양이거나 비어 있다. */
	AnimationClip(std::string newName, std::string newPath);

public:
	// 애니메이션이 있는 노드마다 하나다. 같은 node를 가진 트랙은 둘 이상 없다.
	std::vector<AnimationNodeTrack> tracks;
	// 클립 길이(초). 모든 트랙 마지막 키 시각 중 가장 큰 값이다.
	float duration = 0.0f;
};
