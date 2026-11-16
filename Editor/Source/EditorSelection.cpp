#include "EditorSelection.h"

#include "GameObject.h"
#include "World.h"

EditorSelection::~EditorSelection()
{
	SetWorld(nullptr);
}

bool EditorSelection::SetWorld(World* newWorld)
{
	if (newWorld == observedWorld) { return true; }

	ClearSelection();
	if (observedWorld != nullptr)
	{
		observedWorld->UnsubscribeGameObjectDestroying(destroyingSubscriptionId);
		observedWorld = nullptr;
		destroyingSubscriptionId = 0;
	}

	if (newWorld == nullptr) { return true; }

	const std::uint64_t newSubscriptionId = newWorld->SubscribeGameObjectDestroying([this](const GameObject& gameObject) noexcept
	{
		if (&gameObject == selectedGameObject)
		{
			ClearSelection();
		}
	});
	if (newSubscriptionId == 0) { return false; }

	observedWorld = newWorld;
	destroyingSubscriptionId = newSubscriptionId;
	return true;
}

bool EditorSelection::Select(GameObject* gameObject)
{
	if (observedWorld == nullptr || !observedWorld->ContainsGameObject(gameObject)) { return false; }
	if (gameObject->IsPendingDestroy()) { return false; }

	selectedGameObject = gameObject;
	return true;
}

void EditorSelection::ClearSelection() noexcept
{
	selectedGameObject = nullptr;
}
