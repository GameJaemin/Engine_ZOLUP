#pragma once

#include "EditorSelection.h"
#include "EditorViewport.h"
#include "EditorWorldSession.h"
#include "IApplication.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#include "Renderer/SceneRenderRequest.h"

#include <optional>

class Engine;

class EditorApplication final : public IApplication
{
public:
	// 패널 UI가 에디터의 핵심 기능이므로 GUI 준비를 요구한다.
	bool UsesGui() const noexcept override { return true; }

	bool OnInitialize(Engine& inEngine) override;
	void OnUpdate(const InputState& input, float deltaTime) override;
	void OnGui() override;
	std::optional<SceneRenderRequest> GetRenderRequest() override;
	void OnShutdown() noexcept override;

private:
	void ProcessLevelOpenRequest();

	// 툴바 버튼이 남기는 요청이다. Paused에서 Play 버튼은 Resume을 남긴다.
	enum class EditorPlayRequest : unsigned char { Play, Pause, Resume, Stop };

	/** OnGui에서 Play, Pause, Stop 버튼을 그리고 누른 버튼의 요청만 남긴다. */
	void DrawPlayToolbar();

	/** OnUpdate 시작에서 남은 요청을 세션에 전달하고 비운다. */
	void ProcessPlayRequest();

private:
	// OnInitialize에서 빌리고 OnShutdown에서 비운다. 소유하지 않는다.
	Engine* engine = nullptr;

	EditorWorldSession worldSession;
	EditorSelection selection;
	EditorViewport editorViewport;
	HierarchyPanel hierarchyPanel;
	InspectorPanel inspectorPanel;
	// 콘텐츠 파일 선택은 GameObject 전용 EditorSelection과 별도로 관리한다.
	ContentBrowserPanel contentBrowserPanel;
	ViewportPanel viewportPanel;

	// 다음 OnUpdate에서 처리할 요청이다. 패널이 World를 순회하는 중에 World를 바꾸지 않도록 미룬다.
	std::optional<EditorPlayRequest> playRequest;
};
