#pragma once

#include "EditorSelection.h"
#include "EditorViewport.h"
#include "EditorWorldSession.h"
#include "IApplication.h"
#include "Input/InputState.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#include "Renderer/SceneRenderRequest.h"

#include <optional>
#include <string>

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
	/** OnUpdate에서 승인된 요청을 한 번 처리한다. 실패 경로는 OnGui에서 표시한다. */
	void ProcessMapOpenRequest();

	/** OnGui에서 맵 전환 확인과 실패를 표시한다. */
	void DrawMapOpenDialogs();

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

	// OnUpdate에서 받은 이번 프레임의 클라이언트 좌표다.
	std::optional<MouseCoordinates> leftDoubleClickPosition;

	// 비교용 에셋 키다. 빈 World로 시작하면 비어 있다.
	std::string currentMapKey;
	std::optional<std::string> pendingMapPath;
	std::optional<std::string> failedMapPath;
	bool canOpenPendingMap = false;
	bool hasUnsavedMapChanges = false;
};
