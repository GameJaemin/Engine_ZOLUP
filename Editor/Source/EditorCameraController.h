#pragma once

class EditorCamera;
class InputState;

/** 입력을 해석해 에디터 카메라의 회전, 위치와 투영 설정을 갱신한다. */
class EditorCameraController
{
public:
	/**
	 * 호출부에서 입력을 허용하고 마우스 오른쪽 버튼을 누르는 동안 Raw 이동량으로 회전하고 키와 휠로 이동한다.
	 * deltaTime은 초 단위이며 키보드 이동 거리를 계산할 때 사용한다.
	 */
	void Update(EditorCamera& camera, const InputState& input, float deltaTime, bool allowInput) const;

private:
	float moveSpeed = 500.0f;
	// Raw 이동량 한 단위당 회전 각도(도)다. 게임 시점과 같은 값으로 시작한다.
	float mouseSensitivity = 0.15f;
	// 원근 투영에서 휠을 앞으로 한 칸 굴릴 때 전진하는 거리(cm)다.
	float dollyDistancePerNotch = 100.0f;
	// 직교 투영에서 휠을 앞으로 한 칸 굴릴 때 세로 범위에 곱하는 배율이다.
	float orthographicZoomFactorPerNotch = 0.9f;
};
