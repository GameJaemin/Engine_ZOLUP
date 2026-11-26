#include "PhysicsProps.h"

#include "Component/BoxColliderComponent.h"
#include "Component/RigidBodyComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "Prefab/Prefab.h"
#include "Renderer/Material.h"
#include "World.h"

bool PhysicsProps::Initialize(World& world, std::shared_ptr<const Model> box, GameplaySpace& space, const PrototypeConfig& config)
{
	bindings.clear();
	for (int index = 0; index < 3; ++index)
	{
		std::shared_ptr<Material> material = std::make_shared<Material>();
		material->baseColor = index % 2 == 0 ? Vector3{ 0.48f, 0.23f, 0.12f } : Vector3{ 0.1f, 0.4f, 0.45f };
		material->metallic = 0.0f;
		material->roughness = 0.85f;
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
