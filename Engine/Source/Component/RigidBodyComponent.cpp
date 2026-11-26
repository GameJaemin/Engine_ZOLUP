#include "RigidBodyComponent.h"

#include "Component/BoxColliderComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"
#include "World.h"

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

	// 루트 객체이므로 부모 기준 Transform이 곧 월드 Transform이다.
	const Transform transform = owner->GetComponent<TransformComponent>()->GetTransform();
	JoltPhysicsScene& scene = world.GetPhysicsScene();
	const std::optional<DynamicBoxID> id = scene.CreateDynamicBox(box->MakeBodySettings(transform), this);
	if (!id.has_value()) { return false; }

	registeredScene = &scene;
	bodyID = id;
	return true;
}

void RigidBodyComponent::SetSimulatePhysics(bool enabled)
{
	if (simulatePhysics == enabled) { return; }

	simulatePhysics = enabled;
	// 켤 때는 콜라이더의 정적 바디를, 끌 때는 이 동적 바디를 비워야 다음 갱신에서 맞는 종류로 만든다.
	// 정적 바디는 BoxColliderComponent::SyncStaticBody가 해제한다.
	UnregisterDynamicBody();
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
