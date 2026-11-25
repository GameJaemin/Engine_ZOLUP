#include "BoxColliderComponent.h"

#include "Component/RigidBodyComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Matrix4x4.h"
#include "GameObject.h"
#include "World.h"

#include <cmath>

namespace
{
	bool IsFinite(const Vector3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool HasOnlyAxisScaleAndTranslation(const Matrix4x4& matrix)
	{
		return matrix.m[0][1] == 0.0f && matrix.m[0][2] == 0.0f &&
			matrix.m[1][0] == 0.0f && matrix.m[1][2] == 0.0f &&
			matrix.m[2][0] == 0.0f && matrix.m[2][1] == 0.0f;
	}
}

BoxColliderComponent::~BoxColliderComponent()
{
	// World가 충돌 컴포넌트보다 늦게 물리 씬을 소멸시킨다.
	UnregisterStaticBody();
	if (registeredPhysicsScene != nullptr)
	{
		registeredPhysicsScene->RemoveBoxColliderComponent(this);
	}
}

void BoxColliderComponent::OnRegister()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr) { return; }

	JoltPhysicsScene& scene = owner->GetWorld().GetPhysicsScene();
	scene.AddBoxColliderComponent(this);
	registeredPhysicsScene = &scene;
}

bool BoxColliderComponent::RegisterStaticBody()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr || registeredPhysicsScene == nullptr || registeredBoxID.has_value()) { return false; }

	World& world = owner->GetWorld();
	if (!world.ContainsGameObject(owner) || owner->IsPendingDestroy()) { return false; }
	const RigidBodyComponent* rigidBody = owner->GetComponent<RigidBodyComponent>();
	if (rigidBody != nullptr && rigidBody->IsSimulatingPhysics()) { return false; }

	const TransformComponent* transform = owner->GetComponent<TransformComponent>();
	const Matrix4x4 worldMatrix = transform->GetWorldMatrix();
	// 정적 박스 등록 API는 월드 축 정렬 박스만 만들므로 축이 섞인 변환은 거절한다.
	if (!HasOnlyAxisScaleAndTranslation(worldMatrix)) { return false; }

	const Vector3 worldCenterCm{
		localCenter.x * worldMatrix.m[0][0] + worldMatrix.m[3][0],
		localCenter.y * worldMatrix.m[1][1] + worldMatrix.m[3][1],
		localCenter.z * worldMatrix.m[2][2] + worldMatrix.m[3][2]
	};
	// 음수 배율은 중심 오프셋의 방향을 뒤집지만 반크기는 거리다.
	const Vector3 worldHalfExtentsCm{
		localHalfExtents.x * std::abs(worldMatrix.m[0][0]),
		localHalfExtents.y * std::abs(worldMatrix.m[1][1]),
		localHalfExtents.z * std::abs(worldMatrix.m[2][2])
	};

	const std::optional<StaticBoxID> boxID = registeredPhysicsScene->AddStaticBox(worldCenterCm, worldHalfExtentsCm);
	if (!boxID.has_value()) { return false; }

	registeredBoxID = boxID;
	return true;
}

void BoxColliderComponent::UnregisterStaticBody()
{
	if (!registeredBoxID.has_value()) { return; }

	registeredPhysicsScene->RemoveStaticBox(*registeredBoxID);
	registeredBoxID.reset();
}

bool BoxColliderComponent::SetLocalCenter(const Vector3& newCenter)
{
	return SetLocalBox(newCenter, localHalfExtents);
}

bool BoxColliderComponent::SetLocalHalfExtents(const Vector3& newHalfExtents)
{
	return SetLocalBox(localCenter, newHalfExtents);
}

bool BoxColliderComponent::SetLocalBox(const Vector3& newCenter, const Vector3& newHalfExtents)
{
	if (!IsFinite(newCenter) || !IsFinite(newHalfExtents)) { return false; }
	if (newHalfExtents.x <= 0.0f || newHalfExtents.y <= 0.0f || newHalfExtents.z <= 0.0f) { return false; }

	localCenter = newCenter;
	localHalfExtents = newHalfExtents;
	return true;
}
