#pragma once

#include "PrototypeTypes.h"

struct PrototypeObstacle
{
	Vector3 minimum;
	Vector3 maximum;
	bool enabled = true;
};

/** 테스트룸 전용 충돌/길찾기 어댑터. 렌더 객체나 다른 게임 컴포넌트를 소유하지 않는다. */
class GameplaySpace
{
public:
	std::vector<PrototypeObstacle> obstacles;
	float halfWidth = 2200.0f;
	float halfDepth = 1800.0f;

	bool IsFree(const Vector3& position, float radius, float height) const;
	/** 몸체 전체가 직선 구간을 통과할 수 있는지 보수적으로 검사한다. */
	bool CanTraverse(const Vector3& start, const Vector3& end, float radius, float height) const;
	Vector3 Move(const Vector3& position, const Vector3& displacement, float radius, float height) const;
	float CameraDistance(const Vector3& origin, const Vector3& direction, float distance, float radius) const;
	std::vector<Vector3> FindPath(const Vector3& start, const Vector3& destination, float radius, float height) const;
};
