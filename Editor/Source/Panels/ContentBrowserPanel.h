#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

/** 실제 에셋 로딩과 무관한 콘텐츠 브라우저 표시용 분류다. */
enum class ContentBrowserEntryType
{
	Folder,
	Model,
	Prefab,
	Map,
	Image,
	Binary,
	File
};

struct ContentBrowserEntry
{
	// 확장자를 포함한 UTF-8 파일명 또는 폴더명.
	std::string name;

	// 저장소 루트 기준 UTF-8 상대 경로. 구분자는 /를 사용한다.
	std::string relativePath;

	ContentBrowserEntryType type = ContentBrowserEntryType::File;
};

/**
 * 한 디렉터리의 직계 항목 조회 결과다.
 * 조회 실패 시 entries는 비어 있고 error에 원인이 저장된다.
 */
struct ContentBrowserDirectoryCache
{
	// ReadDirectory가 반환한 폴더 우선, 이름순 목록.
	std::vector<ContentBrowserEntry> entries;

	// 오류가 없으면 정상 조회 결과이며, 빈 폴더도 정상 결과에 포함된다.
	std::error_code error;
};

class ContentBrowserPanel
{
public:
	/**
	 * 저장소 루트 경로를 구하고 초기 Content 폴더를 조회한다.
	 * 루트 경로 조회에 실패하면 기존 상태를 유지한다.
	 * 루트 경로 조회에 성공하면 기존 탐색 상태와 파일 선택을 초기화한다.
	 * 초기 Content 폴더 조회까지 성공하면 true를 반환한다.
	 * 두 Content 폴더 모두 조회할 수 없으면 현재 폴더와 목록을 비운 상태로 false를 반환한다.
	 */
	bool Initialize();

	/**
	 * 디렉터리 캐시를 비우고 현재 폴더의 직계 항목을 다시 조회한다.
	 * outError는 오류가 없는 상태로 전달한다.
	 * 현재 폴더가 없으면 캐시를 포함한 기존 상태를 유지하고 실패한다.
	 * 조회 실패 시 현재 경로와 목록은 유지하지만, 제거한 캐시는 복원하지 않는다.
	 * 현재 폴더가 있으면 조회 성공 여부와 관계없이 선택을 검증하며, 무효한 선택은 해제한다.
	 * 반환값과 outError는 폴더 재조회 결과를 나타낸다.
	 * 방문 기록과 현재 기록 위치는 변경하지 않는다.
	 * 호출 전 캐시에서 받은 참조는 새로고침 이후 사용하지 않는다.
	 */
	bool Refresh(std::error_code& outError);

	/**
	 * 선택 파일을 검증한 뒤 저장소 기준 UTF-8 상대 경로를 반환한다.
	 * 선택이 없거나 검증에 실패하면 빈 문자열을 반환한다.
	 * 반환값은 복사본이므로 이후 패널의 선택 변경에 영향을 받지 않는다.
	 */
	std::string GetSelectedFilePath();

	/**
	 * 유효한 ImGui Context에서 NewFrame 이후, Render 전에 호출한다.
	 * 파일 탐색 초기화는 호출자가 Initialize로 수행한다.
	 * 창 표시 전에 선택을 검증해 삭제되거나 접근할 수 없는 파일의 선택을 해제한다.
	 * 맵 더블클릭으로 파일 선택에 성공하면 맵 열기 요청을 기록한다.
	 */
	void Draw();

	/** 저장소 기준 UTF-8 상대 경로를 반환하고 요청을 비운다. 요청이 없으면 std::nullopt다. */
	std::optional<std::string> TakeLevelOpenRequest();

	/** 렌더러가 살아 있고 이 패널을 더 그리지 않는 시점에 썸네일 해제를 요청한다. */
	void ShutdownThumbnails();

public:
	// ImGui 창 이름이다. 도킹 기본 배치도 이 이름으로 창을 찾으므로 같은 상수를 쓴다.
	static constexpr const char* WindowName = "Content Browser";

private:
	struct Thumbnail
	{
		// Hakko는 자원 해제 시 텍스처 ID를 재배치할 수 있으므로 이름을 보관한다.
		std::wstring textureName;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
	};

	/** 현재 폴더의 이미지 항목을 조회하고, 캐시에 없으면 프레임 한도 안에서 준비한다. */
	const Thumbnail* FindOrPrepareThumbnail(const ContentBrowserEntry& entry);
	void PrepareThumbnail(const ContentBrowserEntry& entry, Thumbnail& thumbnail) const;
	void ClearThumbnails();

