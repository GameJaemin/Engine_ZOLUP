#pragma once

#include <cstdint>

class GameObject;
class World;

/** 선택과 관찰은 비소유다. 관찰 중인 World를 먼저 파괴하려면 SetWorld(nullptr)로 분리한다. */
class EditorSelection
{
public:
	EditorSelection() = default;
	~EditorSelection();

	EditorSelection(const EditorSelection&) = delete;
	EditorSelection& operator=(const EditorSelection&) = delete;
	EditorSelection(EditorSelection&&) = delete;
	EditorSelection& operator=(EditorSelection&&) = delete;

	/**
	 * World를 바꾸면 기존 선택을 비우고 구독을 해제한다. 같은 World면 기존 선택을 유지한다.
	 * nullptr로 분리할 수 있다. 새 구독이 거절되면 분리된 상태로 false를 반환한다.
	 */
	bool SetWorld(World* newWorld);

	/** 관찰 중인 World에 등록된 객체를 선택한다. 미등록 또는 삭제 예정이면 기존 선택을 유지한다. */
	bool Select(GameObject* gameObject);

	void ClearSelection() noexcept;

	GameObject* GetSelectedGameObject() { return selectedGameObject; }
	const GameObject* GetSelectedGameObject() const { return selectedGameObject; }

private:
	World* observedWorld = nullptr;
	GameObject* selectedGameObject = nullptr;
	std::uint64_t destroyingSubscriptionId = 0;
};
