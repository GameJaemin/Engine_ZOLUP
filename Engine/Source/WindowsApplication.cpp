#include "WindowsApplication.h"

#include "Core/Log.h"
#include "Input/Key.h"

#include <Windowsx.h>

// Windowsx.h의 매크로가 WindowsApplication::IsMinimized 정의를 바꾸지 않도록 한다.
#undef IsMinimized

#include <cstdio>

namespace
{
	constexpr wchar_t WindowClassName[] = L"SeonEngineWindow";

	/** 다른 Windows 호출이 오류 번호를 바꾸기 전에 받은 값으로 출력한다. */
	void LogWindowsError(const char* operation, DWORD errorCode)
	{
		char message[160] = {};
		std::snprintf(message, sizeof(message), "%s failed. Windows error: %lu", operation, static_cast<unsigned long>(errorCode));
		Log::Write(LogLevel::Error, "WindowsApplication", message);
	}

	static_assert(static_cast<int>(Key::Z) - static_cast<int>(Key::A) == 'Z' - 'A', "A부터 Z까지 Key 값이 연속해야 한다.");
	static_assert(static_cast<int>(Key::Number9) - static_cast<int>(Key::Number0) == '9' - '0', "Number0부터 Number9까지 Key 값이 연속해야 한다.");

	/** 지원하지 않는 가상 키는 Key::Unknown으로 바꾼다. 왼쪽과 오른쪽 수정 키는 구분하지 않는다. */
	Key TranslateKey(WPARAM virtualKey)
	{
		// Windows의 문자와 숫자 가상 키 코드는 ASCII 대문자와 숫자 코드와 같다.
		if (virtualKey >= 'A' && virtualKey <= 'Z')
		{
			return static_cast<Key>(static_cast<int>(Key::A) + static_cast<int>(virtualKey - 'A'));
		}

		if (virtualKey >= '0' && virtualKey <= '9')
		{
			return static_cast<Key>(static_cast<int>(Key::Number0) + static_cast<int>(virtualKey - '0'));
		}

		switch (virtualKey)
		{
		case VK_SPACE:
			return Key::Space;
		case VK_RETURN:
			return Key::Enter;
		case VK_ESCAPE:
			return Key::Escape;
		case VK_TAB:
			return Key::Tab;
		case VK_SHIFT:
			return Key::Shift;
		case VK_CONTROL:
			return Key::Control;
		case VK_MENU:
			return Key::Alt;
		case VK_UP:
			return Key::Up;
		case VK_DOWN:
			return Key::Down;
		case VK_LEFT:
			return Key::Left;
		case VK_RIGHT:
			return Key::Right;
		default:
			return Key::Unknown;
		}
	}

	void RecordMouseButton(HWND windowHandle, InputState& inputState, Key button, bool isPressed)
	{
		if (isPressed)
		{
			inputState.PressKey(button);
			// 커서가 창 밖에서 버튼을 떼어도 이 창이 메시지를 받도록 한다.
			if (GetCapture() != windowHandle) { SetCapture(windowHandle); }
			return;
		}

		inputState.ReleaseKey(button);
		if (inputState.IsKeyDown(Key::MouseLeft) || inputState.IsKeyDown(Key::MouseRight) || inputState.IsKeyDown(Key::MouseMiddle)) { return; }
		if (GetCapture() == windowHandle) { ReleaseCapture(); }
	}

	void RecordMouseMove(HWND windowHandle, InputState& inputState, LPARAM lParam)
	{
		if (GetFocus() != windowHandle) { return; }

		const int x = GET_X_LPARAM(lParam);
		const int y = GET_Y_LPARAM(lParam);
		RECT clientRect = {};
		// 마우스 캡처 중에는 클라이언트 영역 밖 좌표도 받을 수 있다.
		if (!GetClientRect(windowHandle, &clientRect) || !PtInRect(&clientRect, POINT{ x, y }))
		{
			inputState.InvalidateMousePosition();
			return;
		}

		if (!inputState.GetMousePosition().has_value())
		{
			// 창을 처음 추적하거나 포커스 복귀 또는 창 재진입 후 다시 추적한다.
			TRACKMOUSEEVENT tracking = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, windowHandle, 0 };
			if (!TrackMouseEvent(&tracking)) { return; }
		}

