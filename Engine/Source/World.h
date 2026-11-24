#pragma once

#include "Input/InputDispatcher.h"
#include "ObjectID.h"
#include "Renderer/RenderScene.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class CameraComponent;
class GameObject;
struct Prefab;
struct PrefabPlacement;
struct Transform;

struct GameObjectDestroyingSubscription
{
	std::uint64_t id;
	std::function<void(const GameObject&)> callback;
};

/** 메인 스레드에서 사용하며 등록한 게임 객체를 단독 소유한다. */
class World
{
	// 생성과 소멸
public:
	World();
	~World();

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;

	// 입력
public:
	/** 이 월드의 입력 전달자를 빌려준다. 월드 수명 동안 유효하다. */
	InputDispatcher& GetInputDispatcher();

private:
	// gameObjects보다 먼저 선언해 더 늦게 파괴한다. 소멸하는 InputComponent가 전달자에서 자신을 제거할 수 있게 하기 위해서다.
	InputDispatcher inputDispatcher;

	// 렌더
public:
	/** 이 월드의 렌더 씬을 빌려준다. 월드 수명 동안 유효하다. */
	RenderScene& GetRenderScene();

private:
	// gameObjects보다 먼저 선언해 더 늦게 파괴한다. 이후 소멸하는 렌더 컴포넌트가 자신을 해제할 수 있게 하기 위해서다.
	RenderScene renderScene;

	// 카메라
public:
	/**
	 * 이 월드에 등록된 객체에 붙은 카메라를 활성으로 지정한다. nullptr는 해제다.
	 * 부착 전이거나 다른 월드, 미등록 객체의 카메라면 기존 값을 유지하고 false를 반환한다.
	 */
	bool SetActiveCamera(CameraComponent* camera);

	/** 활성 카메라를 빌려준다. 지정하지 않았거나 지정한 카메라가 소멸했으면 nullptr이다. */
	CameraComponent* GetActiveCamera() { return activeCamera; }
	const CameraComponent* GetActiveCamera() const { return activeCamera; }

private:
	// 비소유. gameObjects보다 먼저 선언해 더 늦게 파괴한다. 소멸하는 CameraComponent가 지정을 해제할 수 있게 하기 위해서다.
	CameraComponent* activeCamera = nullptr;

	// 객체 관리
public:
	/**
	 * 새 ObjectID를 발급해 등록하고 해당 객체가 삭제되기 전까지 유효한 비소유 포인터를 반환한다.
	 * 발급한 ID가 이 World의 기존 ID와 겹치면 다시 발급한다.
	 * 빈 입력, 소속 불일치, 이미 ID를 가진 객체, ID 발급 실패와 재발급 횟수 초과는 nullptr를 반환하고 입력 객체는 정리한다.
	 * 갱신 중에는 호출하지 않는다. 할당 실패 예외는 전파한다.
	 */
	GameObject* AddGameObject(std::unique_ptr<GameObject> newGameObject);

	/**
	 * 저장이나 복제에서 가져온 ID를 유지해 등록한다. 그 밖의 계약은 ID를 받지 않는 AddGameObject와 같다.
	 * 무효 ID, 이 World에 이미 있는 ID, 이미 ID를 가진 객체는 nullptr를 반환하고 입력 객체는 정리한다.
	 * 이미 있는 ID는 다시 발급하지 않고 거절한다.
	 * 삭제 예정 객체의 ID도 DestroyPendingGameObjects로 실제 삭제되기 전까지는 이미 있는 ID로 보고 거절한다.
	 */
	GameObject* AddGameObject(std::unique_ptr<GameObject> newGameObject, const ObjectID& objectID);

	/** 기본 GameObject에 이름과 Transform을 적용한 뒤 등록한다. name을 생략하면 객체 기본 이름을 유지한다. */
	[[nodiscard]] GameObject* CreateGameObject(const std::optional<std::string>& name, const Transform& transform);

