#include "BoxColliderComponent.h"

#include "Component/RigidBodyComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"
#include "World.h"

#include <cmath>

namespace
{
	bool IsFinite(const Vector3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool IsSameVector(const Vector3& a, const Vector3& b)
	{
		return a.x == b.x && a.y == b.y && a.z == b.z;
	}

	/** 같은 Transform에서 계산한 설정은 같은 값이 나오므로 성분을 정확히 비교한다. */
	bool IsSameSettings(const BoxBodySettings& a, const BoxBodySettings& b)
	{
		return IsSameVector(a.positionCm, b.positionCm) &&
			a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y && a.rotation.z == b.rotation.z && a.rotation.w == b.rotation.w &&
			IsSameVector(a.boxCenterCm, b.boxCenterCm) &&
			IsSameVector(a.halfExtentsCm, b.halfExtentsCm);
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

bool BoxColliderComponent::SyncStaticBody()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr || registeredPhysicsScene == nullptr) { return true; }
	// 부착은 월드 등록보다 먼저 일어날 수 있고, 삭제 예정 객체는 실제 소멸 때 바디를 해제한다.
	if (owner->IsPendingDestroy() || !owner->GetWorld().ContainsGameObject(owner)) { return true; }

	const RigidBodyComponent* rigidBody = owner->GetComponent<RigidBodyComponent>();
	if (rigidBody != nullptr && rigidBody->IsSimulatingPhysics())
	{
		// 이 박스는 동적 바디가 쓰므로 정적 바디를 비워 둔다.
		UnregisterStaticBody();
		return true;
	}

	const BoxBodySettings settings = MakeBodySettings(owner->GetComponent<TransformComponent>()->GetWorldTransform());
	// 바디가 있거나 이미 실패한 입력이면 다시 시도하지 않는다.
	if (lastStaticBodySettings.has_value() && IsSameSettings(*lastStaticBodySettings, settings)) { return true; }

	UnregisterStaticBody();
	lastStaticBodySettings = settings;
	const std::optional<StaticBoxID> boxID = registeredPhysicsScene->AddStaticBox(settings);
	if (!boxID.has_value()) { return false; }

	registeredBoxID = boxID;
	return true;
}

BoxBodySettings BoxColliderComponent::MakeBodySettings(const Transform& bodyTransform) const
{
	const Vector3& scale = bodyTransform.scale;
	return BoxBodySettings{
		.positionCm = bodyTransform.position,
		.rotation = bodyTransform.rotation,
		// 음수 배율은 중심 오프셋의 방향을 뒤집지만 반크기는 거리다.
		.boxCenterCm = { localCenter.x * scale.x, localCenter.y * scale.y, localCenter.z * scale.z },
		.halfExtentsCm = {
			localHalfExtents.x * std::abs(scale.x),
			localHalfExtents.y * std::abs(scale.y),
			localHalfExtents.z * std::abs(scale.z)
		}
	};
}

void BoxColliderComponent::UnregisterStaticBody()
{
	lastStaticBodySettings.reset();
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
