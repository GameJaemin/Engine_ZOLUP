#include "EditorWorldSession.h"

#include "EditorSelection.h"
#include "GameObject.h"
#include "LevelLoader.h"
#include "ObjectID.h"
#include "SceneSnapshot.h"
#include "World.h"

#include <optional>
#include <utility>

namespace
{
	// 선택이 없으면 무효 ID다.
	ObjectID GetSelectedObjectID(const EditorSelection& selection)
	{
		const GameObject* selected = selection.GetSelectedGameObject();
		return selected != nullptr ? selected->GetObjectID() : ObjectID{};
	}

	// 무효 ID이거나 world에 없으면 선택이 비어 있는 채로 둔다.
	void SelectByObjectID(EditorSelection& selection, World& world, const ObjectID& objectID)
	{
		if (!objectID.IsValid()) { return; }
		if (GameObject* object = world.FindGameObject(objectID)) { selection.Select(object); }
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

bool EditorWorldSession::OpenLevel(const std::filesystem::path& levelPath, EditorSelection& selection)
{
	// Edit World를 바꾸면 Play World와 원본이 어긋나므로 Play 중에는 거절한다.
	if (playState != EditorPlayState::Editing) { return false; }

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
	// 종료 시 렌더 데이터는 DataHub가 일괄 해제한다.
	if (playWorld)
	{
		playWorld->GetRenderScene().ClearObjectDataIds();
		playWorld.reset();
	}
	playState = EditorPlayState::Editing;

	if (!editWorld) { return; }

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

bool EditorWorldSession::StartPlay(EditorSelection& selection)
{
	if (playState != EditorPlayState::Editing || !editWorld) { return false; }

	// Capture는 실패 이유를 로그로 남기며 Edit World를 바꾸지 않는다.
	const std::optional<SceneSnapshot> snapshot = CaptureWorld(*editWorld);
	if (!snapshot.has_value()) { return false; }

	// 실패한 새 World는 한 번도 그리지 않았으므로 렌더 자원을 정리할 필요 없이 버린다.
	std::unique_ptr<World> newWorld = std::make_unique<World>();
	if (!RestoreWorld(*snapshot, *newWorld)) { return false; }

	const ObjectID selectedObjectID = GetSelectedObjectID(selection);
	if (!selection.SetWorld(newWorld.get())) { return false; }
	SelectByObjectID(selection, *newWorld, selectedObjectID);

	playWorld = std::move(newWorld);
	playState = EditorPlayState::Playing;
	return true;
}

void EditorWorldSession::StopPlay(EditorSelection& selection)
{
	if (playState == EditorPlayState::Editing) { return; }

	const ObjectID selectedObjectID = GetSelectedObjectID(selection);
	if (editWorld && selection.SetWorld(editWorld.get()))
	{
		SelectByObjectID(selection, *editWorld, selectedObjectID);
	}
	else
	{
		// 파괴할 Play World를 계속 관찰하지 않도록 분리한다. nullptr 분리는 실패하지 않는다.
		selection.SetWorld(nullptr);
	}

	// 맵 교체와 같이 공용 조명 슬롯을 비운 뒤 파괴한다. Edit World는 다음 그리기에서 슬롯을 다시 연결한다.
	playWorld->GetRenderScene().ReleaseLightData();
	playWorld.reset();
	playState = EditorPlayState::Editing;
}

bool EditorWorldSession::SetPaused(bool paused)
{
	if (playState == EditorPlayState::Editing) { return false; }

	playState = paused ? EditorPlayState::Paused : EditorPlayState::Playing;
	return true;
}

World* EditorWorldSession::GetActiveWorld()
{
	return playWorld ? playWorld.get() : editWorld.get();
}
