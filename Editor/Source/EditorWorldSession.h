#pragma once

#include <filesystem>
#include <memory>

class EditorSelection;
class World;

class EditorWorldSession
{
public:
	EditorWorldSession();
	~EditorWorldSession();

	/** 세션이 닫혀 있으면 빈 Edit World로 세션을 시작한다. 이미 열려 있으면 아무 일도 하지 않는다. */
	void NewLevel();

	/**
	 * 절대 경로의 맵을 불러오고 선택을 새 World에 연결한 뒤 Edit World를 교체한다.
	 * 맵 로드나 선택 연결이 실패하면 기존 Edit World와 선택을 유지하고 false를 반환한다.
	 */
	bool OpenLevel(const std::filesystem::path& levelPath, EditorSelection& selection);

	/** 렌더 ID 목록을 비우고 Edit World를 파괴한다. 닫힌 세션에서는 아무 일도 하지 않는다. */
	void Close() noexcept;

	/** 비소유 포인터를 반환한다. 닫힌 세션에서는 nullptr이며, 다음 맵 교체 성공 또는 Close 전까지 유효하다. */
	World* GetEditWorld();
	const World* GetEditWorld() const;

private:
	std::unique_ptr<World> editWorld;
};
