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

namespace
{
	// 무작위 ID가 연속으로 겹칠 확률은 사실상 0이다. 그래도 Generate가 같은 값을 계속 돌려주는 고장에서
	// 무한 반복하지 않도록 횟수를 제한한다.
	constexpr int MaxObjectIDGenerateAttempts = 8;
}

World::World() = default;
World::~World()
{
	// 객체 파괴 도중 들어오는 ID 조회가 파괴 중인 객체를 돌려주지 않게 인덱스를 먼저 비운다.
	gameObjectsByID.clear();
}

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
	for (int attempt = 0; attempt < MaxObjectIDGenerateAttempts; ++attempt)
	{
		const ObjectID objectID = ObjectID::Generate();
		if (!objectID.IsValid()) { return nullptr; }

		// 이 World에 이미 있는 ID면 다시 만든다.
		if (gameObjectsByID.contains(objectID)) { continue; }

		return AddGameObject(std::move(newGameObject), objectID);
	}

	// 정해진 횟수 안에 겹치지 않는 ID를 얻지 못했다.
	return nullptr;
}

GameObject* World::AddGameObject(std::unique_ptr<GameObject> newGameObject, const ObjectID& objectID)
{
	if (newGameObject == nullptr) { return nullptr; }
	if (&newGameObject->GetWorld() != this) { return nullptr; }
	if (!objectID.IsValid() || newGameObject->GetObjectID().IsValid()) { return nullptr; }

	// 할당 실패 예외가 목록과 인덱스를 어긋나게 하지 않도록 목록 공간을 인덱스 등록보다 먼저 확보한다.
	if (gameObjects.size() == gameObjects.capacity())
	{
		gameObjects.reserve(std::max<std::size_t>(gameObjects.capacity() * 2, 1));
	}

	GameObject* gameObject = newGameObject.get();
	// 저장이나 복제에서 온 ID가 이미 있으면 데이터 손상이나 중복 등록이므로 다시 발급하지 않고 거절한다.
	if (!gameObjectsByID.emplace(objectID, gameObject).second) { return nullptr; }

	gameObject->SetObjectID(objectID);
	// 공간을 확보했으므로 여기서는 실패하지 않는다.
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
			gameObjectsByID.erase(entry->GetObjectID());
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
	if (gameObject == nullptr) { return false; }

	// 등록된 객체는 자기 ID로 인덱스에 들어 있다. 미등록 객체는 무효 ID라 찾지 못하고,
	// 다른 World의 같은 ID 객체는 주소가 달라 걸러진다.
	const auto found = gameObjectsByID.find(gameObject->GetObjectID());
	return found != gameObjectsByID.end() && found->second == gameObject;
}

GameObject* World::FindGameObject(const ObjectID& objectID)
{
	const auto found = gameObjectsByID.find(objectID);
	if (found == gameObjectsByID.end() || found->second->IsPendingDestroy()) { return nullptr; }
	return found->second;
}

const GameObject* World::FindGameObject(const ObjectID& objectID) const
{
	const auto found = gameObjectsByID.find(objectID);
	if (found == gameObjectsByID.end() || found->second->IsPendingDestroy()) { return nullptr; }
	return found->second;
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
	if (!ContainsGameObject(owner)) { return false; }

	activeCamera = camera;
	return true;
}

void World::Tick(float deltaTime)
{
	for (const std::unique_ptr<GameObject>& gameObject : gameObjects)
	{
		if (gameObject->IsPendingDestroy()) { continue; }

		gameObject->Tick(deltaTime);
	}
}
