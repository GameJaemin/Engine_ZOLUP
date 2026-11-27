#pragma once

#include "GameplaySpace.h"

#include "Core/Math/Transform.h"
#include "ObjectID.h"

#include <memory>

class World;
class Model;

struct PhysicsPropBinding
{
	ObjectID objectID;
	Transform spawn;
	std::size_t obstacleIndex = 0;
};

/** Jolt 강체를 게임의 평지 질의 어댑터에 연결한다. 두 월드의 물리 계산을 중복 실행하지 않는다. */
class PhysicsProps
{
public:
	bool Initialize(World& world, std::shared_ptr<const Model> box, GameplaySpace& space, const PrototypeConfig& config);
	void Reset(World& world);
	void SyncSpace(const World& world, GameplaySpace& space) const;
	std::size_t GetCount() const { return bindings.size(); }

private:
	std::vector<PhysicsPropBinding> bindings;
};
