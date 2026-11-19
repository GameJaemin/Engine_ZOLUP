#pragma once

#include "Renderer/TextureRenderTarget2D.h"

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
	 * 생성 및 크기 변경 요청은 같은 프레임의 Sketch에서 소비해야 한다.
	 */
	void Draw();

	/** DataHub 종료 전에 호출한다. 실패하면 렌더 타깃 상태를 유지한다. */
	bool Shutdown();

public:
	/** 패널이 소유한 타깃이다. 패널 수명 동안 빌리며 표시 여부나 초기화 상태와 관계없이 반환한다. */
	const TextureRenderTarget2D& GetRenderTarget() const { return renderTarget; }

	/**
	 * 현재 프레임의 Draw 이후에 읽는다. 표시 영역의 너비와 높이가 모두 1픽셀 이상이고
	 * 해당 크기의 타깃 준비 요청이 접수되었으면 true다. GPU 자원 생성 완료를 뜻하지 않는다.
	 * 다음 Draw 시작 또는 Shutdown에서 false로 초기화한다.
	 */
	bool ShouldRenderScene() const { return shouldRenderScene; }

	/** 바로 앞 Draw에서 장면 이미지 위의 우클릭 시작을 감지했으면 true다. */
	bool WasImageRightClicked() const { return wasImageRightClicked; }

public:
	// 기본 도킹 배치와 창 생성에서 같은 이름을 사용한다.
	static constexpr const char* WindowName = "Viewport";

private:
	// 현재 단일 Viewport 패널이 사용하는 등록 이름이다.
	static constexpr const wchar_t* RenderTargetName = L"Editor.SceneViewport";

	TextureRenderTarget2D renderTarget;
	bool shouldRenderScene = false;
	bool wasImageRightClicked = false;
};
