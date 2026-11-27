#pragma once

#include "GameplaySpace.h"
#include "GrayboxLayout.h"
#include <array>

/** 한 층 탈출 프로토타입의 배치와 목표. 월드/렌더러 없이 진행 규칙을 검사한다. */
struct EscapeLayout
{
	Vector3 keyPosition{ -1650.0f, 0.0f, 1100.0f };
	Vector3 doorPosition{ 800.0f, 0.0f, -1100.0f };
	Vector3 escapePosition{ 1750.0f, 0.0f, -1100.0f };
	float interactionDistance = 220.0f;
	float escapeRadius = 180.0f;
	static constexpr Vector3 ElevatorPosition{1700.0f, 640.0f, 1350.0f};
	static constexpr float ElevatorHalfWidth = 130.0f;
	static constexpr std::array<Vector3, 4> Exhibits{{ {-1100.0f, 0.0f, -200.0f}, {1100.0f, 0.0f, 200.0f}, {-300.0f, 0.0f, 700.0f}, {500.0f, 0.0f, -700.0f} }};
	static constexpr std::array<Vector3, 2> Tables{{ {-350.0f, 0.0f, -300.0f}, {-1650.0f, 0.0f, 600.0f} }};

	std::vector<PrototypeObstacle> Partitions() const
	{
		std::vector<PrototypeObstacle> result{
			{ {-825.0f, 0.0f, -1800.0f}, {-775.0f, 520.0f, 700.0f} },
			{ {-825.0f, 0.0f, 1300.0f}, {-775.0f, 520.0f, 1800.0f} },
			{ {775.0f, 0.0f, -1800.0f}, {825.0f, 520.0f, -1400.0f} },
			{ {775.0f, 0.0f, -800.0f}, {825.0f, 520.0f, 1800.0f} }
		};
		result.insert(result.end(),GrayboxLayout::Screens.begin(),GrayboxLayout::Screens.end());
		return result;
	}
	PrototypeObstacle Door() const { return { doorPosition + Vector3{-25.0f, 0.0f, -300.0f}, doorPosition + Vector3{25.0f, 430.0f, 300.0f} }; }
};

enum class EscapeStage { FindKey, ReturnToExit, ReachExit, Cleared };

struct EscapeProgress
{
	EscapeStage stage = EscapeStage::FindKey;
	bool TakeKey() { if (stage != EscapeStage::FindKey) { return false; } stage = EscapeStage::ReturnToExit; return true; }
	bool UnlockExit() { if (stage != EscapeStage::ReturnToExit) { return false; } stage = EscapeStage::ReachExit; return true; }
	bool IsDoorOpen() const { return stage == EscapeStage::ReachExit || stage == EscapeStage::Cleared; }
	bool HasKey() const { return stage != EscapeStage::FindKey; }
	bool TryEscapeElevator(const Vector3& position, PlayerMode mode)
	{
		const Vector3 offset = position - EscapeLayout::ElevatorPosition;
		if (stage == EscapeStage::Cleared || (mode != PlayerMode::Moving && mode != PlayerMode::Fallen)
			|| std::abs(offset.y) > 5.0f || std::abs(offset.x) > EscapeLayout::ElevatorHalfWidth
			|| std::abs(offset.z) > EscapeLayout::ElevatorHalfWidth) { return false; }
		stage = EscapeStage::Cleared;
		return true;
	}
	bool TryEscape(const Vector3& position, PlayerMode mode, const EscapeLayout& layout)
	{
		if (stage != EscapeStage::ReachExit || mode != PlayerMode::Moving || position.x <= layout.doorPosition.x + 100.0f || PrototypeMath::Distance(position, layout.escapePosition) > layout.escapeRadius) { return false; }
		stage = EscapeStage::Cleared;
		return true;
	}
	const char* Objective() const
	{
		switch (stage)
		{
		case EscapeStage::FindKey: return "Find the brass key in the west storage room";
		case EscapeStage::ReturnToExit: return "Return to the locked exit and use the key";
		case EscapeStage::ReachExit: return "Reach the hidden elevator upstairs";
		case EscapeStage::Cleared: return "You escaped the exhibition";
		}
		return "";
	}
};

/** HUD와 실제 입력이 같은 목표 선택 판정을 사용한다. */
inline int FindEscapeInteraction(const EscapeProgress& progress, const EscapeLayout& layout, const GameplaySpace& space,
	const Vector3& position, PlayerMode mode, float viewYaw, std::size_t doorObstacle)
{
	if (mode != PlayerMode::Moving || progress.stage == EscapeStage::Cleared || progress.IsDoorOpen()) { return 0; }
	const bool keyTarget = !progress.HasKey() && PrototypeMath::Distance(position, layout.keyPosition) <= layout.interactionDistance;
	const PrototypeObstacle door = layout.Door();
	// 넓은 문은 중심이 아닌 가장 가까운 표면을 선택한다. 달려와 붙어도 같은 거리/시야 규칙을 쓴다.
	const Vector3 target = keyTarget ? layout.keyPosition : Vector3{
		std::clamp(position.x, door.minimum.x, door.maximum.x), 0.0f,
		std::clamp(position.z, door.minimum.z, door.maximum.z)};
	Vector3 delta = target - position;
	delta.y = 0.0f;
	const float distance = PrototypeMath::Length(delta);
	if (distance > layout.interactionDistance) { return 0; }
	if (distance > 1.0f && (delta / distance).Dot(PrototypeMath::Direction(viewYaw)) < 0.15f) { return 0; }
	// 대상 문 자신만 제외한다. 다른 벽/사물은 계속 상호작용을 가린다.
	const std::optional<std::size_t> ignore = keyTarget ? std::nullopt : std::optional<std::size_t>{doorObstacle};
	if (distance > 1.0f && space.CameraDistance(position + Vector3{0.0f, 65.0f, 0.0f}, delta / distance, distance, 0.0f, ignore) < distance - 5.0f) { return 0; }
	return keyTarget ? 1 : progress.HasKey() ? 2 : 3;
}

bool LoadEscapeLayout(EscapeLayout& layout);
