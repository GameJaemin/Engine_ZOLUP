#pragma once

#include "Input/InputState.h"

#include <Windows.h>

/** 메인 스레드에서 생성, 초기화, 사용, 소멸하며, 자신이 만든 창과 창 클래스를 소유한다. */
class WindowsApplication
{
	// 생성과 소멸
public:
	WindowsApplication() = default;
	~WindowsApplication();

	WindowsApplication(const WindowsApplication&) = delete;
	WindowsApplication& operator=(const WindowsApplication&) = delete;
	WindowsApplication(WindowsApplication&&) = delete;
	WindowsApplication& operator=(WindowsApplication&&) = delete;

	/** 창 클래스를 등록하고 창을 만든다. 이미 등록했거나 종료를 시작한 객체의 재호출은 실패한다. */
	bool Initialize();

	/**
	 * 창과 등록한 창 클래스를 정리한다. 창을 사용하는 렌더러를 먼저 해제하고 호출한다.
	 * 미초기화 상태와 반복 호출은 안전하다. 실패 시 false를 반환하고 남은 자원은 재호출로 정리할 수 있다.
	 * 호출 후 Initialize로 재사용하지 않는다. 소멸자도 이 함수를 호출한다.
	 */
	bool Shutdown();

private:
	bool Create();

	// 창과 메시지
public:
	/**
	 * 대기 중인 메시지를 처리한다. 프레임당 한 번 호출하며 시작할 때 이전 프레임의 키 이벤트를 비운다.
	 * 종료 요청 시 outExitCode에 종료 코드를 저장하고 false를 반환한다.
	 * WM_CLOSE는 종료만 요청한다. 루프를 마친 호출자가 Shutdown으로 창을 파괴한다.
	 */
	bool ProcessMessages(int& outExitCode);

	/**
	 * 소유한 창 핸들을 빌려주며 창이 없으면 nullptr이다.
	 * 초기화 실패 후 창 정리도 실패하면 핸들을 유지하고 소멸자에서 다시 정리한다.
	 * 창 파괴 또는 이 객체 소멸 후에는 빌린 핸들을 사용하지 않는다.
	 */
	HWND GetWindowHandle() const;

	/** 클라이언트 영역 크기를 조회한다. 실패하면 출력값을 유지한다. */
	bool GetClientSize(int& outWidth, int& outHeight) const;

	/** 창의 최소화 상태를 조회한다. 창이 없으면 false다. */
	bool IsMinimized() const;

private:
	static LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

private:
	HWND window = nullptr;
	HINSTANCE instance = nullptr;

	// 창 생성에 실패해도, 자신이 등록한 창 클래스는 소멸 시 해제한다.
	bool isWindowClassRegistered = false;
	// 닫기 요청 또는 명시적 정리를 시작했으면 창 파괴 시 종료 메시지를 다시 올리지 않는다.
	bool closeRequested = false;

	// 입력
public:
	/**
	 * 창이 수집한 키 상태를 읽기 전용으로 빌려준다. 이 객체가 살아 있는 동안 유효하다.
	 * 상태는 ProcessMessages 호출 중에 갱신된다.
	 */
	const InputState& GetInputState() const;

	/**
	 * 커서 가두기 설정을 저장하고 현재 창 상태에 맞춰 실제 제한을 갱신한다.
	 * 메인 스레드에서 호출한다.
	 * 포커스 상실 또는 최소화 중에는 설정을 유지하고 실제 제한만 해제한다.
	 * 설정이 켜져 있으면 포커스가 있고 최소화되지 않은 창으로 복귀할 때 다시 적용한다.
	 * 실제 적용 또는 해제에 실패해도 설정은 유지한다.
	 */
	void SetCursorConfined(bool confined);

	/**
	 * 커서 표시 설정을 저장하고 현재 커서에 반영한다. 메인 스레드에서 호출한다.
	 * 우리 창의 클라이언트 영역 또는 마우스 캡처 중에만 커서 모양을 변경한다.
	 * 숨김은 포커스가 있고 최소화되지 않은 창에서 적용하며, 그 외에는 기본 화살표를 사용한다.
	 * 포커스 상실 또는 최소화 중에도 저장한 설정은 유지한다.
	 */
	void SetCursorVisible(bool visible);

private:
	/** 생성된 창을 마우스 Raw Input 수신 대상으로 등록한다. */
	bool RegisterRawMouseInput();

	/** 등록을 해제한다. 미등록 상태의 반복 호출은 성공한다. */
	bool UnregisterRawMouseInput();

	/** 저장한 설정과 창의 포커스 및 최소화 상태에 따라 실제 제한을 적용하거나 해제한다. */
	bool UpdateCursorConfinement();

	/** 현재 클라이언트 영역을 화면 좌표로 변환해 제한을 적용한다. */
	bool ApplyCursorConfinement();

	/**
	 * 우리 창의 클라이언트 영역 또는 마우스 캡처 중인 커서에 표시 설정을 반영한다.
	 * forceVisible이 true면 저장된 설정을 바꾸지 않고 표시를 우선한다.
	 */
	void UpdateCursorVisibility(bool forceVisible = false);

private:
	InputState inputState;
	// 등록 성공 후 설정하고, 해제 성공 후 지운다.
	bool rawMouseInputRegistered{false};
	// 호출자가 요청한 설정이며 포커스 상실 또는 최소화 중에도 유지한다.
	bool cursorConfinementRequested = false;
	// 실제 제한 적용에 성공하면 설정하고, 해제에 성공하면 지운다.
	bool cursorConfined = false;
	// 호출자가 요청한 표시 설정이며 포커스 상실 또는 최소화 중에도 유지한다.
	bool cursorVisibleRequested = true;
};

