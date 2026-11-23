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
};
