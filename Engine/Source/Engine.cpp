#include "Engine.h"

#include "Core/FrameTimer.h"
#include "Core/Log.h"
#include "Component/EngineComponentTypes.h"
#include "IApplication.h"
#include "Renderer/PostProcessPasses.h"
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
#include <vector>

namespace
{
	// 후처리 슬롯 0에 ACES 톤매핑을 설정한다. saturation은 0이면 흑백, 1이면 기본 채도다.
	void ConfigureTonemap(std::uint8_t layer, float saturation)
	{
		Hakko::DataHub::RTB_SetTonemap tonemap{};
		tonemap.ppSlot = 0;
		tonemap.mode = Hakko::DataHub::Tonemap_Mode_ACES;
		tonemap.saturation = saturation;
		Hakko::DataHub::SendRTB(tonemap, layer);
	}

	// 후처리 설정은 레이어마다 따로 저장되므로 새로 만든 레이어에도 같은 설정을 보낸다.
	void ConfigureDefaultRenderPasses(std::uint8_t layer)
	{
		// 포워드 SSAO가 읽을 불투명 깊이를 먼저 만든다.
		Hakko::DataHub::SetLayerEarlyZ(true, layer);

		// 반경 25 cm, 깊이 바이어스 1 cm를 초깃값으로 사용한다.
		Hakko::DataHub::RTB_SetSSAO ambientOcclusion{};
		ambientOcclusion.radiusScale = 50.0f;
		ambientOcclusion.intensity = 0.7f;
		ambientOcclusion.biasScale = 40.0f;
		Hakko::DataHub::SendRTB(ambientOcclusion, layer);

		// 기준 픽셀과 카메라 공간 깊이 차이가 4 cm 미만인 샘플을 평균에 포함한다.
		Hakko::DataHub::RTB_SetAOBlur ambientOcclusionBlur{};
		ambientOcclusionBlur.depthThresholdScale = 16.0f;
		Hakko::DataHub::SendRTB(ambientOcclusionBlur, layer);

		// 슬롯 순서에 따라 톤매핑 결과에 FXAA를 적용한다.
		ConfigureTonemap(layer, 1.0f);

		Hakko::DataHub::RTB_SetFXAA antialiasing{};
		antialiasing.ppSlot = 1;
		Hakko::DataHub::SendRTB(antialiasing, layer);
	}

	// 레이어 0은 메인 화면이라 항상 유지한다. 새로 생긴 레이어에는 0번과 같은 기본 패스를 설정한다.
	void ResizeRenderLayers(std::size_t layerCount)
	{
		const std::size_t oldCount = Hakko::DataHub::LayerList.size();
		Hakko::DataHub::LayerList.resize(layerCount);
		for (std::size_t layer = oldCount; layer < layerCount; ++layer)
		{
			ConfigureDefaultRenderPasses(static_cast<std::uint8_t>(layer));
		}
	}

