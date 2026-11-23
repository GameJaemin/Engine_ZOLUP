#pragma once

#include "Input/Key.h"

#include <array>
#include <optional>
#include <vector>

/** 픽셀 단위 마우스 위치다. +X는 오른쪽, +Y는 아래쪽이다. */
struct MouseCoordinates
{
	int x = 0;
	int y = 0;
};

/**
 * Raw Input으로 받은 상대 이동량이다. 화면 픽셀 단위가 아닌 장치 보고 단위다.
 * +X는 오른쪽, +Y는 아래쪽이다.
 */
struct RawMouseDelta
{
	int x{};
	int y{};
};

/**
 * 창에서 수집한 키와 마우스 입력을 메인 스레드에서 보관한다.
 * 키별 누르는 중 여부와 이번 프레임에 들어온 순서대로의 키 이벤트를 함께 유지한다.
 */
class InputState
{
	// 프레임 갱신
public:
	/**
	 * 메시지 처리 전에 프레임당 한 번 호출한다. 키 이벤트 목록, 더블클릭 위치, 상대 이동량 및 휠 누적값을 비운다.
	 * 누르는 중인 키, 마지막 마우스 위치와 이벤트 목록 메모리는 유지한다.
	 */
	void ClearFrameEvents();

	// 키 상태
public:
	/**
	 * 누르는 중이 아니던 키만 누름으로 기록하고 이벤트를 추가한다.
	 * 자동 반복과 Unknown은 무시한다. 할당 실패 시 상태를 바꾸지 않고 예외를 전파한다.
	 */
	void PressKey(Key key);

	/**
	 * 누르는 중인 키만 뗌으로 기록하고 이벤트를 추가한다. 짝 없는 뗌 이벤트를 만들지 않는다.
	 * 할당 실패 시 상태를 바꾸지 않고 예외를 전파한다.
	 */
	void ReleaseKey(Key key);

	/**
	 * 포커스 상실 시 호출한다. 누르는 중인 키마다 Key 값 순서로 뗌 이벤트를 추가한다.
	 * 실제 뗌과 강제 뗌을 구분하지 않는다. 할당 실패 시 이미 처리한 키는 유지하고 예외를 전파한다.
	 */
	void ReleaseAllKeys();

	/** Unknown과 범위 밖 값은 false를 반환한다. */
	bool IsKeyDown(Key key) const;

	/**
	 * 이번 프레임에 들어온 순서대로의 이벤트를 빌려준다.
	 * 다음 ClearFrameEvents 또는 키 기록 전까지만 순회한다.
	 */
	const std::vector<KeyEvent>& GetFrameEvents() const;

private:
	// 인덱스는 Key 값이다.
	std::array<bool, KeyCount> downKeys = {};
	std::vector<KeyEvent> frameEvents;

	// 마우스 입력
public:
	/** 클라이언트 좌표의 현재 마우스 위치를 기록한다. */
	void RecordMousePosition(int x, int y);

	/** 커서가 창을 벗어날 때 저장한 위치를 지운다. 상대 이동량은 유지한다. */
	void InvalidateMousePosition();

	/** 포커스 상실 시 저장한 마우스 위치, 더블클릭 위치, 상대 이동량 및 휠 누적값을 비운다. */
	void ResetMouseInput();

	/** 마지막으로 기록한 클라이언트 좌표를 반환한다. 유효한 위치가 없으면 std::nullopt를 반환한다. */
	std::optional<MouseCoordinates> GetMousePosition() const;

	/** 왼쪽 버튼 더블클릭 메시지의 클라이언트 좌표를 기록한다. */
	void RecordLeftDoubleClick(int x, int y);

	/** 이번 프레임에서 마지막 왼쪽 버튼 더블클릭 위치를 반환한다. */
	std::optional<MouseCoordinates> GetLeftDoubleClickPosition() const;

	/** 상대 이동량을 이번 프레임의 누적값에 더한다. 화면 픽셀 단위가 아니다. */
	void AddRawMouseDelta(int deltaX, int deltaY);

	/** 이번 프레임에 누적된 상대 이동량을 반환한다. */
	RawMouseDelta GetRawMouseDelta() const;

	/** 이번 프레임의 휠 이동량에 더한다. 앞으로 한 칸은 +1.0이며 소수 입력도 받는다. */
	void AddMouseWheelDelta(float delta);

	/** 이번 프레임에 누적한 휠 이동량을 반환한다. 단위는 휠 한 칸이다. */
	float GetMouseWheelDelta() const;

private:
	std::optional<MouseCoordinates> mousePosition;
	std::optional<MouseCoordinates> leftDoubleClickPosition;
	// 이번 프레임에 누적된 장치 보고 단위의 상대 이동량이다.
	RawMouseDelta rawMouseDelta{};
	float mouseWheelDelta = 0.0f;
};
