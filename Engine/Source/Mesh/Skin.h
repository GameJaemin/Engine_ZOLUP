#pragma once

#include "Core/Math/Matrix4x4.h"

#include <cstddef>
#include <string>
#include <vector>

/** 스킨 메시를 휘게 하는 뼈 목록과 뼈마다의 역바인드 행렬이다. 같은 파일에서 불러온 Model의 노드에만 쓴다. */
struct Skin
{
	// 사람이 보기 위한 이름
	std::string name;
	// 뼈로 쓰는 노드의 배치표(sceneNodes) 번호. 하나 이상이며 중복이 없다. SkinWeights::joints는 이 배열의 번호다.
	std::vector<std::size_t> joints;
	// joints와 같은 개수와 순서다. 메시 좌표의 점을 그 뼈 기준 좌표로 옮기며, 엔진 좌표계(왼손, cm)이고 행 벡터 규칙이다.
	std::vector<Matrix4x4> inverseBindMatrices;
};
