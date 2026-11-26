#include "JoltPhysicsScene.h"

#include "Component/BoxColliderComponent.h"
#include "Component/RigidBodyComponent.h"
#include "GameObject.h"
#include "JoltBroadPhaseLayers.h"
#include "JoltObjectLayers.h"
#include "World.h"

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	bool IsFinite(const Vector3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	/** 입력이 유효하면 바디 원점에 회전과 중심 오프셋을 적용한 박스 바디 설정을 만든다. 아니면 nullopt다. */
	std::optional<JPH::BodyCreationSettings> MakeBoxCreationSettings(
		const BoxBodySettings& settings, JPH::EMotionType motionType, JPH::ObjectLayer layer)
	{
		constexpr float CmToMeters = 0.01f;
		const Vector3 position = settings.positionCm * CmToMeters;
		const Vector3 boxCenter = settings.boxCenterCm * CmToMeters;
		const Vector3 halfExtents = settings.halfExtentsCm * CmToMeters;
		if (!IsFinite(position) || !IsFinite(boxCenter) || !IsFinite(halfExtents)) { return std::nullopt; }
		if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f) { return std::nullopt; }

		const JPH::Quat rotation(settings.rotation.x, settings.rotation.y, settings.rotation.z, settings.rotation.w);
		const float rotationLength = rotation.Length();
		if (!std::isfinite(rotationLength) || rotationLength <= 0.0f) { return std::nullopt; }

		const JPH::RefConst<JPH::Shape> box = new JPH::BoxShape(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
		// 바디 원점은 객체 원점에 두고, 박스는 원점에서 중심 오프셋만큼 옮긴 모양으로 감싼다.
		const JPH::RefConst<JPH::Shape> shape = new JPH::RotatedTranslatedShape(
			JPH::Vec3(boxCenter.x, boxCenter.y, boxCenter.z), JPH::Quat::sIdentity(), box);
		// 행렬에서 분해한 회전은 길이가 1에서 조금 벗어날 수 있고, Jolt는 단위 쿼터니언을 요구한다.
		return JPH::BodyCreationSettings(
			shape.GetPtr(), JPH::RVec3(position.x, position.y, position.z), rotation / rotationLength, motionType, layer);
	}

	DynamicBoxPose ReadDynamicBoxPose(const JPH::BodyInterface& bodies, const JPH::BodyID& bodyID)
	{
		JPH::RVec3 position;
		JPH::Quat rotation;
		// 위치와 회전을 같은 바디 상태에서 읽어 한 결과로 반환한다.
		bodies.GetPositionAndRotation(bodyID, position, rotation);
		constexpr float MetersToCm = 100.0f;
		return DynamicBoxPose{
			{
				static_cast<float>(position.GetX() * MetersToCm),
				static_cast<float>(position.GetY() * MetersToCm),
				static_cast<float>(position.GetZ() * MetersToCm)
			},
			{ rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW() }
		};
	}
}

struct StaticBoxRegistration
{
	StaticBoxID boxID;
	JPH::BodyID bodyID;
};

struct DynamicBoxRegistration
{
	DynamicBoxID boxID;
	JPH::BodyID bodyID;
	// 비소유. 컴포넌트는 소멸할 때 바디와 이 등록 정보를 함께 제거한다.
	RigidBodyComponent* component = nullptr;
};

struct JoltPhysicsSceneState
{
	JoltBroadPhaseLayerMapping broadPhaseLayerMapping;
	JoltObjectLayerPairFilter objectLayerPairFilter;
	// 검색 표는 생성 시 매핑과 충돌 쌍 규칙을 읽으므로 두 설정을 먼저 생성한다.
	JPH::ObjectVsBroadPhaseLayerFilterTable objectVsBroadPhaseLayerFilter{ broadPhaseLayerMapping, JoltBroadPhaseLayers::Count, objectLayerPairFilter, JoltObjectLayers::Count };
	// 비소유. 컴포넌트가 부착 및 소멸 시 등록과 해제를 요청한다.
	std::vector<BoxColliderComponent*> boxColliderComponents;
	std::vector<StaticBoxRegistration> staticBoxes;
	std::vector<DynamicBoxRegistration> dynamicBoxes;
	// Jolt 바디 슬롯이 재사용되어도 제거된 씬 ID는 다시 발급하지 않는다.
	std::uint64_t nextStaticBoxID = 1;
	std::uint64_t nextDynamicBoxID = 1;
	double accumulatedTime = 0.0;
	// 물리 스텝에서 사용할 임시 메모리 할당기와 호출 스레드의 작업 실행기를 씬이 소유한다.
	JPH::TempAllocatorMalloc tempAllocator;
	JPH::JobSystemSingleThreaded jobSystem{ JPH::cMaxPhysicsJobs };
	// PhysicsSystem이 매핑과 필터를 참조하므로 가장 먼저 소멸하도록 마지막에 둔다.
	JPH::PhysicsSystem physicsSystem;

