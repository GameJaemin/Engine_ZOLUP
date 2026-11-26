#pragma once

#include "Camera/CameraViewInfo.h"
#include "Component.h"
#include "Core/Math/Vector2.h"
#include "Renderer/SceneRenderRequest.h"

#include <string>
#include <vector>

/** 원근 또는 직교 투영 카메라의 설정값을 보관한다. 위치와 회전은 같은 객체의 TransformComponent를 따른다. */
class CameraComponent : public Component
{
	// 생성과 소멸
public:
	/** 활성 카메라로 지정되어 있었다면 소속 World의 지정을 해제한다. */
	~CameraComponent() override;

	// 투영 모드
public:
	/** 기본값은 원근이다. 모드를 바꿔도 각 모드의 설정값은 유지된다. */
	CameraProjectionMode GetProjectionMode() const { return projectionMode; }
	void SetProjectionMode(CameraProjectionMode newProjectionMode) { projectionMode = newProjectionMode; }

private:
	CameraProjectionMode projectionMode = CameraProjectionMode::Perspective;

	// 원근 투영 설정
public:
	/** 세로 시야각이며 단위는 도다. */
	float GetVerticalFovDegrees() const { return verticalFovDegrees; }

	/** 유한하며 0보다 크고 180보다 작은 값만 받는다. 조건을 벗어나면 기존 값을 유지하고 false를 반환한다. */
	bool SetVerticalFovDegrees(float newVerticalFovDegrees);

private:
	float verticalFovDegrees = 60.0f;

	// 직교 투영 설정
public:
	/** 보이는 영역의 전체 세로 길이이며 단위는 cm다. 가로는 이 값 * 화면 비율이다. */
	float GetOrthographicHeight() const { return orthographicHeight; }

	/** 유한하며 0보다 큰 값만 받는다. 조건을 벗어나면 기존 값을 유지하고 false를 반환한다. */
	bool SetOrthographicHeight(float newOrthographicHeight);

private:
	float orthographicHeight = 1000.0f;

	// 그리기 거리 (두 모드 공통)
public:
	/** 가까운 쪽 그리기 경계까지의 거리이며 단위는 cm다. */
	float GetNearClipDistance() const { return nearClipDistance; }

	/** 먼 쪽 그리기 경계까지의 거리이며 단위는 cm다. */
	float GetFarClipDistance() const { return farClipDistance; }

	/** 현재 far와 함께 SetClipDistances로 검사하고 near만 설정한다. */
	bool SetNearClipDistance(float newNearClipDistance);

	/** 현재 near와 함께 SetClipDistances로 검사하고 far만 설정한다. */
	bool SetFarClipDistance(float newFarClipDistance);

	/**
	 * near와 far를 함께 설정한다. 둘 다 유한하며 0 < near < far일 때만 받고,
	 * 조건을 벗어나면 기존 값을 유지하고 false를 반환한다.
	 */
	bool SetClipDistances(float newNearClipDistance, float newFarClipDistance);

	/** near와 far를 x, y(cm)로 함께 읽는다. */
	Vector2 GetClipDistances() const { return Vector2{ nearClipDistance, farClipDistance }; }

	/** x를 near, y를 far로 함께 설정한다. 검사와 실패 시 동작은 SetClipDistances(near, far)와 같다. */
	bool SetClipDistances(const Vector2& newClipDistances);

private:
	float nearClipDistance = 10.0f;
	float farClipDistance = 100000.0f;

	// 보이는 모양
public:
	/** 0은 흑백, 1은 기본 채도다. */
	float GetSaturation() const { return saturation; }

	/** 유한하며 0 이상 2 이하만 받는다. 조건을 벗어나면 기존 값을 유지하고 false를 반환한다. */
	bool SetSaturation(float newSaturation);

	/** 내장 후처리 뒤에 실행할 픽셀 셰이더의 저장소 루트 기준 경로다. 비어 있으면 추가 후처리가 없다. */
	const std::string& GetPostProcessShaderPath() const { return postProcessShaderPath; }

	/** 빈 문자열(끄기) 또는 실제로 있는 파일 경로만 받는다. 조건을 벗어나면 기존 값을 유지하고 false를 반환한다. */
	bool SetPostProcessShaderPath(const std::string& newPath);

	/** 셰이더의 ACTION_DATA_CB에 앞에서부터 순서대로 들어가는 값이다. */
	const std::vector<float>& GetPostProcessParameters() const { return postProcessParameters; }

	/** 모두 유한하고 MaxPostProcessParameters개 이하일 때만 받는다. 조건을 벗어나면 기존 값을 유지하고 false를 반환한다. */
	bool SetPostProcessParameters(std::vector<float> newParameters);

	/** 요청에 담을 후처리 목록을 만든다. 경로가 비어 있으면 빈 목록이다. */
	std::vector<PostProcessPass> MakePostProcessPasses() const;

private:
	float saturation = 1.0f;
	std::string postProcessShaderPath;
	std::vector<float> postProcessParameters;

	// 시점 값
public:
	/**
	 * 같은 객체의 부모까지 합친 월드 위치와 회전, 이 카메라의 투영 설정으로 시점 값을 채운다. 크기는 쓰지 않는다.
	 * 부착 전이거나 TransformComponent가 없으면 출력값을 유지하고 false를 반환한다.
	 */
	bool GetCameraViewInfo(CameraViewInfo& outCameraViewInfo) const;
};
