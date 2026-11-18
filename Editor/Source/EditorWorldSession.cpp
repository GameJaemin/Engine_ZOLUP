#include "EditorWorldSession.h"

#include "LevelLoader.h"
#include "World.h"

#include <optional>
#include <utility>
#include <vector>

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

bool EditorWorldSession::OpenLevel(const std::filesystem::path& levelPath)
{
	if (editWorld) { return false; }

	// 불러오기에 성공한 World만 세션에 넣는다. 실패하면 세션은 닫힌 상태로 남는다.
	std::unique_ptr<World> newWorld = std::make_unique<World>();
	const std::optional<std::vector<GameObject*>> loadedObjects = LoadLevel(levelPath, *newWorld);
	if (!loadedObjects.has_value()) { return false; }

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
