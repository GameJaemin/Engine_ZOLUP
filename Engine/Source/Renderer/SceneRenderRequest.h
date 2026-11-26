#pragma once

#include "Camera/CameraViewInfo.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

class RenderScene;
class TextureRenderTarget2D;

// 후처리 하나에 넘길 수 있는 값의 최대 개수다. 렌더러의 액션 데이터 칸 수와 같다.
inline constexpr std::size_t MaxPostProcessParameters = 64;

/** 내장 후처리(톤맵, FXAA) 뒤에 실행할 화면 전체 후처리 하나다. */
struct PostProcessPass
{
	// 저장소 루트 기준 픽셀 셰이더 경로다. 셰이더는 g_SR0으로 이전 결과를 읽고 결과 색 하나를 출력한다.
	std::string shaderPath;

	// 셰이더의 ACTION_DATA_CB에 앞에서부터 순서대로 들어간다. 최대 MaxPostProcessParameters개다.
	std::vector<float> parameters;
};

/** 이번 프레임에 그릴 장면, 카메라, 출력 대상과 채도 및 후처리 설정이다. */
struct SceneRenderRequest
{
	// 같은 프레임의 렌더 제출 처리가 끝날 때까지 빌린다.
	std::reference_wrapper<RenderScene> scene;

	// 요청을 만들 때의 카메라 값을 복사한다.
	CameraViewInfo camera;

	// nullptr면 백버퍼다. 지정한 객체는 렌더 제출 처리가 끝날 때까지 빌린다.
	const TextureRenderTarget2D* target = nullptr;

	// 요청을 만들 때의 채도다. 0은 흑백, 1은 기본 채도다.
	float saturation = 1.0f;

	// 내장 후처리 뒤에 순서대로 실행한다. 비어 있으면 추가 후처리가 없다.
	std::vector<PostProcessPass> postProcessPasses;
};
