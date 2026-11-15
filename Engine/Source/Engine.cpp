#include "Engine.h"

#include "Core/FrameTimer.h"
#include "Core/Log.h"
#include "IApplication.h"
#include "Renderer/RenderScene.h"

#include "D3D12Renderer.h"
#include "DataHub.h"
#include "DataHub_Interface.h"

#include <memory>
#include <optional>
#include <utility>

Engine::Engine() = default;

Engine::~Engine()
{
	Shutdown();
}

int Engine::Run(IApplication& application)
{
	if (hasRun) { return 1; }
	hasRun = true;

	int exitCode = 1;
	if (Initialize())
	{
		if (application.OnInitialize())
		{
			exitCode = RunFrameLoop(application);
		}

		application.OnShutdown();
	}

	if (!Shutdown()) { return 1; }

	return exitCode;
}

bool Engine::Initialize()
{
	if (!windowsApplication.Initialize()) { return false; }

	int clientWidth = 0;
	int clientHeight = 0;
	if (!windowsApplication.GetClientSize(clientWidth, clientHeight) || clientWidth <= 0 || clientHeight <= 0)
	{
		Log::Write(LogLevel::Error, "Engine", "Invalid client size for rendering");
		return false;
	}

	std::unique_ptr<Hakko::D3D12Renderer> newRenderer = std::make_unique<Hakko::D3D12Renderer>(Hakko::RenderLimits{});
	// 렌더러 초기화에서 예약 리소스와 그리기 설정을 읽으므로 DataHub를 먼저 준비한다.
	shouldFinalizeDataHub = true;
	if (!Hakko::DataHub::Init())
	{
		Log::Write(LogLevel::Error, "Engine", "DataHub initialization failed");
		return false;
	}

	if (!newRenderer->Init(windowsApplication.GetWindowHandle(), clientWidth, clientHeight))
	{
		Log::Write(LogLevel::Error, "Engine", "D3D12 renderer initialization failed");
		// Init에 실패한 렌더러는 GPU 펜스 상태를 보장할 수 없어 Finalize하지 않는다.
		return false;
	}

	renderer = std::move(newRenderer);
	renderWidth = clientWidth;
	renderHeight = clientHeight;

	Log::Write(LogLevel::Info, "Engine", "D3D12 renderer initialized");

	Hakko::DataHub::SetLayerBackground(RESERVED_TEX_NONE_DIFFUSE_ID);
	Hakko::DataHub::SetLayerBackgroundColor(0.3f, 0.05f, 0.1f, 1.0f);

	return true;
}

int Engine::RunFrameLoop(IApplication& application)
{
	Log::Write(LogLevel::Info, "Engine", "Run loop started");

	// 초기화와 World 구성 시간을 첫 측정에서 빼려고 루프 직전에 만든다.
	FrameTimer frameTimer;
	int exitCode = 0;

	while (windowsApplication.ProcessMessages(exitCode))
	{
		application.OnUpdate(windowsApplication.GetInputState(), frameTimer.Tick());

		// 최소화 중에도 애플리케이션과 시간을 갱신한다.
		if (windowsApplication.IsMinimized()) { continue; }

		int clientWidth = 0;
		int clientHeight = 0;
		if (!windowsApplication.GetClientSize(clientWidth, clientHeight))
		{
			Log::Write(LogLevel::Error, "Engine", "Failed to get client size for rendering");
			return 1;
		}

		if (clientWidth <= 0 || clientHeight <= 0) { continue; }

		if (clientWidth != renderWidth || clientHeight != renderHeight)
		{
			renderer->OnResize(clientWidth, clientHeight);
			renderWidth = clientWidth;
			renderHeight = clientHeight;
		}

		const std::optional<SceneView> sceneView = application.GetSceneView();
		if (!sceneView.has_value()) { continue; }

		RenderScene& scene = sceneView->scene.get();
		const float aspectRatio = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
		const FramePreparationResult preparationResult = scene.PrepareFrame(sceneView->camera, aspectRatio);
		if (preparationResult == FramePreparationResult::SkipFrame) { continue; }
		if (preparationResult == FramePreparationResult::Failed) { return 1; }

		renderer->Sketch();
		renderer->Render();
	}

	return exitCode;
}

bool Engine::Shutdown()
{
	if (renderer)
	{
		renderer->Finalize();
	}
	if (shouldFinalizeDataHub)
	{
		Hakko::DataHub::Finalize();
		shouldFinalizeDataHub = false;
	}
	renderer.reset();

	return windowsApplication.Shutdown();
}
