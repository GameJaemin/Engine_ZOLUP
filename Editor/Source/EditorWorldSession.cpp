#include "EditorWorldSession.h"

#include "EditorSelection.h"
#include "LevelLoader.h"
#include "World.h"

#include <optional>
#include <utility>
#include <vector>

namespace
{
	std::unique_ptr<World> LoadEditWorld(const std::filesystem::path& levelPath)
	{
		std::unique_ptr<World> newWorld = std::make_unique<World>();
		const std::optional<std::vector<GameObject*>> loadedObjects = LoadLevel(levelPath, *newWorld);
		if (!loadedObjects.has_value()) { return nullptr; }

		return newWorld;
	}
}

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
	editWorld = LoadEditWorld(levelPath);
	return editWorld != nullptr;
}

bool EditorWorldSession::ReplaceLevel(const std::filesystem::path& levelPath, EditorSelection& selection)
{
	if (!editWorld) { return false; }

	std::unique_ptr<World> newWorld = LoadEditWorld(levelPath);
	if (!newWorld || !selection.SetWorld(newWorld.get())) { return false; }

	// World 소멸 시 메시 컴포넌트가 객체 및 본 데이터를 해제한다.
	editWorld->GetRenderScene().ReleaseLightData();
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
