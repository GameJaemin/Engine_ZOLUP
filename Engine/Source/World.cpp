#include "World.h"

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

World::World() = default;
World::~World() = default;

InputDispatcher& World::GetInputDispatcher()
{
	return inputDispatcher;
}

RenderScene& World::GetRenderScene()
{
	return renderScene;
}

GameObject* World::AddGameObject(std::unique_ptr<GameObject> newGameObject)
{
	if (newGameObject == nullptr) { return nullptr; }
	if (&newGameObject->GetWorld() != this) { return nullptr; }

	GameObject* gameObject = newGameObject.get();
	gameObjects.push_back(std::move(newGameObject));
	return gameObject;
}

GameObject* World::CreateGameObject(const std::optional<std::string>& name, const Transform& transform)
{
	std::unique_ptr<GameObject> object = std::make_unique<GameObject>(*this);
	if (name.has_value())
	{
		object->SetName(*name);
	}
	object->GetComponent<TransformComponent>()->SetTransform(transform);
	// World가 객체를 소유하고 호출부에는 빌린 포인터를 돌려준다.
	return AddGameObject(std::move(object));
}

bool World::DestroyGameObject(GameObject* gameObject)
{
	if (isDestroyingPendingGameObjects || !ContainsGameObject(gameObject)) { return false; }
	if (gameObject->IsPendingDestroy()) { return true; }

	std::vector<GameObject*> targets{ gameObject };
	for (std::size_t index = 0; index < targets.size(); ++index)
	{
		for (GameObject* child : targets[index]->GetChildren())
		{
			if (!child->IsPendingDestroy())
			{
				targets.push_back(child);
			}
		}
	}

	// 모든 공간을 확보한 뒤 상태를 바꿔 요청 일부만 접수되는 일을 막는다.
	pendingDestroyGameObjects.reserve(pendingDestroyGameObjects.size() + targets.size());
	for (GameObject* target : targets)
	{
		target->SetPendingDestroy(true);
		pendingDestroyGameObjects.push_back(target);
	}

	return true;
}

void World::DestroyPendingGameObjects()
{
	if (isDestroyingPendingGameObjects || pendingDestroyGameObjects.empty()) { return; }

	std::vector<std::unique_ptr<GameObject>> removedObjects;
	removedObjects.reserve(pendingDestroyGameObjects.size());

	isDestroyingPendingGameObjects = true;
	NotifyGameObjectsDestroying();

	for (GameObject* target : pendingDestroyGameObjects)
	{
		target->OnDestroying();
	}

	for (GameObject* target : pendingDestroyGameObjects)
	{
		target->SetParent(nullptr);
	}

	// 소멸자가 실행되기 전에 계층과 등록 목록을 정리한다.
	for (std::unique_ptr<GameObject>& entry : gameObjects)
	{
		if (std::ranges::find(pendingDestroyGameObjects, entry.get()) != pendingDestroyGameObjects.end())
		{
			removedObjects.push_back(std::move(entry));
		}
	}
	std::erase(gameObjects, nullptr);
	pendingDestroyGameObjects.clear();
	removedObjects.clear();

	const auto firstUnsubscribed = std::ranges::remove(destroyingSubscriptions, std::uint64_t{ 0 }, &GameObjectDestroyingSubscription::id).begin();
	destroyingSubscriptions.erase(firstUnsubscribed, destroyingSubscriptions.end());
	isDestroyingPendingGameObjects = false;
}

std::uint64_t World::SubscribeGameObjectDestroying(std::function<void(const GameObject&)> callback)
{
	if (isDestroyingPendingGameObjects || !callback || nextDestroyingSubscriptionId == 0) { return 0; }

	const std::uint64_t subscriptionId = nextDestroyingSubscriptionId;
	destroyingSubscriptions.push_back({ subscriptionId, std::move(callback) });
	++nextDestroyingSubscriptionId;
	return subscriptionId;
}

void World::UnsubscribeGameObjectDestroying(std::uint64_t subscriptionId)
{
	if (subscriptionId == 0) { return; }

	const auto subscription = std::ranges::find(destroyingSubscriptions, subscriptionId, &GameObjectDestroyingSubscription::id);
	if (subscription == destroyingSubscriptions.end()) { return; }

	if (isDestroyingPendingGameObjects)
	{
		// 실행 중인 콜백과 반복자가 유효하도록 순회가 끝날 때까지 자리만 유지한다.
		subscription->id = 0;
		return;
	}

	destroyingSubscriptions.erase(subscription);
}

bool World::ContainsGameObject(const GameObject* gameObject) const
{
	for (const std::unique_ptr<GameObject>& entry : gameObjects)
	{
		if (entry.get() == gameObject) { return true; }
	}

	return false;
}

void World::NotifyGameObjectsDestroying() noexcept
{
	for (const GameObject* target : pendingDestroyGameObjects)
	{
		for (const GameObjectDestroyingSubscription& subscription : destroyingSubscriptions)
		{
			if (subscription.id == 0) { continue; }

			subscription.callback(*target);
		}
	}
}

bool World::SetActiveCamera(CameraComponent* camera)
{
	if (camera == nullptr)
	{
		activeCamera = nullptr;
		return true;
	}

	const GameObject* owner = camera->GetGameObject();
	if (owner == nullptr) { return false; }
	if (&owner->GetWorld() != this) { return false; }

	// 월드가 소유한 객체인지 확인한다. 등록 전 객체는 월드보다 먼저 사라질 수 있다.
	for (const std::unique_ptr<GameObject>& gameObject : gameObjects)
	{
		if (gameObject.get() == owner)
		{
			activeCamera = camera;
			return true;
		}
	}
	return false;
}

void World::Tick(float deltaTime)
{
	for (const std::unique_ptr<GameObject>& gameObject : gameObjects)
	{
		if (gameObject->IsPendingDestroy()) { continue; }

		gameObject->Tick(deltaTime);
	}
}
