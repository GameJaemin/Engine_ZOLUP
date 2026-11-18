#pragma once

#include "EditorCamera.h"
#include "EditorCameraController.h"
#include "EditorSelection.h"
#include "EditorWorldSession.h"
#include "IApplication.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Renderer/ViewportRect.h"

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
	std::optional<SceneView> GetSceneView() override;
	void OnShutdown() noexcept override;

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

	// OnGui가 매 프레임 계산하는 도킹 가운데 칸의 클라이언트 영역이다. 같은 프레임의 GetSceneView에서 사용한다.
	// 칸을 찾지 못했거나 크기가 없으면 nullopt이다.
	std::optional<ViewportRect> sceneViewport;
	// 장면 뷰포트 위에서 오른쪽 버튼을 누른 뒤 뗄 때까지 true다. 이 동안만 에디터 카메라가 입력을 받는다.
	bool isViewportCameraActive = false;
};
