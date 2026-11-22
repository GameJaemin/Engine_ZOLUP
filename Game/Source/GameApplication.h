#pragma once

#include "IApplication.h"
#include "Renderer/SceneRenderRequest.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

class Engine;
class World;

class GameApplication final : public IApplication
{
public:
	explicit GameApplication(std::string_view initialMapCameraName = {});
	~GameApplication() override;

	bool OnInitialize(Engine& inEngine) override;
	void OnUpdate(const InputState& input, float deltaTime) override;
	std::optional<SceneRenderRequest> GetRenderRequest() override;
	void OnShutdown() noexcept override;

private:
	// 비어 있으면 플레이어 카메라로 시작한다.
	std::string mapCameraName;

	// OnInitialize에서 빌리고 OnShutdown에서 비운다. 소유하지 않는다.
	Engine* engine = nullptr;

	// 애플리케이션 초기화부터 종료 콜백까지 살아 있다.
	std::unique_ptr<World> gameWorld;
};
