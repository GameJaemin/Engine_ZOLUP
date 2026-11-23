#include "EditorApplication.h"

#include "AssetManager.h"
#include "Core/Log.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector3.h"
#include "Engine.h"
#include "Paths.h"
#include "Prefab.h"
#include "World.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
	// 에디터가 시작할 때 여는 맵의 저장소 기준 경로다.
	constexpr std::string_view DefaultLevelPath = "Game/Content/Maps/DevMap.json";
	constexpr char ConfirmMapOpenPopup[] = "Unsaved Map Changes";
	constexpr char MapOpenFailedPopup[] = "Could Not Open Map";

	/**
	 * 도킹 공간을 오른쪽 Inspector, 아래 Content Browser, 왼쪽 Hierarchy와 가운데 Viewport로 나눈다.
	 * 비율은 viewport 작업 영역 기준이다.
	 */
	void BuildDefaultDockLayout(ImGuiID dockSpaceID, const ImGuiViewport& viewport)
	{
		ImGui::DockBuilderAddNode(dockSpaceID, ImGuiDockNodeFlags_DockSpace);
		// 나누기 전에 크기를 정해야 나눈 비율이 정확하다.
		ImGui::DockBuilderSetNodeSize(dockSpaceID, viewport.WorkSize);

		// 나눌 때마다 centerID는 남은 칸으로 바뀌고, 마지막에 남은 칸에 Viewport를 배치한다.
		ImGuiID centerID = dockSpaceID;
		const ImGuiID rightID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Right, 0.25f, nullptr, &centerID);
		const ImGuiID bottomID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Down, 0.3f, nullptr, &centerID);
		// 남은 폭은 전체의 75%이므로 전체의 20%가 되도록 0.2 / 0.75를 사용한다.
		const ImGuiID leftID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Left, 0.2f / 0.75f, nullptr, &centerID);

		ImGui::DockBuilderDockWindow(InspectorPanel::WindowName, rightID);
		ImGui::DockBuilderDockWindow(ContentBrowserPanel::WindowName, bottomID);
		ImGui::DockBuilderDockWindow(HierarchyPanel::WindowName, leftID);
		ImGui::DockBuilderDockWindow(ViewportPanel::WindowName, centerID);
		ImGui::DockBuilderFinish(dockSpaceID);
	}
}

