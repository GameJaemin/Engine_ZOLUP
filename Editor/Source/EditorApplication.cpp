#include "EditorApplication.h"

#include "World.h"

bool EditorApplication::OnInitialize()
{
	worldSession.Open();
	return selection.SetWorld(worldSession.GetEditWorld());
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
}
