#pragma once

#include "CameraViewInfo.h"

#include <functional>
#include <optional>

class InputState;
class RenderScene;

/** 현재 단일 백버퍼 경로에 제출할 장면과 카메라 값이다. */
struct SceneView
{
	// 장면은 같은 프레임의 렌더 제출이 끝날 때까지 살아 있어야 한다.
	std::reference_wrapper<RenderScene> scene;
	CameraViewInfo camera;
};

/** Engine::Run이 메인 스레드에서 빌려 쓰는 애플리케이션 실행 정책이다. */
class IApplication
{
public:
	virtual ~IApplication() = default;

	/** 창, DataHub 및 렌더러 초기화가 끝난 뒤 호출한다. */
	virtual bool OnInitialize() = 0;

	/** 메시지 처리 뒤 호출한다. input은 호출 중에 빌리며 deltaTime은 초 단위다. */
	virtual void OnUpdate(const InputState& input, float deltaTime) = 0;

	/** 갱신 후 렌더 가능한 프레임에서 호출한다. nullopt면 장면 제출을 건너뛴다. */
	virtual std::optional<SceneView> GetSceneView() = 0;

	/**
	 * OnInitialize가 반환했다면 정상 종료 또는 실패 반환 경로에서 한 번 호출한다.
	 * World 등 엔진 서비스에 의존하는 자원을 이 호출에서 해제한다.
	 * 이 시점에는 렌더러와 DataHub가 살아 있으며 GPU 완료를 기다리기 전이다.
	 */
	virtual void OnShutdown() noexcept = 0;
};
