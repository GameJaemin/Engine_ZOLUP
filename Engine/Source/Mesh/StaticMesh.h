#pragma once

#include "Asset.h"
#include "Mesh/MeshVertex.h"

#include <cstdint>
#include <vector>

/**
 * 이름과 경로 및 CPU 정점과 인덱스 배열을 소유하는 정적 메시 에셋이다. 인덱스 3개가 삼각형 하나이며,
 * 바깥에서 볼 때 시계 방향으로 감긴 면이 앞면이다. GPU 자원은 소유하지 않는다.
 * 스킨 메시면 정점별 뼈 가중치(skinWeights)도 가진다. 어느 스킨의 뼈인지는 메시가 아니라 메시를 쓰는 노드가 정한다.
 */
class StaticMesh : public Asset
{
public:
	/**
	 * UTF-8 표시 이름과 저장소 루트 기준 상대 경로를 받는다. 파일 없이 생성하면 빈 경로를 사용한다.
	 * 파일 안의 여러 메시 중 하나면 경로 뒤에 "#번호"가 붙는다.
	 */
	StaticMesh(std::string newName, std::string newPath);

public:
	std::vector<MeshVertex> vertices;
	// 각 값은 vertices의 요소 번호다.
	std::vector<std::uint32_t> indices;
	// 비어 있으면 스킨이 없는 메시다. 있으면 vertices와 같은 개수와 순서다.
	std::vector<SkinWeights> skinWeights;
};