	JoltPhysicsSceneState()
	{
		constexpr JPH::uint MaxBodies = 64;
		constexpr JPH::uint AutoBodyMutexCount = 0;
		constexpr JPH::uint MaxBodyPairs = 256;
		constexpr JPH::uint MaxContactConstraints = 64;
		// Jolt 바디 생성 이후 등록 목록에 추가할 때 재할당하지 않도록 공간을 확보한다.
		staticBoxes.reserve(MaxBodies);
		dynamicBoxes.reserve(MaxBodies);
		physicsSystem.Init(MaxBodies, AutoBodyMutexCount, MaxBodyPairs, MaxContactConstraints, broadPhaseLayerMapping, objectVsBroadPhaseLayerFilter, objectLayerPairFilter);
	}
};

JoltPhysicsScene::JoltPhysicsScene()
	: state(std::make_unique<JoltPhysicsSceneState>())
{
}

JoltPhysicsScene::~JoltPhysicsScene()
{
	while (!state->dynamicBoxes.empty())
	{
		RemoveDynamicBox(state->dynamicBoxes.front().boxID);
	}
	while (!state->staticBoxes.empty())
	{
		RemoveStaticBox(state->staticBoxes.front().boxID);
	}
}

void JoltPhysicsScene::AddBoxColliderComponent(BoxColliderComponent* component)
{
	if (component == nullptr) { return; }

	std::vector<BoxColliderComponent*>& components = state->boxColliderComponents;
	if (std::ranges::find(components, component) != components.end()) { return; }

	components.push_back(component);
}

void JoltPhysicsScene::RemoveBoxColliderComponent(BoxColliderComponent* component)
{
	std::erase(state->boxColliderComponents, component);
}

bool JoltPhysicsScene::PrepareStaticBodies()
{
	bool allBodiesReady = true;
	for (BoxColliderComponent* collider : state->boxColliderComponents)
	{
		// 등록 대상 판단과 변경 감지는 콜라이더가 맡는다.
		if (!collider->SyncStaticBody())
		{
			allBodiesReady = false;
		}
	}
	return allBodiesReady;
}

bool JoltPhysicsScene::PrepareDynamicBodies()
{
	bool allBodiesReady = true;
	// RigidBody는 박스로 바디를 만들므로 박스 목록에서 소유 객체를 찾는다.
	for (BoxColliderComponent* collider : state->boxColliderComponents)
	{
		GameObject* owner = collider->GetGameObject();
		if (owner == nullptr || owner->IsPendingDestroy() || !owner->GetWorld().ContainsGameObject(owner)) { continue; }

		RigidBodyComponent* rigidBody = owner->GetComponent<RigidBodyComponent>();
		if (rigidBody == nullptr || !rigidBody->IsSimulatingPhysics() || rigidBody->HasDynamicBody()) { continue; }

		if (!rigidBody->RegisterDynamicBody())
		{
			allBodiesReady = false;
		}
	}
	return allBodiesReady;
}

bool JoltPhysicsScene::Update(float deltaTime)
{
	if (!std::isfinite(deltaTime) || deltaTime < 0.0f) { return false; }

	// 물리 스텝이 없는 프레임에도 새로 배치된 박스를 등록한다. 앞쪽이 실패해도 동적 바디 준비를 진행한다.
	const bool staticPrepared = PrepareStaticBodies();
	const bool dynamicPrepared = PrepareDynamicBodies();
	const bool prepared = staticPrepared && dynamicPrepared;

	constexpr float FixedDeltaTime = 1.0f / 60.0f;
	state->accumulatedTime += deltaTime;
	if (state->accumulatedTime < FixedDeltaTime) { return prepared; }

	do
	{
		const JPH::EPhysicsUpdateError result = state->physicsSystem.Update(
			FixedDeltaTime, 1, &state->tempAllocator, &state->jobSystem);
		// Jolt가 진행한 시간은 오류가 보고된 경우에도 다시 계산하지 않는다.
		state->accumulatedTime -= FixedDeltaTime;
		if (result != JPH::EPhysicsUpdateError::None) { return false; }
	} while (state->accumulatedTime >= FixedDeltaTime);

	// 스텝이 여러 번 진행된 프레임에는 마지막 바디 상태만 반영한다.
	const JPH::BodyInterface& bodies = state->physicsSystem.GetBodyInterface();
	for (const DynamicBoxRegistration& registration : state->dynamicBoxes)
	{
		if (registration.component == nullptr) { continue; }
		registration.component->ApplyPhysicsPose(ReadDynamicBoxPose(bodies, registration.bodyID));
	}
	return prepared;
}