		inputState.RecordMousePosition(x, y);
	}

	/** 포커스가 있는 창의 마우스 상대 이동량만 기록한다. */
	void RecordRawMouseInput(HWND windowHandle, InputState& inputState, LPARAM lParam)
	{
		if (GetFocus() != windowHandle) { return; }

		RAWINPUT rawInput{};
		UINT dataSize = static_cast<UINT>(sizeof(rawInput));

		const UINT bytesRead = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &rawInput, &dataSize, static_cast<UINT>(sizeof(RAWINPUTHEADER)));
		if (bytesRead == static_cast<UINT>(-1))
		{
			LogWindowsError("GetRawInputData", GetLastError());
			return;
		}

		if (rawInput.header.dwType != RIM_TYPEMOUSE) { return; }

		const RAWMOUSE& mouse = rawInput.data.mouse;

		// 절대 좌표를 상대 이동량으로 해석하지 않는다.
		if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) { return; }

		inputState.AddRawMouseDelta(static_cast<int>(mouse.lLastX), static_cast<int>(mouse.lLastY));
	}

	/**
	 * 키, 마우스 버튼, 이동, 휠 및 입력 해제 메시지를 입력 상태에 기록한다.
	 * Alt와 함께 누른 키는 WM_SYSKEYDOWN으로 들어오므로 일반 키 메시지와 같이 기록한다.
	 * 창 프로시저는 Windows가 호출하므로 예외가 그 밖으로 나가지 않게 noexcept로 둔다.
	 * 이벤트 목록 할당에 실패하면 std::terminate로 프로그램을 종료한다.
	 */
	void RecordInputMessage(HWND windowHandle, InputState& inputState, UINT message, WPARAM wParam, LPARAM lParam) noexcept
	{
		switch (message)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			inputState.PressKey(TranslateKey(wParam));
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			inputState.ReleaseKey(TranslateKey(wParam));
			break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			RecordMouseButton(windowHandle, inputState, Key::MouseLeft, message == WM_LBUTTONDOWN);
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			RecordMouseButton(windowHandle, inputState, Key::MouseRight, message == WM_RBUTTONDOWN);
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			RecordMouseButton(windowHandle, inputState, Key::MouseMiddle, message == WM_MBUTTONDOWN);
			break;
		case WM_MOUSEMOVE:
			RecordMouseMove(windowHandle, inputState, lParam);
			break;
		case WM_INPUT:
			RecordRawMouseInput(windowHandle, inputState, lParam);
			break;
		case WM_MOUSELEAVE:
			inputState.InvalidateMousePosition();
			break;
		case WM_MOUSEWHEEL:
			if (GetFocus() == windowHandle)
			{
				const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
				inputState.AddMouseWheelDelta(delta);
			}
			break;
		case WM_KILLFOCUS:
			inputState.ReleaseAllKeys();
			inputState.ResetMouseInput();
			if (GetCapture() == windowHandle) { ReleaseCapture(); }
			break;
		case WM_CAPTURECHANGED:
			// 먼저 해제한 버튼은 ReleaseKey에서 무시하므로 뗌 이벤트가 중복되지 않는다.
			inputState.ReleaseKey(Key::MouseLeft);
			inputState.ReleaseKey(Key::MouseRight);
			inputState.ReleaseKey(Key::MouseMiddle);
			break;
		default:
			break;
		}
	}

	/** 엔진 커서 모양을 Windows 시스템 커서 이름으로 바꾼다. */
	LPCWSTR ToSystemCursorName(CursorShape shape)
	{
		switch (shape)
		{
		case CursorShape::TextInput:  return IDC_IBEAM;
		case CursorShape::ResizeAll:  return IDC_SIZEALL;
		case CursorShape::ResizeNS:   return IDC_SIZENS;
		case CursorShape::ResizeEW:   return IDC_SIZEWE;
		case CursorShape::ResizeNESW: return IDC_SIZENESW;
		case CursorShape::ResizeNWSE: return IDC_SIZENWSE;
		case CursorShape::Hand:       return IDC_HAND;
		case CursorShape::Wait:       return IDC_WAIT;
		case CursorShape::Progress:   return IDC_APPSTARTING;
		case CursorShape::NotAllowed: return IDC_NO;
		case CursorShape::Arrow:
		default:                      return IDC_ARROW;
		}
	}
}

