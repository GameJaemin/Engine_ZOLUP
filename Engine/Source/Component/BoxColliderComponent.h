#pragma once

#include "Component.h"
#include "Core/Math/Vector3.h"
#include "Physics/JoltPhysicsScene.h"

#include <optional>

class BoxColliderComponent final : public Component
{
public:
	~BoxColliderComponent() override;

	/** 소속 World의 물리 씬에 컴포넌트 주소를 등록한다. */
	void OnRegister() override;

	/**
	 * 소속 World에 등록된 객체의 배치를 끝낸 뒤 호출한다. 배율과 이동을 월드 박스에 반영한다.
	 * 시뮬레이션이 켜진 RigidBodyComponent가 붙어 있거나 이미 바디가 있거나 월드 행렬의 선형 부분이 대각선이 아니거나 바디 등록에 실패하면 false다.
	 * 바디는 등록 시점의 배치와 로컬 박스 값으로 유지된다.
	 */
	[[nodiscard]] bool RegisterStaticBody();

	/** 이 컴포넌트가 등록한 정적 바디의 ID가 있는지 반환한다. */
	bool HasStaticBody() const { return registeredBoxID.has_value(); }

	/** 등록한 바디를 해제한다. 바디가 없으면 바로 반환하며, 해제 뒤 다시 등록할 수 있다. */
	void UnregisterStaticBody();

	/** 로컬 중심을 cm 단위로 설정한다. 비유한 값은 거절하고 기존 박스를 유지한다. */
	[[nodiscard]] bool SetLocalCenter(const Vector3& newCenter);

	/** 로컬 반크기를 cm 단위로 설정한다. 각 축이 유한한 양수일 때만 저장한다. */
	[[nodiscard]] bool SetLocalHalfExtents(const Vector3& newHalfExtents);

	/** 두 입력이 모두 유효할 때만 로컬 중심과 반크기를 함께 저장한다. */
	[[nodiscard]] bool SetLocalBox(const Vector3& newCenter, const Vector3& newHalfExtents);

	/** 객체 원점에서 로컬 박스 중심까지의 오프셋을 cm 단위로 반환한다. */
	Vector3 GetLocalCenter() const { return localCenter; }

	/** 로컬 박스 중심에서 각 축의 면까지의 거리를 cm 단위로 반환한다. */
	Vector3 GetLocalHalfExtents() const { return localHalfExtents; }

private:
	Vector3 localCenter{};
	Vector3 localHalfExtents{ 50.0f, 50.0f, 50.0f };

	// 비소유. 컴포넌트 주소를 등록한 물리 씬이다.
	JoltPhysicsScene* registeredPhysicsScene = nullptr;
	// 바디 등록에 성공한 경우에만 이 씬에서 발급한 ID가 있다.
	std::optional<StaticBoxID> registeredBoxID;
};
