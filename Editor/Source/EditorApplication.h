#pragma once

#include "EditorCamera.h"
#include "EditorCameraController.h"
#include "EditorSelection.h"
#include "EditorWorldSession.h"
#include "IApplication.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"

#include <optional>

class EditorApplication final : public IApplication
{
public:
	bool OnInitialize() override;
	void OnUpdate(const InputState& input, float deltaTime) override;
	std::optional<SceneView> GetSceneView() override;
	void OnShutdown() noexcept override;

	/**
	 * 유효한 ImGui Context에서 NewFrame 이후, Render 전에 호출한다.
	 * 세션이 닫혀 있으면 아무것도 그리지 않는다.
	 * 패널을 그리는 동안 World의 실제 삭제 처리를 실행하지 않는다.
	 */
	void DrawPanels();

private:
	EditorWorldSession worldSession;
	EditorSelection selection;
	EditorCamera editorCamera;
	EditorCameraController cameraController;
	HierarchyPanel hierarchyPanel;
	InspectorPanel inspectorPanel;
};
