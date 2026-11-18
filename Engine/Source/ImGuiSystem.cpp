#include "ImGuiSystem.h"

#include "Core/Log.h"

#include <imgui.h>
#include <imgui_impl_win32.h>

// imgui_impl_win32.h는 Windows.h에 의존하지 않으려고 이 선언을 막아 두므로, 사용하는 쪽에서 직접 선언한다.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	/** ImGui 커서 종류를 엔진 커서 모양으로 바꾼다. 숨김과 알 수 없는 값은 Arrow다. */
	CursorShape ToCursorShape(ImGuiMouseCursor cursor)
	{
		switch (cursor)
		{
		case ImGuiMouseCursor_TextInput:  return CursorShape::TextInput;
		case ImGuiMouseCursor_ResizeAll:  return CursorShape::ResizeAll;
		case ImGuiMouseCursor_ResizeNS:   return CursorShape::ResizeNS;
		case ImGuiMouseCursor_ResizeEW:   return CursorShape::ResizeEW;
		case ImGuiMouseCursor_ResizeNESW: return CursorShape::ResizeNESW;
		case ImGuiMouseCursor_ResizeNWSE: return CursorShape::ResizeNWSE;
		case ImGuiMouseCursor_Hand:       return CursorShape::Hand;
		case ImGuiMouseCursor_Wait:       return CursorShape::Wait;
		case ImGuiMouseCursor_Progress:   return CursorShape::Progress;
		case ImGuiMouseCursor_NotAllowed: return CursorShape::NotAllowed;
		case ImGuiMouseCursor_Arrow:
		default:                          return CursorShape::Arrow;
		}
	}
}

ImGuiSystem::~ImGuiSystem()
{
	Shutdown();
}

bool ImGuiSystem::Initialize(HWND window)
{
	if (context != nullptr) { return false; }

	// 헤더와 링크한 라이브러리의 버전 및 구조체 크기가 다르면 여기서 멈춘다.
	IMGUI_CHECKVERSION();
	context = ImGui::CreateContext();
	// 다른 Context가 이미 현재로 지정되어 있으면 CreateContext가 그쪽으로 되돌리므로 직접 지정한다.
	ImGui::SetCurrentContext(context);

	// 실행 위치에 imgui.ini를 만들지 않는다. 레이아웃 저장 위치는 도킹 단계에서 정한다.
	ImGui::GetIO().IniFilename = nullptr;

	// 커서 모양과 표시는 엔진의 WindowsApplication이 정한다. ImGui가 원하는 모양은
	// GetRequestedCursorShape로 읽어 엔진에 넘기므로 백엔드가 커서를 직접 바꾸지 않게 한다.
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	if (!ImGui_ImplWin32_Init(window))
	{
		Log::Write(LogLevel::Error, "ImGui", "Win32 platform backend initialization failed");
		Shutdown();
		return false;
	}
	isPlatformBackendInitialized = true;

	return true;
}

void ImGuiSystem::Shutdown()
{
	if (context == nullptr) { return; }

	// 백엔드 정리와 파괴는 현재 Context를 대상으로 하므로 소유한 Context를 지정한다.
	ImGui::SetCurrentContext(context);

	if (isPlatformBackendInitialized)
	{
		ImGui_ImplWin32_Shutdown();
		isPlatformBackendInitialized = false;
	}

	ImGui::DestroyContext(context);
	context = nullptr;
}

void ImGuiSystem::BeginFrame()
{
	// Win32 백엔드가 창 크기, 경과 시간과 마우스 상태를 먼저 채워야 NewFrame이 그 값을 사용한다.
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

CursorShape ImGuiSystem::GetRequestedCursorShape() const
{
	if (context == nullptr || ImGui::GetCurrentContext() != context) { return CursorShape::Arrow; }

	return ToCursorShape(ImGui::GetMouseCursor());
}

void ImGuiSystem::RenderFrame()
{
	// 렌더러가 Sketch에서 ImGui::GetDrawData로 이 결과를 읽는다.
	ImGui::Render();
}

void ImGuiSystem::HandleWindowMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
	// 백엔드가 커서를 바꾸지 않도록 NoMouseCursorChange를 켜 두었으므로 반환값은 필요 없다.
	ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam);
}

bool ImGuiSystem::IsRendererBackendReady() const
{
	if (context == nullptr || ImGui::GetCurrentContext() != context) { return false; }

	// TODO: 렌더러가 ImGui 준비 결과를 공개하면 그 결과로 바꾼다.
	// 지금은 렌더러의 ImGuiModule::Init이 모두 성공했을 때만 설정하는 값을 확인한다.
	const ImGuiIO& io = ImGui::GetIO();
	return io.BackendRendererName != nullptr && (io.BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0;
}