std::optional<StaticBoxID> JoltPhysicsScene::AddStaticBox(const BoxBodySettings& settings)
{
	if (state->nextStaticBoxID == 0) { return std::nullopt; }

	const std::optional<JPH::BodyCreationSettings> creationSettings = MakeBoxCreationSettings(settings, JPH::EMotionType::Static, JoltObjectLayers::Static);
	if (!creationSettings.has_value()) { return std::nullopt; }

	JPH::BodyInterface& bodies = state->physicsSystem.GetBodyInterface();
	// 정적 바디는 활성화할 필요가 없고, DontActivate로 추가해도 충돌 검색에 등록된다.
	const JPH::BodyID bodyID = bodies.CreateAndAddBody(*creationSettings, JPH::EActivation::DontActivate);
	if (bodyID.IsInvalid()) { return std::nullopt; }

	const StaticBoxID boxID{ state->nextStaticBoxID };
	state->staticBoxes.push_back(StaticBoxRegistration{ boxID, bodyID });
	++state->nextStaticBoxID;
	return boxID;
}

void JoltPhysicsScene::RemoveStaticBox(StaticBoxID boxID)
{
	const auto registration = std::ranges::find(state->staticBoxes, boxID, &StaticBoxRegistration::boxID);
	if (registration == state->staticBoxes.end()) { return; }

	JPH::BodyInterface& bodies = state->physicsSystem.GetBodyInterface();
	// 등록된 바디는 충돌 검색에서 먼저 빼야 DestroyBody로 해제할 수 있다.
	bodies.RemoveBody(registration->bodyID);
	bodies.DestroyBody(registration->bodyID);
	// 등록 순서는 사용 계약에 포함되지 않으므로 마지막 항목으로 빈 자리를 채운다.
	*registration = state->staticBoxes.back();
	state->staticBoxes.pop_back();
}

std::optional<DynamicBoxID> JoltPhysicsScene::AddDynamicBox(const BoxBodySettings& settings)
{
	return CreateDynamicBox(settings, nullptr);
}

std::optional<DynamicBoxID> JoltPhysicsScene::CreateDynamicBox(const BoxBodySettings& settings, RigidBodyComponent* component)
{
	if (state->nextDynamicBoxID == 0) { return std::nullopt; }

	const std::optional<JPH::BodyCreationSettings> creationSettings = MakeBoxCreationSettings(settings, JPH::EMotionType::Dynamic, JoltObjectLayers::Dynamic);
	if (!creationSettings.has_value()) { return std::nullopt; }

	JPH::BodyInterface& bodies = state->physicsSystem.GetBodyInterface();
	// 동적 바디를 활성화해 이후 첫 물리 스텝에서 바로 시뮬레이션할 수 있게 한다.
	const JPH::BodyID bodyID = bodies.CreateAndAddBody(*creationSettings, JPH::EActivation::Activate);
	if (bodyID.IsInvalid()) { return std::nullopt; }

	const DynamicBoxID boxID{ state->nextDynamicBoxID };
	state->dynamicBoxes.push_back(DynamicBoxRegistration{ boxID, bodyID, component });
	++state->nextDynamicBoxID;
	return boxID;
}

std::optional<DynamicBoxPose> JoltPhysicsScene::GetDynamicBoxPose(DynamicBoxID boxID) const
{
	const auto& boxes = state->dynamicBoxes;
	const auto registration = std::ranges::find(boxes, boxID, &DynamicBoxRegistration::boxID);
	if (registration == boxes.end()) { return std::nullopt; }

	return ReadDynamicBoxPose(state->physicsSystem.GetBodyInterface(), registration->bodyID);
}

std::optional<Vector3> JoltPhysicsScene::GetDynamicBoxPositionCm(DynamicBoxID boxID) const
{
	const std::optional<DynamicBoxPose> pose = GetDynamicBoxPose(boxID);
	if (!pose.has_value()) { return std::nullopt; }
	return pose->positionCm;
}

void JoltPhysicsScene::RemoveDynamicBox(DynamicBoxID boxID)
{
	auto& boxes = state->dynamicBoxes;
	const auto registration = std::ranges::find(boxes, boxID, &DynamicBoxRegistration::boxID);
	if (registration == boxes.end()) { return; }

	JPH::BodyInterface& bodies = state->physicsSystem.GetBodyInterface();
	// 등록된 바디는 충돌 검색에서 먼저 빼야 DestroyBody로 해제할 수 있다.
	bodies.RemoveBody(registration->bodyID);
	bodies.DestroyBody(registration->bodyID);
	*registration = boxes.back();
	boxes.pop_back();
}
