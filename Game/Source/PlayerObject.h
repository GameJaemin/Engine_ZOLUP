#pragma once

#include "Core/Math/EulerAngles.h"
#include "Core/Math/Vector3.h"
#include "GameObject.h"

class TransformComponent;
class World;

/** 방향키로 시점을 돌리고 WASD와 E/Q로 이동하는 1인칭 카메라를 가진다. Space로 위치를 출력한다. */
class PlayerObject : public GameObject
{
	// 생성과 소멸
public:
	/** 기본 TransformComponent를 조회하고 입력, 카메라 컴포넌트를 부착하며 키 바인딩을 등록한다. */
	explicit PlayerObject(World& targetWorld);

	// 조작
public:
	/**
	 * 이번 프레임의 마우스 상대 이동량을 장치 보고 단위로 받는다.
	 * +X는 오른쪽, +Y는 아래쪽이다.
	 */
	void AddMouseLookInput(int deltaX, int deltaY);

protected:
	/** 이번 프레임의 회전을 적용한 뒤 새 yaw를 기준으로 이동한다. deltaTime은 초 단위다. */
	void OnTick(float deltaTime) override;

private:
	/** label과 현재 위치를 cm 단위로 콘솔에 한 줄 출력한다. */
	void PrintPosition(const char* label) const;

private:
	// 비소유. GameObject 생성자가 부착한 TransformComponent를 가리킨다.
	TransformComponent* transform = nullptr;

	// 쿼터니언을 매 프레임 오일러 각으로 되돌리지 않고 조작 각도를 도 단위로 누적한다.
	EulerAngles viewRotation = {};

	// Held 바인딩으로 모아 OnTick에서 한 번 소비한다. x는 오른쪽, y는 위쪽, z는 앞쪽이다.
	Vector3 moveInput = {};
	// 프레임 단위 입력이다. 각도 변화량은 회전 속도와 경과 시간을 곱해서 구한다.
	float yawInput = 0.0f;
	float pitchInput = 0.0f;
	// 마우스 감도를 적용한 이번 프레임의 각도 변화량이며 OnTick에서 한 번 소비한다.
	float mouseYawDeltaDegrees = 0.0f;
	float mousePitchDeltaDegrees = 0.0f;
};
