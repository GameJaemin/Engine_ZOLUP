#pragma once

/**
 * 장면을 출력할 사각형 영역이다. 창 클라이언트 영역의 픽셀 단위이며,
 * 원점은 왼쪽 위이고 x는 오른쪽, y는 아래로 증가한다.
 */
struct ViewportRect
{
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};
