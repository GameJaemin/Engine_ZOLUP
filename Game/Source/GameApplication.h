#pragma once

#include "IApplication.h"
#include "Renderer/SceneRenderRequest.h"

#include <memory>
#include <optional>

class Engine;
class ThirdPersonCharacter;
class World;

class GameApplication final : public IApplication
{
public:
	GameApplication();
	~GameApplication() override;

	bool OnInitialize(Engine& inEngine) override;
	void OnUpdate(const InputState& input, float deltaTime) override;
	std::optional<SceneRenderRequest> GetRenderRequest() override;
	void OnShutdown() noexcept override;

private:
	// OnInitialize에서 빌리고 OnShutdown에서 비운다. 소유하지 않는다.
	Engine* engine = nullptr;

	// 애플리케이션 초기화부터 종료 콜백까지 살아 있다.
	std::unique_ptr<World> gameWorld;
	// World보다 먼저 해제해 생성 객체와 카메라 연결을 정리한다.
	std::unique_ptr<ThirdPersonCharacter> thirdPerson;
	bool cursorCaptured = false;
};
