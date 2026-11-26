#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>

namespace JoltBroadPhaseLayers
{
	inline constexpr JPH::BroadPhaseLayer Static{ 0 };
	inline constexpr JPH::BroadPhaseLayer Character{ 1 };
	inline constexpr JPH::BroadPhaseLayer Dynamic{ 2 };
	inline constexpr JPH::uint Count = 3;
}

/** JoltRuntime 초기화 뒤 생성한다. 객체 레이어를 후보 검색 그룹으로 매핑한다. */
class JoltBroadPhaseLayerMapping final : public JPH::BroadPhaseLayerInterfaceTable
{
public:
	JoltBroadPhaseLayerMapping();
};
