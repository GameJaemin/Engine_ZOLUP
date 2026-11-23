#include "EditorWorldSession.h"

#include "EditorSelection.h"
#include "LevelLoader.h"
#include "World.h"

#include <utility>

EditorWorldSession::EditorWorldSession() = default;

EditorWorldSession::~EditorWorldSession()
{
	Close();
}

void EditorWorldSession::NewLevel()
{
	if (editWorld) { return; }

	editWorld = std::make_unique<World>();
}

bool EditorWorldSession::OpenLevel(const std::filesystem::path& levelPath, EditorSelection& selection)
{
	std::unique_ptr<World> newWorld = std::make_unique<World>();
	if (!LoadLevel(levelPath, *newWorld).has_value()) { return false; }
	if (!selection.SetWorld(newWorld.get())) { return false; }

	if (editWorld)
	{
		editWorld->GetRenderScene().ReleaseLightData();
	}
	editWorld = std::move(newWorld);

	return true;
}

void EditorWorldSession::Close() noexcept
{
	if (!editWorld) { return; }

	// 종료 시 렌더 데이터는 DataHub가 일괄 해제한다.
	editWorld->GetRenderScene().ClearObjectDataIds();
	editWorld.reset();
}

World* EditorWorldSession::GetEditWorld()
{
	return editWorld.get();
}

const World* EditorWorldSession::GetEditWorld() const
{
	return editWorld.get();
}
