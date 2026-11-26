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
	 * 콜라이더는 하나여야 한다. 바디 원점은 객체 원점이고 질량 중심은 박스 중심이다.
	 * 시뮬레이션이 꺼져 있거나 이미 등록된 정적 또는 동적 바디가 있거나 등록에 실패하면 false다.
	 */
	[[nodiscard]] bool RegisterDynamicBody();

	/** 동적 바디로 시뮬레이션할지 반환한다. 기본값은 true다. */
	bool IsSimulatingPhysics() const { return simulatePhysics; }

	/** 시뮬레이션 여부를 저장한다. 값이 바뀌면 기존 동적 바디를 해제하고 다음 물리 갱신에서 맞는 바디를 다시 만든다. */
	void SetSimulatePhysics(bool enabled);

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
