#pragma once

class EditorViewport;

class ViewportPanel
{
public:
	ViewportPanel() = default;
	ViewportPanel(const ViewportPanel&) = delete;
	ViewportPanel& operator=(const ViewportPanel&) = delete;
	ViewportPanel(ViewportPanel&&) = delete;
	ViewportPanel& operator=(ViewportPanel&&) = delete;

public:
	/**
	 * DataHub와 ImGui가 준비된 상태에서 NewFrame 이후, Render 전에 호출한다.
	 * 같은 프레임에 viewport.ResetRenderRequestState()를 먼저 호출한다.
	 * 생성 및 크기 변경 요청은 같은 프레임의 Sketch에서 소비해야 한다.
	 */
	void Draw(EditorViewport& viewport);

	/** 바로 앞 Draw에서 장면 이미지 위의 우클릭 시작을 감지했으면 true다. */
	bool WasImageRightClicked() const { return wasImageRightClicked; }

public:
	// 기본 도킹 배치와 창 생성에서 같은 이름을 사용한다.
	static constexpr const char* WindowName = "Viewport";

private:
	bool wasImageRightClicked = false;
};