	FramePreparationResult PrepareRenderRequest(const SceneRenderRequest& request, const ViewportRect& renderArea, const Hakko::DataHub::ShadowAtlas& shadowAtlas, std::uint8_t layer, float elapsedSeconds)
	{
		float projectionAspectRatio = renderArea.width / renderArea.height;
		Hakko::DataHub::ViewTarget output{};

		if (request.target != nullptr)
		{
			const TextureRenderTarget2D& target = *request.target;
			const std::uint32_t outputWidth = target.GetWidth();
			const std::uint32_t outputHeight = target.GetHeight();
			const bool hasValidSize = outputWidth > 0 && outputHeight > 0;
			const bool isRegistered = !target.GetName().empty() && target.FindTextureID() != 0;

			if (!hasValidSize || !isRegistered)
			{
				Log::Write(LogLevel::Error, "Engine", "Invalid scene render target");
				return FramePreparationResult::Failed;
			}

			projectionAspectRatio = static_cast<float>(outputWidth) / static_cast<float>(outputHeight);
			output = Hakko::DataHub::ViewTarget{ target.GetName(), outputWidth, outputHeight };
		}

		RenderScene& scene = request.scene.get();
		const FramePreparationResult result = scene.PrepareFrame(request.camera, renderArea, projectionAspectRatio, shadowAtlas, layer);
		if (result == FramePreparationResult::Ready)
		{
			Hakko::DataHub::SetLayerCompositeTarget(output, layer);

			// 셰이더의 g_Time은 레이어마다 따로 있어 준비한 레이어마다 보낸다.
			Hakko::DataHub::RTB_SetTime time{};
			time.totalTime = elapsedSeconds;
			Hakko::DataHub::SendRTB(time, layer);

			// 요청마다 채도와 후처리 목록이 다를 수 있어 매 프레임 그 요청이 쓰는 레이어에 보낸다.
			ConfigureTonemap(layer, request.saturation);
			PostProcessPasses::Configure(layer, request.postProcessPasses);
		}

		return result;
	}
}

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
	if (!RegisterEngineComponentTypes(componentTypeRegistry))
	{
		Log::Write(LogLevel::Error, "Engine", "Component type registration failed");
		return false;
	}

	if (!joltRuntime.Initialize()) { return false; }

	// 출력 장치가 없어도 화면과 입력은 동작해야 하므로 경고만 남기고 계속한다.
	if (!audioSystem.Initialize())
	{
		Log::Write(LogLevel::Warning, "Engine", "Audio is disabled");
	}

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

	ConfigureDefaultRenderPasses(0U);

	return true;
}

int Engine::RunFrameLoop(IApplication& application)
{
	Log::Write(LogLevel::Info, "Engine", "Run loop started");

	// 초기화와 World 구성 시간을 첫 측정에서 빼려고 루프 직전에 만든다.
	FrameTimer frameTimer;
	// 프레임 루프 시작부터 누적한 실제 경과 초다. 셰이더의 g_Time으로 보낸다.
	// float에 작은 델타를 오래 더하면 오차가 쌓이므로 double로 누적하고 보낼 때만 float로 바꾼다.
	double elapsedSeconds = 0.0;
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
		const float deltaSeconds = frameTimer.Tick();
		elapsedSeconds += deltaSeconds;
		application.OnUpdate(windowsApplication.GetInputState(), deltaSeconds);
		audioSystem.Update();

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
			const FramePreparationResult result = PrepareRenderRequest(*request, renderArea, directionalShadowAtlas, 0U, static_cast<float>(elapsedSeconds));
			if (result == FramePreparationResult::Failed) { return 1; }
		}

		// 메인 요청이 그리는 World의 캡처를 함께 그린다. 게임과 에디터 Play가 같은 경로를 쓴다.
		std::vector<SceneRenderRequest> captureRequests;
		if (request.has_value())
		{
			request->scene.get().CollectCaptureRequests(captureRequests);
		}

		// 준비에 성공한 캡처만 1번부터 빈칸 없이 채운다. 건너뛴 요청이 쓰던 칸은 다음 요청이 덮거나 아래 축소로 사라진다.
		std::uint8_t nextLayer = 1U;
		for (const SceneRenderRequest& capture : captureRequests)
		{
			if (nextLayer >= RENDER_LAYER_MAX)
			{
				Log::Write(LogLevel::Warning, "Engine", "Too many scene capture requests");
				break;
			}
			if (capture.target == nullptr)
			{
				Log::Write(LogLevel::Error, "Engine", "Scene capture request has no render target");
				continue;
			}

			ResizeRenderLayers(nextLayer + 1U);
			const FramePreparationResult result = PrepareRenderRequest(capture, renderArea, directionalShadowAtlas, nextLayer, static_cast<float>(elapsedSeconds));
			if (result == FramePreparationResult::Failed) { return 1; }
			if (result == FramePreparationResult::Ready) { ++nextLayer; }
		}

		// 이번 프레임에 쓰지 않은 캡처 레이어를 없애 이전 영상이 백버퍼에 합성되지 않게 한다.
		ResizeRenderLayers(nextLayer);

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

	// Run에서 애플리케이션 정리가 끝난 뒤 장치를 닫는다.
	audioSystem.Shutdown();

	// Run에서 애플리케이션 정리가 끝난 뒤 Jolt의 전역 등록을 해제한다.
	joltRuntime.Shutdown();

	return windowsApplication.Shutdown();
}
