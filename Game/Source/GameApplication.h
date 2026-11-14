#pragma once

#include "IApplication.h"

#include <memory>
#include <optional>

class World;

class GameApplication final : public IApplication
{
public:
	GameApplication();
	~GameApplication() override;

	bool OnInitialize() override;
	void OnUpdate(const InputState& input, float deltaTime) override;
	std::optional<SceneView> GetSceneView() override;
	void OnShutdown() noexcept override;

private:
	// 애플리케이션 초기화부터 종료 콜백까지 살아 있다.
	std::unique_ptr<World> gameWorld;
};
