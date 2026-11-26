#pragma once

#include "Component.h"
#include "Core/Math/Vector3.h"
#include "Physics/JoltPhysicsScene.h"

#include <optional>

struct Transform;

class BoxColliderComponent final : public Component
{
public:
	~BoxColliderComponent() override;

	/** 소속 World의 물리 씬에 컴포넌트 주소를 등록한다. */
	void OnRegister() override;

	/**
	 * 정적 바디를 객체의 현재 월드 위치, 회전, 배율과 로컬 박스에 맞춘다. 매 프레임 호출해도 된다.
	 * 월드 미등록, 삭제 예정이거나 시뮬레이션이 켜진 RigidBodyComponent가 있으면 바디를 해제하고 true다.
	 * 입력이 마지막 시도와 같으면 아무것도 하지 않고 true다. 바뀌었으면 다시 만들고, 새 시도가 실패하면 false다.
	 * 부모 배율이 축마다 다르고 자식이 회전해 월드 행렬이 기울어지면 박스는 근삿값이 된다.
	 */
	[[nodiscard]] bool SyncStaticBody();

	/** 바디 원점의 Transform(위치 cm)에 이 박스를 배율과 함께 적용한 바디 설정을 반환한다. */
	BoxBodySettings MakeBodySettings(const Transform& bodyTransform) const;

	/** 이 컴포넌트가 등록한 정적 바디의 ID가 있는지 반환한다. */
	bool HasStaticBody() const { return registeredBoxID.has_value(); }

	/** 등록한 바디를 해제하고 마지막 시도 기록을 지운다. 다음 SyncStaticBody에서 다시 등록할 수 있다. */
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
	// 마지막으로 정적 바디 등록을 시도한 설정이다. 성공과 실패 모두 기록해 같은 입력은 다시 시도하지 않는다.
	std::optional<BoxBodySettings> lastStaticBodySettings;
};
