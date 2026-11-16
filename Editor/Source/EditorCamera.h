#pragma once

#include "CameraViewInfo.h"

/** 에디터 시점의 월드 위치, 회전 및 투영 설정을 보관한다. */
class EditorCamera
{
public:
	CameraViewInfo GetCameraViewInfo() const;
	void SetPosition(const Vector3& newPosition);
	/** 월드 기준 단위 쿼터니언을 받는다. */
	void SetRotation(const Quaternion& newRotation);

	// 투영 설정은 유효하지 않은 입력을 받으면 값을 유지하고 false를 반환한다.
	bool SetProjectionMode(CameraProjectionMode newProjectionMode);
	bool SetVerticalFovDegrees(float newVerticalFovDegrees);
	/** 직교 투영의 전체 세로 길이(cm)를 설정한다. */
	bool SetOrthographicHeight(float newOrthographicHeight);
	/** 두 투영 모드에서 쓰는 near와 far 거리(cm)를 함께 설정한다. */
	bool SetClipDistances(float newNearClipDistance, float newFarClipDistance);

private:
	CameraViewInfo cameraViewInfo{
		.position = {},
		.rotation = {},
		.projectionMode = CameraProjectionMode::Perspective,
		.verticalFovDegrees = 60.0f,
		.orthographicHeight = 1000.0f,
		.nearClipDistance = 10.0f,
		.farClipDistance = 100000.0f
	};
};
