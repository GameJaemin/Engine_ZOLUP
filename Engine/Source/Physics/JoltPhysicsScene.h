#pragma once

#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector3.h"

#include <cstdint>
#include <memory>
#include <optional>

class BoxColliderComponent;
class RigidBodyComponent;
struct JoltPhysicsSceneState;

/** 등록한 정적 박스의 ID다. 발급한 물리 씬에서만 사용한다. */
struct StaticBoxID
{
	std::uint64_t value = 0;

	bool operator==(const StaticBoxID& other) const = default;
};

/** 등록한 동적 박스의 ID다. 발급한 물리 씬에서만 사용한다. */
struct DynamicBoxID
{
	std::uint64_t value = 0;

	bool operator==(const DynamicBoxID& other) const = default;
};

/** 바디 원점의 월드 배치와 그 원점 기준 박스다. 길이 단위는 cm다. */
struct BoxBodySettings
{
	// 바디 원점의 월드 위치와 회전이다. 객체 원점과 같다.
	Vector3 positionCm;
	Quaternion rotation;
	// 바디 원점에서 박스 중심까지의 거리다. 회전 전 바디 축 기준이며 배율을 이미 적용한 값이다.
	Vector3 boxCenterCm;
	// 각 축이 유한한 양수여야 한다.
	Vector3 halfExtentsCm;
};

/** 동적 박스의 월드 위치(cm)와 회전이다. */
struct DynamicBoxPose
{
	Vector3 positionCm;
	Quaternion rotation;
};

/** JoltRuntime 초기화 뒤 생성하고 런타임 종료 전에 소멸해야 한다. */
class JoltPhysicsScene final
{
public:
	/** 레이어 매핑과 필터를 연결해 바디가 없는 물리 월드를 만든다. */
	JoltPhysicsScene();
	~JoltPhysicsScene();

	JoltPhysicsScene(const JoltPhysicsScene&) = delete;
	JoltPhysicsScene& operator=(const JoltPhysicsScene&) = delete;
	JoltPhysicsScene(JoltPhysicsScene&&) = delete;
	JoltPhysicsScene& operator=(JoltPhysicsScene&&) = delete;

	/** 부착된 박스 컴포넌트의 주소를 보관한다. nullptr와 중복 등록은 무시한다. */
	void AddBoxColliderComponent(BoxColliderComponent* component);

	/** 소멸하는 박스 컴포넌트의 주소를 목록에서 제거한다. */
	void RemoveBoxColliderComponent(BoxColliderComponent* component);

	/**
	 * 등록된 각 박스의 정적 바디를 현재 배치와 로컬 박스에 맞춘다. 판단 기준은 BoxColliderComponent::SyncStaticBody를 따른다.
	 * 이번 호출에서 새로 시도한 등록이 하나라도 실패하면 false를 반환하되 나머지는 계속 처리한다.
	 */
	[[nodiscard]] bool PrepareStaticBodies();

	/**
	 * 월드에 등록된 삭제 예정이 아닌 객체 중 시뮬레이션이 켜진 RigidBodyComponent의 동적 바디를 생성한다.
	 * 처리 대상 중 하나라도 생성에 실패하면 false를 반환하되 나머지는 계속 처리한다.
	 */
	[[nodiscard]] bool PrepareDynamicBodies();

	/**
	 * 정적 바디를 현재 배치에 맞추고 새 동적 바디를 준비한 뒤 프레임 경과 시간(deltaTime, 초)을 누적해 1/60초 단위로 물리를 갱신한다.
	 * 스텝이 끝나면 컴포넌트와 연결된 동적 바디의 최종 위치와 회전을 객체에 반영한다.
	 * 남은 시간은 다음 호출에 사용한다. 입력, 바디 준비 또는 물리 스텝에 오류가 있으면 false다.
	 */
	[[nodiscard]] bool Update(float deltaTime);

	/**
	 * 정적 박스를 하나 등록한다.
	 * 성공하면 씬에서 사용할 ID를 반환한다. 입력이 유효하지 않거나 바디를 등록할 수 없으면 nullopt다.
	 */
	[[nodiscard]] std::optional<StaticBoxID> AddStaticBox(const BoxBodySettings& settings);

	/** 이 씬에서 발급한 ID로 박스를 제거하고 해제한다. 이미 제거된 ID는 무시한다. */
	void RemoveStaticBox(StaticBoxID boxID);

	/**
	 * 동적 박스를 등록하고 활성화한다. 질량 중심은 박스 중심이다.
	 * 입력이 유효하지 않거나 바디를 등록할 수 없으면 nullopt다.
	 */
	[[nodiscard]] std::optional<DynamicBoxID> AddDynamicBox(const BoxBodySettings& settings);

	/**
	 * AddDynamicBox와 RigidBodyComponent::RegisterDynamicBody가 사용하는 생성 경로다.
	 * component가 있으면 해당 컴포넌트는 이 씬에 속해야 하며 소멸 전에 반환된 ID로 바디를 해제한다.
	 * nullptr를 전달하면 객체에 연결하지 않고 바디만 등록한다.
	 */
	[[nodiscard]] std::optional<DynamicBoxID> CreateDynamicBox(const BoxBodySettings& settings, RigidBodyComponent* component);

	/** 현재 월드 위치와 회전을 조회한다. 이 씬에 등록된 박스가 없으면 nullopt다. */
	[[nodiscard]] std::optional<DynamicBoxPose> GetDynamicBoxPose(DynamicBoxID boxID) const;

	/** 현재 월드 위치를 cm로 반환한다. 이 씬에 등록된 박스가 없으면 nullopt다. */
	[[nodiscard]] std::optional<Vector3> GetDynamicBoxPositionCm(DynamicBoxID boxID) const;

	/** 이 씬에서 발급한 ID로 박스를 제거하고 해제한다. 이미 제거된 ID는 무시한다. */
	void RemoveDynamicBox(DynamicBoxID boxID);

private:
	std::unique_ptr<JoltPhysicsSceneState> state;
};
