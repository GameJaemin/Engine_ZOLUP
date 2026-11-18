#pragma once

#include <filesystem>
#include <memory>

class World;

class EditorWorldSession
{
public:
	EditorWorldSession();
	~EditorWorldSession();

	/** 세션이 닫혀 있으면 빈 Edit World로 세션을 시작한다. 이미 열려 있으면 아무 일도 하지 않는다. */
	void NewLevel();

	/**
	 * 닫힌 세션에서 호출한다. levelPath(절대 경로)의 맵을 불러온 Edit World로 세션을 시작한다.
	 * 불러오기에 실패하면 세션을 열지 않고 false를 반환한다. 이미 열려 있으면 아무 일도 하지 않고 false다.
	 */
	bool OpenLevel(const std::filesystem::path& levelPath);

	/** 렌더 ID 목록을 비우고 Edit World를 파괴한다. 닫힌 세션에서는 아무 일도 하지 않는다. */
	void Close() noexcept;

	/** 비소유 포인터를 반환한다. 세션이 닫혀 있으면 nullptr이며 반환한 포인터는 Close 이전까지만 유효하다. */
	World* GetEditWorld();
	const World* GetEditWorld() const;

private:
	std::unique_ptr<World> editWorld;
};
