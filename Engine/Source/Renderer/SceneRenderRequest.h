#pragma once

#include "Camera/CameraViewInfo.h"

#include <functional>

class RenderScene;
class TextureRenderTarget2D;

/** 이번 프레임에 그릴 장면, 카메라와 출력 대상이다. */
struct SceneRenderRequest
{
	// 같은 프레임의 렌더 제출 처리가 끝날 때까지 빌린다.
	std::reference_wrapper<RenderScene> scene;

	// 요청을 만들 때의 카메라 값을 복사한다.
	CameraViewInfo camera;

	// nullptr면 백버퍼다. 지정한 객체는 렌더 제출 처리가 끝날 때까지 빌린다.
	const TextureRenderTarget2D* target = nullptr;
};
