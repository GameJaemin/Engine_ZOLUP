#include "EditorApplication.h"

#include "Core/Log.h"
#include "Engine.h"
#include "Input/InputState.h"
#include "Input/Key.h"
#include "Paths.h"
#include "World.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
	// 맵 열기 기능이 생기기 전까지 에디터가 시작할 때 여는 맵이다. 저장소 루트 기준 경로다.
	constexpr std::string_view DefaultLevelPath = "Game/Content/Maps/DevMap.json";

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

	/** 마우스 위치가 영역 안에 있으면 true다. 위치를 모르면 false다. */
	bool IsMouseInside(const std::optional<ViewportRect>& rect, const std::optional<MouseCoordinates>& mousePosition)
	{
		if (!rect.has_value() || !mousePosition.has_value()) { return false; }

		const float x = static_cast<float>(mousePosition->x);
		const float y = static_cast<float>(mousePosition->y);
		return x >= rect->x && x < rect->x + rect->width && y >= rect->y && y < rect->y + rect->height;
	}
}

bool EditorApplication::OnInitialize(Engine& inEngine)
{
	engine = &inEngine;

	// Engine이 Run 초기화에서 ImGui Context를 만든 뒤 호출하므로 UI 호출이 아닌 설정 변경은 여기서 할 수 있다.
	// 패널을 탭과 칸으로 붙일 수 있게 한다. 렌더러가 창 밖 출력을 지원하지 않아 멀티 뷰포트는 켜지 않는다.
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// TODO: 맵 열기 기능이 생기면 기본 맵 대신 사용자가 고른 맵을 연다.
	std::filesystem::path levelPath;
	if (!Paths::GetAbsolutePath(DefaultLevelPath, levelPath))
	{
		Log::Write(LogLevel::Error, "Editor", "Could not resolve the default level path from the repository root");
	}
	else if (!worldSession.OpenLevel(levelPath))
	{
		Log::Write(LogLevel::Error, "Editor", "Failed to open the default level");
	}

	// 기본 맵을 열지 못해도 패널과 콘텐츠 브라우저는 쓸 수 있으므로 빈 World로 시작한다.
	if (worldSession.GetEditWorld() == nullptr)
	{
		worldSession.NewLevel();
	}

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
	const bool wasViewportCameraActive = isViewportCameraActive;

	// 오른쪽 버튼을 누른 순간에만 뷰포트 위인지 판단하고, 뗄 때까지 그 결과를 유지한다.
	// 장면 영역은 지난 프레임 OnGui에서 계산한 값이다. 떠 있는 패널이 뷰포트를 덮은 곳은 ImGui가 마우스를 잡는다.
	for (const KeyEvent& event : input.GetFrameEvents())
	{
		if (event.key != Key::MouseRight) { continue; }

		if (event.type == KeyEventType::Pressed)
		{
			isViewportCameraActive = IsMouseInside(sceneViewport, input.GetMousePosition()) && !ImGui::GetIO().WantCaptureMouse;
		}
		else
		{
			isViewportCameraActive = false;
		}
	}

	// 카메라를 조작하는 동안 커서를 숨긴다. 상태가 바뀔 때만 엔진에 알린다.
	if (isViewportCameraActive != wasViewportCameraActive)
	{
		engine->SetCursorVisible(!isViewportCameraActive);
	}

	cameraController.Update(editorCamera, input, deltaTime, isViewportCameraActive);
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
	// 조작 중에 종료되면 숨긴 커서가 남지 않도록 되돌린다.
	if (isViewportCameraActive)
	{
		engine->SetCursorVisible(true);
		isViewportCameraActive = false;
	}

	selection.SetWorld(nullptr);
	worldSession.Close();

	engine = nullptr;
}