	/** 현재 목록 항목의 실제 파일 경로를 검사한다. 성공 시에만 outFile을 갱신한다. */
	bool ResolveEntryFile(const ContentBrowserEntry& entry, std::filesystem::path& outFile, std::error_code& outError) const;

	/**
	 * 이동 버튼, Breadcrumb, 검색과 새로고침을 한 줄에 배치한다.
	 * 유효한 ImGui 프레임과 패널 창 내부에서 호출한다.
	 * 전체 너비가 부족하면 툴바 내부에서 가로 스크롤한다.
	 * 여러 작업이 실패하면 표시 순서상 첫 오류를 반환한다.
	 */
	std::error_code DrawToolbar();

	/**
	 * 뒤로, 앞으로, 상위 폴더 이동 버튼을 같은 줄에 그린다.
	 * 유효한 ImGui 프레임과 패널 창 내부에서 호출한다.
	 * 이동 실패 시 표시할 수 있도록 오류를 반환하며, 그 외에는 오류 없는 값을 반환한다.
	 */
	std::error_code DrawNavigationButtons();

	/**
	 * 현재 폴더를 새로고침하는 버튼을 그린다.
	 * 유효한 ImGui 프레임과 패널 창 내부에서 호출한다.
	 * 현재 폴더가 없으면 버튼을 비활성화한다.
	 * 새로고침 실패 시 오류를 반환하며, 그 외에는 오류 없는 값을 반환한다.
	 */
	std::error_code DrawRefreshButton();

	/**
	 * 현재 폴더의 경로를 구간별로 표시한다.
	 * 유효한 ImGui 프레임과 패널 창 내부에서 호출한다.
	 * Content부터 각 구간을 클릭해 이동할 수 있으며, 성공한 이동은 기록한다.
	 * 이동 실패 시 오류를 반환하며, 그 외에는 오류 없는 값을 반환한다.
	 */
	std::error_code DrawBreadcrumb();

	/**
	 * 파일명 검색어 입력창을 그린다.
	 * 유효한 ImGui 프레임과 패널 창 내부에서 호출한다.
	 * 입력 내용은 searchText에 UTF-8로 보관하며, 목록 필터링은 별도로 처리한다.
	 */
	void DrawSearchInput();

	/**
	 * 항목 이름에 현재 검색어가 포함되면 true를 반환한다.
	 * 검색어가 비어 있으면 모든 항목을 허용한다.
	 * ASCII 영문은 대소문자를 무시하고, 그 외 UTF-8 바이트는 그대로 비교한다.
	 * 파일과 폴더의 이름만 검사하며 경로나 파일 내용은 검색하지 않는다.
	 */
	bool MatchesSearch(const ContentBrowserEntry& entry) const;

	/**
	 * 콘텐츠 항목 하나를 지정한 너비의 타일로 그린다. tileWidth는 여백 합계인 12보다 커야 한다.
	 * 유효한 ImGui 프레임과 콘텐츠 영역 내부에서 호출한다.
	 * 파일 클릭 시 선택하고, 폴더 더블클릭 시 이동 요청 경로를 출력한다.
	 * 맵 더블클릭으로 파일 선택에 성공하면 맵 열기 요청을 기록한다.
	 * 호출자는 전체 항목 순회를 마친 뒤 요청한 폴더로 이동한다.
	 */
	std::error_code DrawEntryTile(const ContentBrowserEntry& entry, float tileWidth, std::string& outRequestedDirectory);

	/**
	 * 현재 폴더의 직계 항목을 검색어로 필터링해 타일 그리드로 표시한다.
	 * 유효한 ImGui 프레임과 콘텐츠 영역 내부에서 호출한다.
	 * 영역 너비에 맞춰 열 개수를 계산하며, 폴더 이동은 순회 종료 후 수행한다.
	 * 파일 선택 또는 폴더 이동 실패 시 오류를 반환한다.
	 */
	std::error_code DrawContentGrid();

	/**
	 * 폴더 하나와 펼쳐진 하위 폴더를 트리로 표시한다.
	 * 유효한 ImGui 프레임과 폴더 영역 내부에서 호출한다.
	 * relativePath는 저장소 기준 UTF-8 상대 경로이며, displayName은 표시 이름이다.
	 * 펼쳐진 폴더만 캐시 조회하고, 파일은 트리에 표시하지 않는다.
	 * 이름 클릭 시 이동 요청 경로를 출력하며, 실제 이동은 전체 트리 순회 후 처리한다.
	 */
	void DrawFolderNode(std::string_view relativePath, std::string_view displayName, std::string& outRequestedDirectory);

