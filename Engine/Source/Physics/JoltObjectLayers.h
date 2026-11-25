#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>

namespace JoltObjectLayers
{
	inline constexpr JPH::ObjectLayer Static = 0;
	inline constexpr JPH::ObjectLayer Character = 1;
	inline constexpr JPH::ObjectLayer Dynamic = 2;
	inline constexpr JPH::uint Count = 3;
}

/** JoltRuntime 초기화 뒤 생성한다. Static은 Character 및 Dynamic과 충돌 검사한다. */
class JoltObjectLayerPairFilter final : public JPH::ObjectLayerPairFilterTable
{
public:
	JoltObjectLayerPairFilter();
};
