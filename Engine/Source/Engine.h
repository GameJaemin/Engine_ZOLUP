#pragma once

#include "AssetManager.h"
#include "Audio/AudioSystem.h"
#include "Component/ComponentTypeRegistry.h"
#include "ImGuiSystem.h"
#include "Physics/JoltRuntime.h"
#include "WindowsApplication.h"

#include "DataHub_Interface.h"

#include <memory>

namespace Hakko
{
	class D3D12Renderer;
}

class IApplication;

/**
 * 창, 렌더러, AssetManager, ComponentTypeRegistry, JoltRuntime 및 AudioSystem을 소유하고 메인 스레드에서 공통 프레임 순서를 실행한다.
 */
class Engine
{
	// 생성과 소멸
public:
	Engine();
	~Engine();

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	Engine(Engine&&) = delete;
	Engine& operator=(Engine&&) = delete;

	/**
	 * application을 호출 중에만 빌려 서비스 초기화, 프레임 실행 및 정리를 수행한다.
	 * Engine 인스턴스당 한 번 호출한다. 실패 반환은 정리 후 1로 처리한다.
	 * application이 GUI를 요구했는데 GUI 준비에 실패한 경우도 실패로 처리한다.
	 * 정상 종료에서는 창 메시지의 종료 코드를 반환한다. C++ 예외는 호출자에게 전파된다.
	 */
	int Run(IApplication& application);

	/**
	 * 커서 가두기 설정을 지정한다. 메인 스레드에서 호출한다.
	 * 애플리케이션의 OnInitialize부터 OnShutdown까지 사용할 수 있다.
	 * 포커스 상실 또는 최소화 중에는 설정을 유지하고 실제 제한만 해제한다.
	 * 설정이 켜져 있으면 창으로 복귀해 조작 가능한 상태가 될 때 다시 적용한다.
	 */
	void SetCursorConfined(bool confined);

	/**
	 * 커서 표시 설정을 지정한다. 메인 스레드에서 호출한다.
	 * 애플리케이션의 OnInitialize부터 OnShutdown까지 사용할 수 있다.
	 * 숨김 설정은 포커스 상실 또는 최소화 중에도 유지하며 복귀 시 다시 반영한다.
	 */
	void SetCursorVisible(bool visible);

	/** 엔진이 소유한 오디오 시스템을 빌려준다. OnInitialize부터 OnShutdown까지 메인 스레드에서 사용한다. */
	AudioSystem& GetAudioSystem() { return audioSystem; }

private:
	/**
	 * 엔진 컴포넌트 타입, Jolt 및 오디오를 초기화한 뒤 창, (enableGui면 ImGui), DataHub와 렌더러를 준비하고 방향광 섀도 맵을 등록한다.
	 * 오디오 초기화에 실패하면 경고를 남기고 소리 없이 계속한다.
	 * enableGui면 렌더러가 ImGui 출력 백엔드를 등록했는지까지 확인한다.
	 */
	bool Initialize(bool enableGui);
	int RunFrameLoop(IApplication& application);
	bool Shutdown();

private:
	// Shutdown에서 렌더러를 먼저 해제한다. 멤버 자동 소멸에서도 창 객체가 가장 오래 살아 있도록 둔다.
	WindowsApplication windowsApplication;

	// 렌더러가 Init부터 Finalize까지 ImGui Context를 사용하므로 렌더러보다 먼저 선언해 나중에 소멸한다.
	ImGuiSystem imguiSystem;

	AssetManager assetManager;

	// 창, ImGui와 렌더러에 의존하지 않으므로 기존 소멸 순서 제약과 무관하다.
	ComponentTypeRegistry componentTypeRegistry;

	JoltRuntime joltRuntime;

	// 장치를 열지 못해도 엔진은 계속 실행하므로 초기화 상태와 무관하게 Shutdown을 호출할 수 있다.
	AudioSystem audioSystem;

	// 초기화에 성공한 렌더러만 소유한다. nullptr이면 Finalize할 렌더러가 없다.
	std::unique_ptr<Hakko::D3D12Renderer> renderer;

	// DataHub가 소유하는 방향광 깊이 텍스처다. 창 크기와 무관하게 재사용한다.
	Hakko::DataHub::ShadowAtlas directionalShadowAtlas;

	// 초기화 또는 OnResize에 마지막으로 전달한 클라이언트 영역의 픽셀 크기다.
	int renderWidth = 0;
	int renderHeight = 0;

	// Init이 실패하거나 예외를 던져도 부분적으로 채워진 DataHub를 정리한다.
	bool shouldFinalizeDataHub = false;
	bool hasRun = false;
};
