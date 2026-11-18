#include "EditorApplication.h"

#include "World.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <optional>

namespace
{
	/**
	 * 도킹 공간을 오른쪽 Inspector, 아래 Content Browser, 왼쪽 Hierarchy와 가운데 빈 칸으로 나눈다.
	 * 비율은 viewport 작업 영역 기준이다.
	 */
	void BuildDefaultDockLayout(ImGuiID dockSpaceID, const ImGuiViewport& viewport)
	{
		ImGui::DockBuilderAddNode(dockSpaceID, ImGuiDockNodeFlags_DockSpace);
		// 나누기 전에 크기를 정해야 나눈 비율이 정확하다.
		ImGui::DockBuilderSetNodeSize(dockSpaceID, viewport.WorkSize);

		// 나눌 때마다 centerID는 남은 칸으로 바뀌고, 마지막에 남은 칸이 가운데 빈 칸이 된다.
		ImGuiID centerID = dockSpaceID;
		const ImGuiID rightID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Right, 0.25f, nullptr, &centerID);
		const ImGuiID bottomID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Down, 0.3f, nullptr, &centerID);
		// 남은 폭은 전체의 75%이므로 전체의 20%가 되도록 0.2 / 0.75를 사용한다.
		const ImGuiID leftID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Left, 0.2f / 0.75f, nullptr, &centerID);

		ImGui::DockBuilderDockWindow(InspectorPanel::WindowName, rightID);
		ImGui::DockBuilderDockWindow(ContentBrowserPanel::WindowName, bottomID);
		ImGui::DockBuilderDockWindow(HierarchyPanel::WindowName, leftID);
		ImGui::DockBuilderFinish(dockSpaceID);
	}

	/**
	 * 도킹 공간의 가운데 칸 영역을 클라이언트 픽셀 좌표로 돌려준다.
	 * 칸이 없거나 크기가 0 이하이면 nullopt다. DockSpaceOverViewport 다음에 호출한다.
	 */
	std::optional<ViewportRect> FindCentralNodeRect(ImGuiID dockSpaceID, const ImGuiViewport& viewport)
	{
		const ImGuiDockNode* centralNode = ImGui::DockBuilderGetCentralNode(dockSpaceID);
		if (centralNode == nullptr) { return std::nullopt; }

		if (!(centralNode->Size.x > 0.0f) || !(centralNode->Size.y > 0.0f)) { return std::nullopt; }

		// ImGui 위치는 뷰포트 기준 좌표이므로 뷰포트 시작 위치를 빼서 클라이언트 좌표로 바꾼다.
		return ViewportRect{ centralNode->Pos.x - viewport.Pos.x, centralNode->Pos.y - viewport.Pos.y, centralNode->Size.x, centralNode->Size.y };
	}

}

bool EditorApplication::OnInitialize(Engine& inEngine)
{
	engine = &inEngine;

	// Engine이 Run 초기화에서 ImGui Context를 만든 뒤 호출하므로 UI 호출이 아닌 설정 변경은 여기서 할 수 있다.
	// 패널을 탭과 칸으로 붙일 수 있게 한다. 렌더러가 창 밖 출력을 지원하지 않아 멀티 뷰포트는 켜지 않는다.
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	worldSession.Open();

	if (!selection.SetWorld(worldSession.GetEditWorld()))
	{
		return false;
	}

	// 파일시스템 탐색만 초기화하므로 ImGui Context나 프레임은 필요하지 않다.
	if (!contentBrowserPanel.Initialize())
	{
		return false;
	}

	return true;
}

void EditorApplication::OnUpdate(const InputState& input, float deltaTime)
{
	cameraController.Update(editorCamera, input, deltaTime, true);
	worldSession.GetEditWorld()->DestroyPendingGameObjects();
}

void EditorApplication::OnGui()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImGuiID dockSpaceID = ImGui::GetID("EditorDockSpace");

	// imgui.ini를 꺼 두어 실행할 때마다 도킹 공간이 없으므로 첫 프레임에 기본 배치를 만든다.
	// 이후 프레임에는 노드가 있으므로 다시 만들지 않고, 사용자가 바꾼 배치를 유지한다.
	if (ImGui::DockBuilderGetNode(dockSpaceID) == nullptr)
	{
		BuildDefaultDockLayout(dockSpaceID, *viewport);
	}

	// 엔진 창 전체를 패널을 붙일 수 있는 도킹 공간으로 만든다. 패널보다 먼저 매 프레임 호출한다.
	// 가운데 빈 칸은 투명하게 두어 뒤의 화면이 보이고 입력도 통과하게 한다.
	ImGui::DockSpaceOverViewport(dockSpaceID, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

	// 장면은 패널이 없는 가운데 칸에만 그린다.
	sceneViewport = FindCentralNodeRect(dockSpaceID, *viewport);

	World* world = worldSession.GetEditWorld();
	if (world == nullptr) { return; }

	// 삭제 대기 객체는 OnUpdate에서 이미 처리했으므로 패널을 그리는 동안 World의 객체 목록은 바뀌지 않는다.
	// Hierarchy에서 바뀐 선택을 같은 프레임의 Inspector가 읽도록 먼저 그린다.
	hierarchyPanel.Draw(*world, selection);
	inspectorPanel.Draw(selection);

	// 콘텐츠 파일 선택은 패널 내부에서 관리하므로 EditorSelection을 전달하지 않는다.
	contentBrowserPanel.Draw();
}

std::optional<SceneView> EditorApplication::GetSceneView()
{
	World* world = worldSession.GetEditWorld();
	if (world == nullptr || !sceneViewport.has_value()) { return std::nullopt; }

	return SceneView{ world->GetRenderScene(), editorCamera.GetCameraViewInfo(), sceneViewport };
}

void EditorApplication::OnShutdown() noexcept
{
	selection.SetWorld(nullptr);
	worldSession.Close();

	engine = nullptr;
}