WindowsApplication::~WindowsApplication()
{
	Shutdown();
}

bool WindowsApplication::Initialize()
{
	if (closeRequested) { return false; }
	if (!Create()) { return false; }

	return RegisterRawMouseInput();
}

bool WindowsApplication::Shutdown()
{
	closeRequested = true;

	SetCursorVisible(true);

	cursorConfinementRequested = false;
	const bool cursorReleased = UpdateCursorConfinement();
	const bool rawInputUnregistered = UnregisterRawMouseInput();

	if (window != nullptr)
	{
		if (!DestroyWindow(window))
		{
			LogWindowsError("DestroyWindow", GetLastError());
			return false;
		}
		// 초기화 중 객체 연결에 실패한 창도 파괴 성공 후 핸들을 비운다.
		window = nullptr;
	}

	if (isWindowClassRegistered)
	{
		if (!UnregisterClassW(WindowClassName, instance))
		{
			LogWindowsError("UnregisterClassW", GetLastError());
			return false;
		}
		isWindowClassRegistered = false;
	}

	return cursorReleased && rawInputUnregistered;
}

bool WindowsApplication::Create()
{
	// ----------
	// 창 클래스 등록
	// ----------
	if (isWindowClassRegistered)
	{
		Log::Write(LogLevel::Error, "WindowsApplication", "Initialize failed: window class is already registered.");
		return false;
	}

	instance = GetModuleHandleW(nullptr);
	if (instance == nullptr)
	{
		LogWindowsError("GetModuleHandleW", GetLastError());
		return false;
	}

	WNDCLASSEXW windowClass = {};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = WindowClassName;
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);

	if (RegisterClassExW(&windowClass) == 0)
	{
		LogWindowsError("RegisterClassExW", GetLastError());
		return false;
	}

	isWindowClassRegistered = true;

	// ----------
	// 창 생성
	// ----------
	const DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	const DWORD windowExStyle = 0;

	const int windowWidth = 1280;
	const int windowHeight = 720;
	window = CreateWindowExW(windowExStyle, WindowClassName, L"SeonEngine", windowStyle, CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight, nullptr, nullptr, instance, nullptr);
	if (window == nullptr)
	{
		LogWindowsError("CreateWindowExW", GetLastError());
		return false;
	}

	// ----------
	// 창과 소유 객체 연결 및 표시
	// ----------
	// 창 생성이 끝난 뒤, 이후 메시지에서 소유 객체를 찾을 수 있도록 연결한다.
	SetLastError(ERROR_SUCCESS);
	const LONG_PTR previousData = SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	const DWORD connectionError = GetLastError();
	if (previousData == 0 && connectionError != ERROR_SUCCESS)
	{
		LogWindowsError("SetWindowLongPtrW", connectionError);
		// 연결되지 않은 창이므로 파괴 성공 시 여기서 직접 핸들을 비운다.
		if (DestroyWindow(window))
		{
			window = nullptr;
		}
		else
		{
			// 핸들을 유지해 소멸자에서 다시 정리한다.
			LogWindowsError("DestroyWindow", GetLastError());
		}
		return false;
	}

	ShowWindow(window, SW_SHOW);

	return true;
}

