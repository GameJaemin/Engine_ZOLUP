#pragma once

#include "EditorCamera.h"
#include "EditorCameraController.h"
#include "EditorSelection.h"
#include "EditorWorldSession.h"
#include "IApplication.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"

#include <optional>

class Engine;

class EditorApplication final : public IApplication
{
public:
	bool OnInitialize(Engine& inEngine) override;
	void OnUpdate(const InputState& input, float deltaTime) override;
	std::optional<SceneView> GetSceneView() override;
	void OnShutdown() noexcept override;

	/**
	 * 호출자는 현재 Context에서 NewFrame 이후, Render 전에만 true를 전달한다.
	 * 프레임 활성 여부를 전달하지 않거나 Context 또는 편집 World가 없으면 그리지 않는다.
	 * 패널을 그리는 동안 World의 실제 삭제 처리를 실행하지 않는다.
	 */
	void DrawPanels(bool isImGuiFrameActive = false);

private:
	// OnInitialize에서 빌리고 OnShutdown에서 비운다. 소유하지 않는다.
	Engine* engine = nullptr;

	EditorWorldSession worldSession;
	EditorSelection selection;
	EditorCamera editorCamera;
	EditorCameraController cameraController;
	HierarchyPanel hierarchyPanel;
	InspectorPanel inspectorPanel;
	// 콘텐츠 파일 선택은 GameObject 전용 EditorSelection과 별도로 관리한다.
	ContentBrowserPanel contentBrowserPanel;
};
