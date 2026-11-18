#pragma once

#include "CursorShape.h"

#include <Windows.h>

struct ImGuiContext;

/**
 * Dear ImGui Context와 Win32 플랫폼 백엔드의 생성과 정리를 맡는다. 메인 스레드에서만 사용한다.
 * 렌더러의 ImGui 모듈은 현재 Context를 사용하므로, 렌더러 Init부터 Finalize까지
 * 이 객체의 Context가 살아 있고 현재 Context로 지정되어 있어야 한다.
 */
class ImGuiSystem
{
	// 생성과 소멸
public:
	ImGuiSystem() = default;
	~ImGuiSystem();

	ImGuiSystem(const ImGuiSystem&) = delete;
	ImGuiSystem& operator=(const ImGuiSystem&) = delete;
	ImGuiSystem(ImGuiSystem&&) = delete;
	ImGuiSystem& operator=(ImGuiSystem&&) = delete;

	/**
	 * Context를 만들어 현재 Context로 두고 window에 Win32 백엔드를 연결한다. 렌더러 Init 전에 호출한다.
	 * 실패하면 만든 것만 정리하고 false를 반환한다. 이미 초기화했으면 false다.
	 */
	bool Initialize(HWND window);

	/** 렌더러 Finalize 뒤, 창 파괴 전에 호출한다. 부분 초기화와 반복 호출에 안전하다. 소멸자도 호출한다. */
	void Shutdown();

	bool IsInitialized() const { return context != nullptr; }

	// 프레임
public:
	/**
	 * 렌더 가능한 프레임마다 앱 갱신 전에 호출한다. IsInitialized일 때만 호출한다.
	 * 호출했으면 같은 프레임에서 반드시 RenderFrame을 호출한다.
	 */
	void BeginFrame();

	/**
	 * 이번 프레임에 ImGui가 원하는 커서 모양을 엔진 타입으로 돌려준다. BeginFrame 다음, RenderFrame 전에 호출한다.
	 * ImGui가 커서 숨김을 요청해도 Arrow를 돌려준다. 숨김은 엔진의 표시 설정으로 다룬다.
	 */
	CursorShape GetRequestedCursorShape() const;

	/** UI 구성을 마친 뒤 렌더러 Sketch 전에 호출한다. 렌더러가 읽을 그리기 목록을 확정한다. */
	void RenderFrame();

	// 입력
public:
	/**
	 * WindowsApplication의 메시지 훅으로 등록하는 함수다. 창 메시지를 현재 Context의 Win32 백엔드에 전달한다.
	 * 커서는 엔진이 관리하므로 백엔드의 처리 결과는 쓰지 않는다.
	 */
	static void HandleWindowMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

	// 렌더러 연결 확인
public:
	/** 렌더러 Init 뒤 호출한다. 렌더러가 이 Context에 ImGui 출력 백엔드를 등록했으면 true다. */
	bool IsRendererBackendReady() const;

private:
	// 이 객체가 만든 Context다. nullptr이면 정리할 것이 없다.
	ImGuiContext* context = nullptr;
	// Win32 백엔드 연결에 성공했을 때만 true다. Context보다 먼저 정리한다.
	bool isPlatformBackendInitialized = false;
};