bool WindowsApplication::ProcessMessages(int& outExitCode)
{
	inputState.ClearFrameEvents();

	MSG message = {};

	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
	{
		if (message.message == WM_QUIT)
		{
			outExitCode = static_cast<int>(message.wParam);
			return false;
		}

		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	return true;
}

HWND WindowsApplication::GetWindowHandle() const
{
	return window;
}

bool WindowsApplication::GetClientSize(int& outWidth, int& outHeight) const
{
	if (window == nullptr) { return false; }

	RECT clientRect = {};
	if (!GetClientRect(window, &clientRect)) { return false; }

	outWidth = clientRect.right - clientRect.left;
	outHeight = clientRect.bottom - clientRect.top;
	return true;
}

bool WindowsApplication::IsMinimized() const
{
	return window != nullptr && IsIconic(window) != FALSE;
}

void WindowsApplication::SetMessageHook(WindowMessageHook hook)
{
	messageHook = hook;
}

LRESULT WindowsApplication::WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
	// 창 생성 중 객체 주소를 연결하기 전과 연결을 해제한 뒤의 메시지는 기록할 대상이 없다.
	WindowsApplication* windowsApplication = reinterpret_cast<WindowsApplication*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));

	if (windowsApplication != nullptr && windowsApplication->messageHook != nullptr)
	{
		windowsApplication->messageHook(windowHandle, message, wParam, lParam);
	}

	if (windowsApplication != nullptr)
	{
		switch (message)
		{
		case WM_CLOSE:
		case WM_DESTROY:
			windowsApplication->SetCursorVisible(true);
			windowsApplication->SetCursorConfined(false);
			break;

		case WM_KILLFOCUS:
			windowsApplication->UpdateCursorVisibility(true);
			// 설정은 유지하고 실제 제한만 해제한다.
			if (windowsApplication->cursorConfined)
			{
				if (ClipCursor(nullptr))
				{
					windowsApplication->cursorConfined = false;
				}
				else
				{
					LogWindowsError("ClipCursor", GetLastError());
				}
			}
			break;

		case WM_SETFOCUS:
		case WM_MOVE:
		case WM_SIZE:
			windowsApplication->UpdateCursorConfinement();
			windowsApplication->UpdateCursorVisibility();
			break;

		case WM_SETCURSOR:
			if (reinterpret_cast<HWND>(wParam) == windowHandle && LOWORD(lParam) == HTCLIENT)
			{
				windowsApplication->UpdateCursorVisibility();
				return TRUE;
			}
			break;

		case WM_MOUSEMOVE:
		case WM_CAPTURECHANGED:
			windowsApplication->UpdateCursorVisibility();
			break;

		default:
			break;
		}

		// 기록 후에도 아래 DefWindowProcW로 넘겨 Alt+F4 같은 시스템 키 처리를 유지한다.
		RecordInputMessage(windowHandle, windowsApplication->inputState, message, wParam, lParam);
	}

	if (message == WM_CLOSE && windowsApplication != nullptr)
	{
		// 렌더러가 창을 사용하는 동안은 창을 유지하고 프레임 루프의 종료만 요청한다.
		if (!windowsApplication->closeRequested)
		{
			windowsApplication->closeRequested = true;
			PostQuitMessage(0);
		}
		return 0;
	}

	if (message == WM_DESTROY)
	{
		if (windowsApplication == nullptr || !windowsApplication->closeRequested)
		{
			PostQuitMessage(0);
		}

		return 0;
	}

	if (message == WM_NCDESTROY)
	{
		WindowsApplication* application = reinterpret_cast<WindowsApplication*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
		if (application != nullptr)
		{
			application->window = nullptr;

			SetWindowLongPtrW(windowHandle, GWLP_USERDATA, 0);
		}
	}

	return DefWindowProcW(windowHandle, message, wParam, lParam);
}

const InputState& WindowsApplication::GetInputState() const
{
	return inputState;
}

void WindowsApplication::SetCursorConfined(bool confined)
{
	cursorConfinementRequested = confined;
	UpdateCursorConfinement();
}

void WindowsApplication::SetCursorVisible(bool visible)
{
	cursorVisibleRequested = visible;
	UpdateCursorVisibility();
}

void WindowsApplication::SetCursorShape(CursorShape shape)
{
	// 매 프레임 같은 모양으로 불려도 SetCursor를 반복하지 않는다.
	if (cursorShape == shape) { return; }

	cursorShape = shape;
	UpdateCursorVisibility();
}

