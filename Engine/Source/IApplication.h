#pragma once

#include "Renderer/SceneRenderRequest.h"

#include <optional>

class Engine;
class InputState;

/** Engine::Run이 메인 스레드에서 빌려 쓰는 애플리케이션 실행 정책이다. */
class IApplication
{
public:
	virtual ~IApplication() = default;

	/**
	 * Engine::Run이 창과 렌더러를 초기화하기 전에 한 번 읽는다. 실행 중 바뀐 값은 반영하지 않는다.
	 * true인데 GUI 준비에 실패하면 Run은 1을 반환한다.
	 */
	virtual bool UsesGui() const noexcept { return false; }

	/**
	 * 창, DataHub 및 렌더러 초기화가 끝난 뒤 호출한다.
	 * engine은 OnShutdown이 끝날 때까지 빌려 사용할 수 있다.
	 * 보관한 Engine 포인터는 OnShutdown에서 비운다.
	 */
	virtual bool OnInitialize(Engine& engine) = 0;

	/**
	 * 메시지 처리 뒤 호출한다. input은 호출 중에 빌리며 deltaTime은 초 단위다.
	 * GUI를 쓰는 앱은 렌더 가능한 프레임에서 ImGui 프레임이 열린 채 호출되지만, UI 호출은 OnGui에서 한다.
	 */
	virtual void OnUpdate(const InputState& input, float deltaTime) = 0;

	/**
	 * UsesGui가 true이고 GUI 준비에 성공했을 때, 렌더 가능한 프레임마다 OnUpdate 다음에 호출한다.
	 * ImGui 프레임이 열려 있는 동안 호출하므로 ImGui UI 호출은 이 함수 안에서만 한다.
	 */
	virtual void OnGui() {}

	/**
	 * 렌더 가능한 프레임에서 OnGui 이후 호출한다.
	 * nullopt면 장면을 그리지 않는다. 반환한 참조와 포인터는 같은 프레임에 소비한다.
	 */
	virtual std::optional<SceneRenderRequest> GetRenderRequest() = 0;

	/**
	 * OnInitialize가 반환했다면 정상 종료 또는 실패 반환 경로에서 한 번 호출한다.
	 * World 등 엔진 서비스에 의존하는 자원을 이 호출에서 해제한다.
	 * 이 시점에는 렌더러와 DataHub가 살아 있으며 GPU 완료를 기다리기 전이다.
	 */
	virtual void OnShutdown() noexcept = 0;
};
