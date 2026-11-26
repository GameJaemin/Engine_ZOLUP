#pragma once

#include <filesystem>
#include <memory>

class EditorSelection;
class World;

// Paused는 Play World를 유지한 채 Tick만 멈춘 상태다.
enum class EditorPlayState : unsigned char { Editing, Playing, Paused };

class EditorWorldSession
{
public:
	EditorWorldSession();
	~EditorWorldSession();

	/** 세션이 닫혀 있으면 빈 Edit World로 세션을 시작한다. 이미 열려 있으면 아무 일도 하지 않는다. */
	void NewLevel();

	/**
	 * 절대 경로의 맵을 불러오고 선택을 새 World에 연결한 뒤 Edit World를 교체한다.
	 * Play 중이거나 맵 로드나 선택 연결이 실패하면 기존 Edit World와 선택을 유지하고 false를 반환한다.
	 */
	bool OpenLevel(const std::filesystem::path& levelPath, EditorSelection& selection);

	/** 렌더 ID 목록을 비우고 Play World와 Edit World를 파괴한다. 닫힌 세션에서는 아무 일도 하지 않는다. */
	void Close() noexcept;

	/** 비소유 포인터를 반환한다. 닫힌 세션에서는 nullptr이며, 다음 맵 교체 성공 또는 Close 전까지 유효하다. */
	World* GetEditWorld();
	const World* GetEditWorld() const;

private:
	std::unique_ptr<World> editWorld;

	// Play
public:
	/**
	 * Edit World를 복사해 Play World를 만들고 선택을 같은 ObjectID의 객체로 옮긴다.
	 * 편집 중이 아니거나 복사, 복원 또는 선택 연결에 실패하면 Edit 상태를 유지하고 false를 반환한다.
	 */
	bool StartPlay(EditorSelection& selection);

	/** Play World를 버리고 선택을 Edit World의 같은 ObjectID 객체로 되돌린다. 편집 중이면 아무 일도 하지 않는다. */
	void StopPlay(EditorSelection& selection);

	/** Playing과 Paused 사이만 바꾼다. 편집 중이면 false를 반환한다. */
	bool SetPaused(bool paused);

	EditorPlayState GetPlayState() const { return playState; }

	/** 패널, 선택 및 렌더가 쓸 World다. Play 중에는 Play World, 그 밖에는 Edit World이며 비소유다. */
	World* GetActiveWorld();

private:
	// Play 중에만 있다. Edit World와 ObjectID가 같은 독립 복사본이며 StopPlay 또는 Close에서 파괴한다.
	std::unique_ptr<World> playWorld;
	EditorPlayState playState = EditorPlayState::Editing;
};
