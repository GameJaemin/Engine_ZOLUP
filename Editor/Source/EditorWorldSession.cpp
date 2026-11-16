#include "EditorWorldSession.h"

#include "World.h"

EditorWorldSession::EditorWorldSession() = default;

EditorWorldSession::~EditorWorldSession()
{
	Close();
}

void EditorWorldSession::Open()
{
	if (editWorld) { return; }

	editWorld = std::make_unique<World>();
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