	/**
	 * 로드된 프리팹을 등록하고 삭제 전까지 유효한 비소유 루트 포인터를 반환한다.
	 * placement에서 지정한 루트 값만 덮어쓴다. 월드 갱신 중에는 호출하지 않는다.
	 * 객체 생성에 실패하면 이번에 만든 객체를 모두 삭제 예정으로 표시하고 nullptr를 반환한다.
	 */
	[[nodiscard]] GameObject* InstantiatePrefab(const Prefab& prefab, const PrefabPlacement& placement);

	/**
	 * 등록된 객체와 그 자손을 삭제 예정으로 표시하고 대기 목록에 넣는다.
	 * 접수했거나 이미 대기 중이면 true를 반환한다.
	 * nullptr, 미등록 또는 삭제 처리 중이면 false를 반환한다.
	 */
	bool DestroyGameObject(GameObject* gameObject);

	/** 대기 객체에 알리고 등록 해제 후 파괴한다. 객체 목록이나 입력 바인딩 순회 중에는 호출하지 않는다. */
	void DestroyPendingGameObjects();

	/**
	 * World 안에서 유효한 구독 번호를 반환한다. 빈 콜백, 삭제 처리 중 또는 번호 소진 시 0을 반환한다.
	 * 계층을 정리하기 전에 대기 목록 순서대로 각 객체를 알린다. 등록된 구독자는 등록 순서대로 호출한다.
	 * 콜백은 참조를 보관하거나 예외를 밖으로 내보내지 않는다. World 자체의 소멸은 알리지 않는다.
	 */
	std::uint64_t SubscribeGameObjectDestroying(std::function<void(const GameObject&)> callback);

	/** 해제는 알림 중에도 즉시 적용한다. 0이나 이미 해제된 번호는 무시한다. */
	void UnsubscribeGameObjectDestroying(std::uint64_t subscriptionId);

	/**
	 * 이 World에 등록된 객체인지 확인한다. nullptr는 false다.
	 * nullptr이거나 살아 있는 객체만 받는다. 이미 파괴된 객체의 주소는 넘기지 않는다.
	 */
	bool ContainsGameObject(const GameObject* gameObject) const;

	/**
	 * 등록된 객체를 ID로 빌려준다. 없거나 삭제 예정이면 nullptr이다.
	 * 삭제 예정 객체는 찾지 못하지만 그 ID는 실제 삭제 전까지 복원 등록에 쓸 수 없다.
	 */
	GameObject* FindGameObject(const ObjectID& objectID);
	const GameObject* FindGameObject(const ObjectID& objectID) const;

	/**
	 * 삭제 예정 객체를 포함한 소유 목록을 등록 순서대로 빌려준다.
	 * 컨테이너 자체는 World 수명 동안 유효하다.
	 * 객체 추가나 실제 삭제 시 원소 참조와 반복자가 무효화될 수 있다.
	 * 객체 포인터는 해당 객체의 실제 파괴 전까지만 유효하다.
	 * const는 컨테이너에 적용되며 내부 GameObject는 수정할 수 있다.
	 * 순회 중 삭제 요청은 등록할 수 있지만 객체 등록이나 실제 삭제를 수행하지 않는다.
	 */
	const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const noexcept { return gameObjects; }

private:
	void NotifyGameObjectsDestroying() noexcept;

private:
	// World가 소유 중인 삭제 대상의 주소를 빌린다. 실제 파괴 전에 목록을 비운다.
	std::vector<GameObject*> pendingDestroyGameObjects;
	std::vector<GameObjectDestroyingSubscription> destroyingSubscriptions;
	std::uint64_t nextDestroyingSubscriptionId = 1;
	bool isDestroyingPendingGameObjects = false;
	// gameObjects가 소유한 객체를 ID로 찾는 비소유 인덱스다. 삭제 예정 객체도 실제 파괴 직전까지 남아 있다.
	std::unordered_map<ObjectID, GameObject*> gameObjectsByID;
	std::vector<std::unique_ptr<GameObject>> gameObjects;

	// 갱신
public:
	/**
	 * 삭제 예정 객체는 건너뛰고 등록 순서대로 갱신한다. deltaTime는 초 단위다.
	 * 콜백에서 객체 등록과 같은 월드의 Tick 재호출을 하지 않는다.
	 */
	void Tick(float deltaTime);
};