bool WindowsApplication::RegisterRawMouseInput()
{
	RAWINPUTDEVICE device{};
	device.usUsagePage = 0x01; // Generic Desktop Controls
	device.usUsage = 0x02;     // Mouse
	device.dwFlags = 0;
	device.hwndTarget = window;

	if (!RegisterRawInputDevices(&device, 1, static_cast<UINT>(sizeof(device))))
	{
		LogWindowsError("RegisterRawInputDevices", GetLastError());
		return false;
	}

	rawMouseInputRegistered = true;
	return true;
}

bool WindowsApplication::UnregisterRawMouseInput()
{
	if (!rawMouseInputRegistered) { return true; }

	RAWINPUTDEVICE device{};
	device.usUsagePage = 0x01; // Generic Desktop Controls
	device.usUsage = 0x02;     // Mouse
	device.dwFlags = RIDEV_REMOVE;
	device.hwndTarget = nullptr;

	if (!RegisterRawInputDevices(&device, 1, static_cast<UINT>(sizeof(device))))
	{
		LogWindowsError("RegisterRawInputDevices", GetLastError());
		return false;
	}

	rawMouseInputRegistered = false;
	return true;
}

bool WindowsApplication::UpdateCursorConfinement()
{
	const bool shouldConfine = cursorConfinementRequested && window != nullptr && GetFocus() == window && !IsIconic(window);

	if (shouldConfine)
	{
		return ApplyCursorConfinement();
	}

	if (!cursorConfined) { return true; }

	if (!ClipCursor(nullptr))
	{
		LogWindowsError("ClipCursor", GetLastError());
		return false;
	}

	cursorConfined = false;
	return true;
}

bool WindowsApplication::ApplyCursorConfinement()
{
	RECT clientRect = {};
	if (!GetClientRect(window, &clientRect))
	{
		LogWindowsError("GetClientRect", GetLastError());
		return false;
	}

	if (clientRect.right <= clientRect.left || clientRect.bottom <= clientRect.top)
	{
		return false;
	}

	POINT topLeft{ clientRect.left, clientRect.top };
	POINT bottomRight{ clientRect.right, clientRect.bottom };

	if (!ClientToScreen(window, &topLeft) || !ClientToScreen(window, &bottomRight))
	{
		LogWindowsError("ClientToScreen", GetLastError());
		return false;
	}

	const RECT screenRect{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };

	if (!ClipCursor(&screenRect))
	{
		LogWindowsError("ClipCursor", GetLastError());
		return false;
	}

	cursorConfined = true;
	return true;
}

void WindowsApplication::UpdateCursorVisibility(bool forceVisible)
{
	if (window == nullptr) { return; }

	// 다른 창이나 비클라이언트 영역의 커서를 덮어쓰지 않는다. 캡처 중에는 창 밖도 제어할 수 있다.
	if (GetCapture() != window)
	{
		POINT cursorPosition{};
		if (!GetCursorPos(&cursorPosition))
		{
			LogWindowsError("GetCursorPos", GetLastError());
			return;
		}

		if (WindowFromPoint(cursorPosition) != window) { return; }

		RECT clientRect{};
		if (!GetClientRect(window, &clientRect))
		{
			LogWindowsError("GetClientRect", GetLastError());
			return;
		}

		if (!ScreenToClient(window, &cursorPosition))
		{
			LogWindowsError("ScreenToClient", GetLastError());
			return;
		}

		if (!PtInRect(&clientRect, cursorPosition)) { return; }
	}

	const bool shouldShow = forceVisible || cursorVisibleRequested || GetFocus() != window || IsIconic(window);
	HCURSOR cursor = nullptr;
	if (shouldShow)
	{
		cursor = LoadCursorW(nullptr, ToSystemCursorName(cursorShape));
		if (cursor == nullptr)
		{
			LogWindowsError("LoadCursorW", GetLastError());
			return;
		}
	}

	// 반환값은 이전 커서 핸들이므로 nullptr을 실패로 해석하지 않는다.
	SetCursor(cursor);
}
