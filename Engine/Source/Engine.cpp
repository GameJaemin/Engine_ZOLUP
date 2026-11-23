#include "Engine.h"

#include "Core/FrameTimer.h"
#include "Core/Log.h"
#include "IApplication.h"
#include "Renderer/RenderScene.h"
#include "Renderer/SceneRenderRequest.h"
#include "Renderer/TextureRenderTarget2D.h"
#include "Renderer/ViewportRect.h"

#include "D3D12Renderer.h"
#include "DataHub.h"
#include "DataHub_Interface.h"

#include <cstdint>
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

	// 렌더러가 Init에서 ImGui Context를 확인하므로 GUI 사용 여부를 앱 초기화보다 먼저 읽는다.
	const bool usesGui = application.UsesGui();
	if (Initialize(usesGui))
	{
		if (application.OnInitialize(*this))
		{
			exitCode = RunFrameLoop(application);
		}

		application.OnShutdown();
	}

	if (!Shutdown()) { return 1; }

	return exitCode;
}

void Engine::SetCursorConfined(bool confined)
{
	windowsApplication.SetCursorConfined(confined);
}

void Engine::SetCursorVisible(bool visible)
{
	windowsApplication.SetCursorVisible(visible);
}

bool Engine::Initialize(bool enableGui)
{
	if (!windowsApplication.Initialize()) { return false; }

	int clientWidth = 0;
	int clientHeight = 0;
	if (!windowsApplication.GetClientSize(clientWidth, clientHeight) || clientWidth <= 0 || clientHeight <= 0)
	{
		Log::Write(LogLevel::Error, "Engine", "Invalid client size for rendering");
		return false;
	}

	// 렌더러는 Init 중 현재 ImGui Context에 출력 백엔드를 등록하므로 Context를 먼저 만든다.
	if (enableGui)
	{
		if (!imguiSystem.Initialize(windowsApplication.GetWindowHandle()))
		{
			Log::Write(LogLevel::Error, "Engine", "ImGui initialization failed");
			return false;
		}

		// Context가 생긴 뒤부터 창 메시지를 ImGui에도 전달한다. Shutdown에서 Context보다 먼저 뺀다.
		windowsApplication.SetMessageHook(&ImGuiSystem::HandleWindowMessage);
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

	// 렌더러는 ImGui 모듈 준비에 실패해도 Init 성공을 반환하므로 따로 확인한다.
	// 실패해도 저장한 렌더러는 Shutdown에서 Finalize된다.
	if (enableGui)
	{
		if (!imguiSystem.IsRendererBackendReady())
		{
			Log::Write(LogLevel::Error, "Engine", "Renderer ImGui backend is not ready");
			return false;
		}
	}

	directionalShadowAtlas = Hakko::DataHub::CreateShadowAtlas(L"DirectionalShadow", 2048);
	if (directionalShadowAtlas.size == 0)
	{
		Log::Write(LogLevel::Error, "Engine", "Directional shadow atlas registration failed");
		return false;
	}


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
		// ----------
		// 렌더 가능 여부와 크기 반영
		// ----------
		// 최소화 중이거나 클라이언트 크기가 0이면 이번 프레임은 그리지 않는다.
		int clientWidth = 0;
		int clientHeight = 0;
		bool canRender = false;
		if (!windowsApplication.IsMinimized())
		{
			if (!windowsApplication.GetClientSize(clientWidth, clientHeight))
			{
				Log::Write(LogLevel::Error, "Engine", "Failed to get client size for rendering");
				return 1;
			}

			canRender = clientWidth > 0 && clientHeight > 0;
		}

		if (canRender && (clientWidth != renderWidth || clientHeight != renderHeight))
		{
			renderer->OnResize(clientWidth, clientHeight);
			renderWidth = clientWidth;
			renderHeight = clientHeight;
		}

		// ----------
		// 애플리케이션 갱신
		// ----------
		// GUI 프레임은 렌더 가능한 프레임에서만 연다. 연 프레임은 아래에서 반드시 닫는다.
		const bool runsGuiFrame = canRender && imguiSystem.IsInitialized();
		if (runsGuiFrame)
		{
			imguiSystem.BeginFrame();
		}

		// 그리지 않는 프레임에도 애플리케이션과 시간을 갱신한다.
		application.OnUpdate(windowsApplication.GetInputState(), frameTimer.Tick());

		if (runsGuiFrame)
		{
			application.OnGui();
			// ImGui는 커서를 직접 바꾸지 않으므로 이번 프레임에 원하는 모양을 창에 넘긴다.
			// 이 값은 ImGui::Render 전까지만 유효하므로 RenderFrame보다 먼저 읽는다.
			windowsApplication.SetCursorShape(imguiSystem.GetRequestedCursorShape());
			imguiSystem.RenderFrame();
		}

		if (!canRender) { continue; }

		// ----------
		// 장면 준비와 렌더링
		// ----------
		const ViewportRect renderArea{ 0.0f, 0.0f, static_cast<float>(clientWidth), static_cast<float>(clientHeight) };

		// 요청이 없거나 장면 준비를 건너뛰면 이전 장면이 남지 않도록 초기화한다.
		Hakko::DataHub::ClearOpaqueDrawRequests();
		Hakko::DataHub::ClearTranslucentDrawRequests();
		Hakko::DataHub::ClearScreenDrawRequests();
		Hakko::DataHub::ClearShadowDrawRequests();
		// 그리기 요청을 비운 뒤에도 남는 깊이 출력과 캐스케이드 범위를 초기화한다.
		Hakko::DataHub::RTB_SetShadowCascade shadow{};
		shadow.enable = false;
		Hakko::DataHub::SendRTB(shadow);
		Hakko::DataHub::SetLayerBackground(0);
		Hakko::DataHub::SetLayerCompositeTarget({});
		Hakko::DataHub::SetLayerViewport(Hakko::DataHub::Layer_Viewport{ renderArea.x, renderArea.y, renderArea.width, renderArea.height });

		const std::optional<SceneRenderRequest> request = application.GetRenderRequest();
		if (request.has_value())
		{
			std::uint32_t outputWidth = static_cast<std::uint32_t>(clientWidth);
			std::uint32_t outputHeight = static_cast<std::uint32_t>(clientHeight);
			Hakko::DataHub::ViewTarget output{};

			if (request->target != nullptr)
			{
				const TextureRenderTarget2D& target = *request->target;
				const bool hasValidSize = target.GetWidth() > 0 && target.GetHeight() > 0;
				const bool isRegistered = !target.GetName().empty() && target.FindTextureID() != 0;

				if (!hasValidSize || !isRegistered)
				{
					Log::Write(LogLevel::Error, "Engine", "Invalid scene render target");
					return 1;
				}

				outputWidth = target.GetWidth();
				outputHeight = target.GetHeight();
				output = Hakko::DataHub::ViewTarget{ target.GetName(), outputWidth, outputHeight };
			}

			const float projectionAspectRatio = static_cast<float>(outputWidth) / static_cast<float>(outputHeight);
			RenderScene& scene = request->scene.get();
			const FramePreparationResult result = scene.PrepareFrame(request->camera, renderArea, projectionAspectRatio, directionalShadowAtlas);

			if (result == FramePreparationResult::Failed) { return 1; }

			if (result == FramePreparationResult::Ready)
			{
				Hakko::DataHub::SetLayerCompositeTarget(output);
			}
		}

		// 장면이 없어도 같은 프레임의 자원 요청과 GUI를 처리한다.
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
		directionalShadowAtlas = {};
		shouldFinalizeDataHub = false;
	}
	renderer.reset();

	// 파괴된 Context로 창 메시지가 가지 않도록 Context보다 먼저 훅을 뺀다.
	windowsApplication.SetMessageHook(nullptr);

	// 렌더러 Finalize가 ImGui 텍스처를 정리한 뒤에 Context를 파괴한다.
	imguiSystem.Shutdown();

	return windowsApplication.Shutdown();
}
