#pragma once

#include <memory>

class World;

class EditorWorldSession
{
public:
	EditorWorldSession();
	~EditorWorldSession();

	/** 세션이 닫혀 있으면 빈 Edit World를 생성한다. */
	void Open();

	/** 렌더 ID 목록을 비우고 Edit World를 파괴한다. 닫힌 세션에서는 아무 일도 하지 않는다. */
	void Close() noexcept;

	/** 비소유 포인터를 반환한다. 세션이 닫혀 있으면 nullptr이며 반환한 포인터는 Close 이전까지만 유효하다. */
	World* GetEditWorld();
	const World* GetEditWorld() const;

private:
	std::unique_ptr<World> editWorld;
};
