#include "PhysicsProps.h"
#include "ObjectSoundComponent.h"

#include "Component/BoxColliderComponent.h"
#include "Component/RigidBodyComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "Prefab/Prefab.h"
#include "Renderer/Material.h"
#include "World.h"

#include <utility>

bool PhysicsProps::Initialize(World& world, std::shared_ptr<const Model> box, GameplaySpace& space, const PrototypeConfig& config)
{
	bindings.clear();
	for (int index = 0; index < 3; ++index)
	{
		MaterialParameters parameters;
		parameters.baseColor = index % 2 == 0 ? Vector3{0.38f,0.38f,0.38f} : Vector3{0.46f,0.46f,0.46f};
		parameters.metallic = 0.0f;
		parameters.roughness = 0.85f;
		const std::shared_ptr<const Material> material = std::make_shared<const Material>("Jolt crate", std::string(), std::move(parameters));
		Prefab prefab;
		prefab.model = box;
		prefab.meshMaterialOverrides = { material };
		Transform spawn;
		spawn.position = { -1750.0f, config.propDropHeight + index * 20.0f, -1050.0f + index * 170.0f };
		spawn.scale = { 0.8f, 0.8f, 0.8f };
		spawn.rotation = Quaternion::CreateFromEulerAngles({ 0.0f, index * 25.0f, index * 8.0f });
		GameObject* object = world.InstantiatePrefab(prefab, { std::string("Jolt crate"), spawn });
		if (!object) { return false; }
		object->AddComponent(std::make_unique<BoxColliderComponent>());
		object->AddComponent(std::make_unique<RigidBodyComponent>());
		bindings.push_back({ object->GetObjectID(), spawn, space.obstacles.size() });
		space.obstacles.push_back({});
	}
	SyncSpace(world, space);
	return true;
}

void PhysicsProps::Reset(World& world)
{
	for (const PhysicsPropBinding& binding : bindings)
	{
		GameObject* object = world.FindGameObject(binding.objectID);
		if (!object) { continue; }
		object->GetComponent<RigidBodyComponent>()->UnregisterDynamicBody();
		object->GetComponent<TransformComponent>()->SetTransform(binding.spawn);
	}
}

std::vector<Transform> PhysicsProps::Capture(const World& world) const
{
	std::vector<Transform> result;
	for (const auto& binding : bindings)
	{
		const GameObject* object = world.FindGameObject(binding.objectID);
		result.push_back(object ? object->GetComponent<TransformComponent>()->GetTransform() : binding.spawn);
	}
	return result;
}

void PhysicsProps::Restore(World& world, const std::vector<Transform>& transforms)
{
	if (transforms.size() != bindings.size()) { Reset(world); return; }
	for (std::size_t index=0; index<bindings.size(); ++index)
	{
		GameObject* object = world.FindGameObject(bindings[index].objectID);
		if (!object) { continue; }
		object->GetComponent<RigidBodyComponent>()->UnregisterDynamicBody();
		object->GetComponent<TransformComponent>()->SetTransform(transforms[index]);
	}
}

void PhysicsProps::SyncSpace(const World& world, GameplaySpace& space) const
{
	for (const PhysicsPropBinding& binding : bindings)
	{
		const GameObject* object = world.FindGameObject(binding.objectID);
		PrototypeObstacle& obstacle = space.obstacles[binding.obstacleIndex];
		if (!object) { obstacle.enabled = false; continue; }
		const TransformComponent* transform = object->GetComponent<TransformComponent>();
		const Matrix4x4 matrix = transform->GetWorldMatrix();
		const Vector3 center = transform->GetPosition();
		const Vector3 extent{
			50.0f * (std::abs(matrix.m[0][0]) + std::abs(matrix.m[1][0]) + std::abs(matrix.m[2][0])),
			50.0f * (std::abs(matrix.m[0][1]) + std::abs(matrix.m[1][1]) + std::abs(matrix.m[2][1])),
			50.0f * (std::abs(matrix.m[0][2]) + std::abs(matrix.m[1][2]) + std::abs(matrix.m[2][2]))
		};
		obstacle = { center - extent, center + extent, true };
	}
}

void PhysicsProps::EmitPlayerBumps(const World& world, const Vector3& player, float attemptedSpeed, ObjectSoundComponent& sounds) const
{
	for (std::size_t index=0;index<bindings.size();++index)
	{
		const GameObject* object=world.FindGameObject(bindings[index].objectID);
		if(!object) {continue;}
		const Vector3 center=object->GetComponent<TransformComponent>()->GetPosition();
		if(std::abs(center.y-player.y)>160.0f) {continue;}
		// 게임 이동이 상자 AABB에 막혔을 때의 접촉 시도다. Jolt 접촉 콜백을 가장하지 않는다.
		const Matrix4x4 matrix=object->GetComponent<TransformComponent>()->GetWorldMatrix();
		const float xExtent=50.0f*(std::abs(matrix.m[0][0])+std::abs(matrix.m[1][0])+std::abs(matrix.m[2][0]));
		const float zExtent=50.0f*(std::abs(matrix.m[0][2])+std::abs(matrix.m[1][2])+std::abs(matrix.m[2][2]));
		const Vector3 nearest{std::clamp(player.x,center.x-xExtent,center.x+xExtent),player.y,std::clamp(player.z,center.z-zExtent,center.z+zExtent)};
		if(PrototypeMath::Distance(nearest,player)<34.0f) {sounds.Trigger("crate"+std::to_string(index),"crate","Bump",center,attemptedSpeed);}
	}
}
