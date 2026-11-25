#pragma once

#include "Component.h"
#include "Physics/JoltPhysicsScene.h"

#include <optional>

class RigidBodyComponent final : public Component
{
public:
	~RigidBodyComponent() override;

	/**
	 * 배치를 마친 루트 객체의 BoxColliderComponent로 동적 바디를 등록한다.
	 * 콜라이더는 하나여야 하며 로컬 중심과 초기 회전은 각각 원점과 항등 회전이어야 한다.
	 * 시뮬레이션이 꺼져 있거나 이미 등록된 정적 또는 동적 바디가 있거나 등록에 실패하면 false다.
	 */
	[[nodiscard]] bool RegisterDynamicBody();

	/** 동적 바디로 시뮬레이션할지 반환한다. 기본값은 true다. */
	bool IsSimulatingPhysics() const { return simulatePhysics; }

	/** 시뮬레이션 여부를 저장한다. 이미 등록한 바디에는 영향이 없고 다음 등록부터 적용된다. */
	void SetSimulatePhysics(bool enabled) { simulatePhysics = enabled; }

	/** 이 컴포넌트가 등록한 동적 바디가 있는지 반환한다. */
	bool HasDynamicBody() const { return bodyID.has_value(); }

	/** 등록한 동적 바디를 해제한다. 바디가 없으면 바로 반환한다. */
	void UnregisterDynamicBody();

	/** JoltPhysicsScene이 스텝을 마친 뒤 호출한다. 소유 객체의 월드 위치와 회전을 반영하고 배율은 유지한다. */
	void ApplyPhysicsPose(const DynamicBoxPose& pose);

private:
	bool simulatePhysics = true;
	// 비소유. World는 컴포넌트보다 물리 씬을 늦게 소멸시킨다.
	JoltPhysicsScene* registeredScene = nullptr;
	std::optional<DynamicBoxID> bodyID;
};
