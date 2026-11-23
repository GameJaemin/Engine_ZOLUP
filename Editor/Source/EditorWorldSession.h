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
	 * 닫힌 세션에서 호출한다. levelPath(절대 경로)의 맵을 불러온 Edit World로 세션을 시작한다.
	 * 불러오기에 실패하면 세션을 열지 않고 false를 반환한다. 이미 열려 있으면 아무 일도 하지 않고 false다.
	 */
	bool OpenLevel(const std::filesystem::path& levelPath);

	/**
	 * 열린 세션에서 렌더 요청을 준비하기 전에 호출한다. DataHub가 초기화된 상태여야 한다.
	 * levelPath(절대 경로)를 새 World에 읽고 selection을 연결한 뒤 Edit World를 교체한다.
	 * 로드 또는 구독에 실패하면 기존 World와 선택을 유지하고 false를 반환한다.
	 */
	bool ReplaceLevel(const std::filesystem::path& levelPath, EditorSelection& selection);

	/** 렌더 ID 목록을 비우고 Edit World를 파괴한다. 닫힌 세션에서는 아무 일도 하지 않는다. */
	void Close() noexcept;

	/** 비소유 포인터를 반환한다. 세션이 닫혀 있으면 nullptr이며 포인터는 교체 또는 Close 이전까지만 유효하다. */
	World* GetEditWorld();
	const World* GetEditWorld() const;

private:
	std::unique_ptr<World> editWorld;
};