bool EditorApplication::OnInitialize(Engine& inEngine)
{
	engine = &inEngine;

	// Engine이 Run 초기화에서 ImGui Context를 만든 뒤 호출하므로 UI 호출이 아닌 설정 변경은 여기서 할 수 있다.
	// 패널을 탭과 칸으로 붙일 수 있게 한다. 렌더러가 창 밖 출력을 지원하지 않아 멀티 뷰포트는 켜지 않는다.
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	std::filesystem::path levelPath;
	if (!Paths::GetAbsolutePath(DefaultLevelPath, levelPath))
	{
		Log::Write(LogLevel::Error, "Editor", "Could not resolve the default level path from the repository root");
	}
	else if (!worldSession.OpenLevel(levelPath))
	{
		Log::Write(LogLevel::Error, "Editor", "Failed to open the default level");
	}
	else
	{
		currentMapKey = Paths::MakeAssetKey(DefaultLevelPath);
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
	leftDoubleClickPosition = input.GetLeftDoubleClickPosition();
	ProcessMapOpenRequest();

	if (editorViewport.UpdateCamera(input, deltaTime))
	{
		engine->SetCursorVisible(true);
	}

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
	// 가운데 도킹 창이 없을 때는 뒤의 화면이 보이고 입력도 통과하게 한다.
	ImGui::DockSpaceOverViewport(dockSpaceID, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

	editorViewport.ResetRenderRequestState();
	const std::optional<std::string> droppedPrefabPath = viewportPanel.Draw(editorViewport);

	if (editorViewport.TryBeginCameraControl(viewportPanel.WasImageRightClicked()))
	{
		engine->SetCursorVisible(false);
	}

	World* world = worldSession.GetEditWorld();
	if (world == nullptr) { return; }

	if (droppedPrefabPath.has_value())
	{
		const std::shared_ptr<const Prefab> prefab = AssetManager::Get().LoadPrefab(*droppedPrefabPath);
		if (prefab == nullptr)
		{
			Log::Write(LogLevel::Warning, "Editor", "Failed to load the dropped prefab");
		}
		else
		{
			Transform transform = prefab->root.localTransform;
			transform.position = Vector3{ 0.0f, 0.0f, 0.0f };
			const PrefabPlacement placement{ .transform = transform };
			GameObject* root = world->InstantiatePrefab(*prefab, placement);
			if (root != nullptr)
			{
				hasUnsavedMapChanges = true;
			}
			if (!selection.Select(root))
			{
				Log::Write(LogLevel::Warning, "Editor", "Could not select the dropped prefab instance");
			}
		}
	}

	// 생성과 삭제 처리를 마친 뒤 World의 객체 목록을 순회한다.
	// Hierarchy에서 바뀐 선택을 같은 프레임의 Inspector가 읽도록 먼저 그린다.
	hierarchyPanel.Draw(*world, selection);
	if (inspectorPanel.Draw(selection))
	{
		hasUnsavedMapChanges = true;
	}

	// 콘텐츠 파일 선택은 패널 내부에서 관리하므로 EditorSelection을 전달하지 않는다.
	std::optional<std::string> requestedMapPath = contentBrowserPanel.Draw(leftDoubleClickPosition);
	if (requestedMapPath && !pendingMapPath && !failedMapPath && Paths::MakeAssetKey(*requestedMapPath) != currentMapKey)
	{
		pendingMapPath = std::move(requestedMapPath);
		canOpenPendingMap = !hasUnsavedMapChanges;
	}
	DrawMapOpenDialogs();
}

std::optional<SceneRenderRequest> EditorApplication::GetRenderRequest()
{
	World* world = worldSession.GetEditWorld();

	if (world == nullptr) { return std::nullopt; }
	return editorViewport.BuildRenderRequest(world->GetRenderScene());
}

void EditorApplication::OnShutdown() noexcept
{
	// 조작 중에 종료되면 숨긴 커서가 남지 않도록 되돌린다.
	if (editorViewport.StopCameraControl())
	{
		engine->SetCursorVisible(true);
	}

	if (!editorViewport.ShutdownRenderTarget())
	{
		Log::Write(LogLevel::Error, "Editor", "Failed to request viewport render target release");
	}

	selection.SetWorld(nullptr);
	worldSession.Close();

	currentMapKey.clear();
	leftDoubleClickPosition.reset();
	pendingMapPath.reset();
	failedMapPath.reset();
	canOpenPendingMap = false;
	hasUnsavedMapChanges = false;
	engine = nullptr;
}

void EditorApplication::ProcessMapOpenRequest()
{
	if (!pendingMapPath || !canOpenPendingMap) { return; }

	std::string requestedMapPath = std::move(*pendingMapPath);
	pendingMapPath.reset();
	canOpenPendingMap = false;

	std::string requestedMapKey = Paths::MakeAssetKey(requestedMapPath);
	std::filesystem::path levelPath;
	if (!Paths::GetAbsolutePath(requestedMapPath, levelPath) || !worldSession.ReplaceLevel(levelPath, selection))
	{
		Log::Write(LogLevel::Error, "Editor", "Could not open map: " + requestedMapPath);
		failedMapPath = std::move(requestedMapPath);
		return;
	}

	currentMapKey = std::move(requestedMapKey);
	hasUnsavedMapChanges = false;
	Log::Write(LogLevel::Info, "Editor", "Opened map: " + requestedMapPath);
}

void EditorApplication::DrawMapOpenDialogs()
{
	if (pendingMapPath && !canOpenPendingMap)
	{
		if (!ImGui::IsPopupOpen(ConfirmMapOpenPopup))
		{
			ImGui::OpenPopup(ConfirmMapOpenPopup);
		}
		if (ImGui::BeginPopupModal(ConfirmMapOpenPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Opening another map will discard unsaved changes.");
			ImGui::TextWrapped("%s", pendingMapPath->c_str());
			if (ImGui::Button("Discard Changes and Open"))
			{
				canOpenPendingMap = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				pendingMapPath.reset();
				canOpenPendingMap = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::EndPopup();
		}
	}

	if (failedMapPath)
	{
		if (!ImGui::IsPopupOpen(MapOpenFailedPopup))
		{
			ImGui::OpenPopup(MapOpenFailedPopup);
		}
		if (ImGui::BeginPopupModal(MapOpenFailedPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("The current map is still open.");
			ImGui::TextWrapped("%s", failedMapPath->c_str());
			if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				failedMapPath.reset();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::EndPopup();
		}
	}
}
