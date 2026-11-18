#include "EditorApplication.h"

#include "World.h"

#include <imgui.h>

bool EditorApplication::OnInitialize(Engine& inEngine)
{
	engine = &inEngine;

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

std::optional<SceneView> EditorApplication::GetSceneView()
{
	return std::nullopt;
}

void EditorApplication::OnShutdown() noexcept
{
	selection.SetWorld(nullptr);
	worldSession.Close();

	engine = nullptr;
}

void EditorApplication::DrawPanels(bool isImGuiFrameActive)
{
	// 프레임 진행 여부는 호출자가 전달하고, Context 존재 여부는 직접 확인한다.
	if (!isImGuiFrameActive || ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}

	World* world = worldSession.GetEditWorld();
	if (world == nullptr) { return; }

	hierarchyPanel.Draw(*world, selection);
	inspectorPanel.Draw(selection);

	// 콘텐츠 파일 선택은 패널 내부에서 관리하므로 EditorSelection을 전달하지 않는다.
	contentBrowserPanel.Draw();
}
