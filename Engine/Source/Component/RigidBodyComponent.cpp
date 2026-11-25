#include "RigidBodyComponent.h"

#include "Component/BoxColliderComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"
#include "World.h"

#include <cmath>

RigidBodyComponent::~RigidBodyComponent()
{
	UnregisterDynamicBody();
}

bool RigidBodyComponent::RegisterDynamicBody()
{
	GameObject* owner = GetGameObject();
	if (!simulatePhysics || owner == nullptr || bodyID.has_value() || owner->IsPendingDestroy() || owner->GetParent() != nullptr) { return false; }

	World& world = owner->GetWorld();
	if (!world.ContainsGameObject(owner) || owner->GetComponent<RigidBodyComponent>() != this) { return false; }

	const BoxColliderComponent* box = nullptr;
	for (const std::unique_ptr<Component>& component : owner->GetComponents())
	{
		const auto* collider = dynamic_cast<const BoxColliderComponent*>(component.get());
		if (collider == nullptr) { continue; }
		if (box != nullptr || collider->HasStaticBody()) { return false; }
		box = collider;
	}
	if (box == nullptr) { return false; }

	// 동적 박스 등록 API는 객체 원점에 회전 없이 바디를 생성한다.
	const Vector3 localCenter = box->GetLocalCenter();
	if (localCenter.x != 0.0f || localCenter.y != 0.0f || localCenter.z != 0.0f) { return false; }

	const Transform transform = owner->GetComponent<TransformComponent>()->GetTransform();
	const Quaternion& rotation = transform.rotation;
	if (rotation.x != 0.0f || rotation.y != 0.0f || rotation.z != 0.0f || std::abs(rotation.w) != 1.0f) { return false; }

	const Vector3 localHalfExtentsCm = box->GetLocalHalfExtents();
	const Vector3 halfExtentsCm{
		localHalfExtentsCm.x * std::abs(transform.scale.x),
		localHalfExtentsCm.y * std::abs(transform.scale.y),
		localHalfExtentsCm.z * std::abs(transform.scale.z)
	};

	JoltPhysicsScene& scene = world.GetPhysicsScene();
	const std::optional<DynamicBoxID> id = scene.CreateDynamicBox(transform.position, halfExtentsCm, this);
	if (!id.has_value()) { return false; }

	registeredScene = &scene;
	bodyID = id;
	return true;
}

void RigidBodyComponent::UnregisterDynamicBody()
{
	if (!bodyID.has_value()) { return; }

	registeredScene->RemoveDynamicBox(*bodyID);
	bodyID.reset();
	registeredScene = nullptr;
}

void RigidBodyComponent::ApplyPhysicsPose(const DynamicBoxPose& pose)
{
	GameObject* owner = GetGameObject();
	// 바디의 월드 위치와 회전은 루트 객체의 Transform에 직접 적용할 수 있다.
	if (owner == nullptr || owner->IsPendingDestroy() || owner->GetParent() != nullptr) { return; }

	TransformComponent* component = owner->GetComponent<TransformComponent>();
	Transform transform = component->GetTransform();
	transform.position = pose.positionCm;
	transform.rotation = pose.rotation;
	component->SetTransform(transform);
}