	/**
	 * Game과 Engine의 Content 루트를 폴더 트리로 표시한다.
	 * 유효한 ImGui 프레임과 폴더 영역 내부에서 호출한다.
	 * 두 루트의 트리 순회가 끝난 뒤 요청한 폴더로 이동한다.
	 * 이동 실패 시 오류를 반환하며, 그 외에는 오류 없는 값을 반환한다.
	 */
	std::error_code DrawFolderTree();

	/**
	 * 저장소 기준 UTF-8 상대 경로를 검사해 실제 디렉터리 경로를 구한다.
	 * 저장소 루트 경로를 준비한 뒤 호출하며, outError는 오류가 없는 상태로 전달한다.
	 * 두 Content 루트 밖의 경로와 reparse point를 거치는 경로는 허용하지 않는다.
	 * 성공 시 outDirectory를 갱신하고, 실패 시 출력 경로를 유지하며 오류를 기록한다.
	 */
	bool ResolveDirectory(std::string_view relativePath, std::filesystem::path& outDirectory, std::error_code& outError) const;

	/**
	 * 저장소 기준 UTF-8 상대 경로의 직계 파일과 폴더를 조회한다.
	 * outError는 오류가 없는 상태로 전달한다.
	 * reparse point와 일반 파일 및 디렉터리가 아닌 항목은 제외한다.
	 * 성공 시 정렬된 목록으로 outEntries를 교체하고, 실패 시 기존 목록을 유지한다.
	 */
	bool ReadDirectory(std::string_view relativePath, std::vector<ContentBrowserEntry>& outEntries, std::error_code& outError) const;

	/**
	 * 저장소 기준 UTF-8 상대 경로의 디렉터리 조회 결과를 반환한다.
	 * 캐시가 없을 때만 조회하며, 실패 결과도 캐시가 제거될 때까지 보관한다.
	 * 반환한 참조는 패널이 소유하며, 해당 캐시 삭제 또는 패널 파괴 후에는 사용할 수 없다.
	 */
	const ContentBrowserDirectoryCache& GetDirectoryEntries(std::string_view relativePath);

	/**
	 * 저장소 기준 UTF-8 상대 경로의 폴더로 이동한다.
	 * outError는 오류가 없는 상태로 전달한다.
	 * 조회 성공 시 현재 폴더와 목록을 교체한다.
	 * 조회 성공 시 해당 폴더의 디렉터리 캐시도 갱신한다.
	 * recordHistory가 true이면 성공한 이동을 방문 기록에 추가한다.
	 * 다른 폴더로 이동한 경우 파일 선택을 해제한다.
	 * 같은 폴더 재조회 시 선택을 유지하며, 선택 파일의 유효성은 별도로 검사한다.
	 * 조회 실패 시 현재 폴더, 목록, 방문 기록, 기존 캐시와 파일 선택을 유지한다.
	 */
	bool NavigateTo(std::string_view relativePath, std::error_code& outError, bool recordHistory = false);

	/**
	 * 저장소 루트 경로를 준비한 뒤 호출한다.
	 * Game/Content를 우선 열고, 실패하면 Engine/Content를 시도한다.
	 * 조회에 성공한 초기 폴더를 방문 기록에 추가한다.
	 * outError는 오류가 없는 상태로 전달한다.
	 * 두 조회가 모두 실패하면 현재 상태를 유지하고 Engine/Content의 오류를 전달한다.
	 */
	bool OpenInitialDirectory(std::error_code& outError);

	/**
	 * 현재 폴더의 상위 폴더로 이동한다.
	 * outError는 오류가 없는 상태로 전달한다.
	 * Content 루트에서는 상위 이동을 허용하지 않는다.
	 * 이동 성공 시 방문 기록에 추가하고, 실패 시 현재 상태와 기록을 유지한다.
	 */
	bool GoUp(std::error_code& outError);

	/**
	 * 현재 폴더의 부모가 허용된 Content 경로이면 true를 반환한다.
	 * 현재 폴더가 없거나 Content 루트이면 false를 반환한다.
	 * 디스크를 조회하거나 패널 상태를 변경하지 않는다.
	 */
	bool CanGoUp() const;

	/**
	 * 현재 방문 기록보다 앞선 기록이 있으면 true를 반환한다.
	 * 방문 기록이나 현재 폴더는 변경하지 않는다.
	 */
	bool CanGoBack() const;

