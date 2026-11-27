#pragma once

#include "PrototypeTypes.h"
#include <optional>

struct PrototypeObstacle
{
	Vector3 minimum;
	Vector3 maximum;
	bool enabled = true;
	bool groundOnly = false;
	// 양수이면 이 높이 이하의 낮은 자세만 통과한다. 카메라를 막는 실물 벽은 아니다.
	float passHeight = 0.0f;
};

struct GroundPatch
{
	Vector3 minimum;
	Vector3 maximum;
	float startHeight = 0.0f;
	float endHeight = 0.0f;
	int steps = 0;
	bool Contains(const Vector3& point) const { return point.x >= minimum.x && point.x <= maximum.x && point.z >= minimum.z && point.z <= maximum.z; }
	float Height(const Vector3& point) const
	{
		const float t = std::clamp((point.z - minimum.z) / (std::max)(1.0f, maximum.z - minimum.z), 0.0f, 1.0f);
		return startHeight + (endHeight - startHeight) * (steps > 0 ? std::ceil(t * steps - 0.0001f) / steps : t);
	}
};

/** 테스트룸 전용 충돌/길찾기 어댑터. 렌더 객체나 다른 게임 컴포넌트를 소유하지 않는다. */
class GameplaySpace
{
public:
	std::vector<PrototypeObstacle> obstacles;
	std::vector<GroundPatch> groundPatches;
	std::optional<float> GroundHeight(const Vector3& point, float previousHeight) const;
	float halfWidth = 2200.0f;
	float halfDepth = 1800.0f;

	bool IsFree(const Vector3& position, float radius, float height) const;
	/** 몸체 전체가 직선 구간을 통과할 수 있는지 보수적으로 검사한다. */
	bool CanTraverse(const Vector3& start, const Vector3& end, float radius, float height) const;
	Vector3 Move(const Vector3& position, const Vector3& displacement, float radius, float height) const;
	/** 상호작용 대상 자신의 장애물만 제외할 수 있다. 일반 카메라 충돌은 모두 검사한다. */
	float CameraDistance(const Vector3& origin, const Vector3& direction, float distance, float radius, std::optional<std::size_t> ignoredObstacle = std::nullopt) const;
	std::vector<Vector3> FindPath(const Vector3& start, const Vector3& destination, float radius, float height) const;
};
