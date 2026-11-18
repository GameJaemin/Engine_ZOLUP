#pragma once

/** 클라이언트 영역에 보일 시스템 커서 모양이다. 숨김은 모양이 아니라 표시 설정으로 다룬다. */
enum class CursorShape
{
	Arrow,
	TextInput,
	ResizeAll,
	// 위아래, 좌우, 오른쪽 위와 왼쪽 아래, 왼쪽 위와 오른쪽 아래 방향 크기 조절이다.
	ResizeNS,
	ResizeEW,
	ResizeNESW,
	ResizeNWSE,
	Hand,
	Wait,
	Progress,
	NotAllowed
};