	/**
	 * 이전 방문 기록의 폴더로 이동한다.
	 * outError는 오류가 없는 상태로 전달한다.
	 * 이전 기록이 없거나 폴더 조회에 실패하면 현재 상태와 기록 위치를 유지한다.
	 * 이동 성공 시 기록 위치만 변경하며 방문 기록은 추가하지 않는다.
	 */
	bool GoBack(std::error_code& outError);

	/**
	 * 다음 방문 기록의 폴더로 이동한다.
	 * outError는 오류가 없는 상태로 전달한다.
	 * 다음 기록이 없거나 폴더 조회에 실패하면 현재 상태와 기록 위치를 유지한다.
	 * 이동 성공 시 기록 위치만 변경하며 방문 기록은 추가하지 않는다.
	 */
	bool GoForward(std::error_code& outError);

	/**
	 * 현재 방문 기록보다 뒤의 기록이 있으면 true를 반환한다.
	 * 방문 기록이나 현재 폴더는 변경하지 않는다.
	 */
	bool CanGoForward() const;

	/**
	 * 성공한 일반 폴더 이동 후 현재 폴더를 방문 기록에 추가한다.
	 * 현재 폴더가 없거나 현재 기록과 같은 경로이면 추가하지 않는다.
	 * 새 기록을 추가할 때 기존 앞으로 가기 기록은 제거한다.
	 * history가 비어 있지 않으면 historyIndex는 유효한 인덱스여야 한다.
	 */
	void RecordCurrentDirectory();

	/**
	 * 현재 목록에 있는 파일을 선택한다. 폴더는 선택하지 않는다.
	 * relativePath는 저장소 기준 UTF-8 상대 경로다.
	 * outError는 오류가 없는 상태로 전달한다.
	 * 실제 파일과 부모 경로를 검사하며 reparse point는 허용하지 않는다.
	 * 성공 시 선택 경로를 저장하고, 실패 시 기존 선택을 유지한다.
	 */
	bool SelectFile(std::string_view relativePath, std::error_code& outError);

	/**
	 * 선택한 파일이 현재 목록에 있고 실제로 접근 가능한 일반 파일인지 확인한다.
	 * 삭제, 이동, 접근 오류 또는 reparse point 검사 실패 시 선택을 해제한다.
	 * 선택이 없으면 아무 작업도 하지 않는다.
	 */
	void ValidateSelection();

private:
	// Paths::GetRootDirectory로 구할 저장소의 절대 경로.
	std::filesystem::path rootDirectory;

	// 저장소 루트 기준 UTF-8 상대 경로. 비어 있으면 현재 폴더가 없다.
	std::string currentDirectory;

	// 현재 폴더의 직계 항목만 소유한다. 하위 폴더 내부는 포함하지 않는다.
	std::vector<ContentBrowserEntry> currentEntries;

	// 다른 폴더로 이동한 뒤 콘텐츠 영역을 그릴 때 세로 스크롤을 초기화한다.
	bool resetContentScroll = false;

	// 키는 Paths::MakeAssetKey로 만든 저장소 기준 상대 경로다.
	// 캐시에 없으면 미조회 상태이며, 있으면 성공 또는 실패 결과를 보관한다.
	std::unordered_map<std::string, ContentBrowserDirectoryCache> directoryCache;

	// 빈 텍스처 이름은 파일 읽기 또는 등록 실패를 뜻하며, 새로고침 시 다시 시도한다.
	std::unordered_map<std::string, Thumbnail> thumbnails;
	// 이전 해제 요청이 처리되기 전에도 같은 파일을 다른 이름으로 등록할 수 있다.
	std::uint64_t thumbnailGeneration = 0;
	int remainingThumbnailLoads = 0;
	// 타일을 이미 그린 프레임에는 텍스처 해제를 요청하지 않는다.
	bool shouldClearThumbnails = false;

	// 성공한 폴더 이동의 저장소 기준 UTF-8 상대 경로를 순서대로 보관한다.
	std::vector<std::string> history;

	// history가 비어 있으면 0이며, 비어 있지 않으면 현재 기록의 유효한 인덱스다.
	std::size_t historyIndex = 0;

	// 선택한 파일의 저장소 기준 UTF-8 상대 경로. 비어 있으면 선택이 없다.
	std::string selectedFilePath;

	// 맵 열기 요청이 있으면 저장소 기준 UTF-8 상대 경로를 보관한다.
	std::optional<std::string> requestedLevelPath;

	// 널 종료 문자를 포함한 UTF-8 검색어 버퍼. 초기 상태는 빈 문자열이다.
	std::array<char, 256> searchText{};
};
