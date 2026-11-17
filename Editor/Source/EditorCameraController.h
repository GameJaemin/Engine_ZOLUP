#pragma once

class EditorCamera;
class InputState;

/** 입력을 해석해 에디터 카메라의 위치, 회전 및 투영 설정을 갱신한다. */
class EditorCameraController
{
public:
	/**
	 * 호출부에서 입력을 허용하고 마우스 오른쪽 버튼을 누르는 동안 조작한다.
	 * deltaTime은 초 단위이며 키보드 이동 거리를 계산할 때 사용한다.
	 */
	void Update(EditorCamera& camera, const InputState& input, float deltaTime, bool allowInput) const;

private:
	float moveSpeed = 500.0f;
	// 마우스가 1픽셀 움직일 때 회전하는 각도다.
	float mouseSensitivity = 0.15f;
	// 원근 투영에서 휠을 앞으로 한 칸 굴릴 때 전진하는 거리(cm)다.
	float dollyDistancePerNotch = 100.0f;
	// 직교 투영에서 휠을 앞으로 한 칸 굴릴 때 세로 범위에 곱하는 배율이다.
	float orthographicZoomFactorPerNotch = 0.9f;
};
