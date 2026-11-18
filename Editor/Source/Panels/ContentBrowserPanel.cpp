#include "ContentBrowserPanel.h"

#include "Paths.h"

#include <imgui.h>
#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
	/** 호출자는 빈 경로와 절대 경로를 거부하고 lexically_normal로 정규화한 상대 경로를 전달한다. */
	bool IsContentRelativePath(const std::filesystem::path& relativePath)
	{
		auto part = relativePath.begin();

		const bool isGame = CompareStringOrdinal(part->c_str(), -1, L"Game", -1, TRUE) == CSTR_EQUAL;
		const bool isEngine = CompareStringOrdinal(part->c_str(), -1, L"Engine", -1, TRUE) == CSTR_EQUAL;

		if (!isGame && !isEngine) { return false; }

		++part;
		if (part == relativePath.end()) { return false; }

		const bool isContent = CompareStringOrdinal(part->c_str(), -1, L"Content", -1, TRUE) == CSTR_EQUAL;
		if (!isContent) { return false; }

		return true;
	}

	/**
	 * root는 실제 경로로 정규화한 저장소 루트다.
	 * relativePath는 정규화와 Content 상대 경로 검사를 통과한 경로다.
	 * outError는 오류가 없는 상태로 전달한다. 실패 시 오류를 기록한다.
	 */
	bool CheckNoReparsePoints(const std::filesystem::path& root, const std::filesystem::path& relativePath, std::error_code& outError)
	{
		std::filesystem::path currentPath = root;

		for (const std::filesystem::path& part : relativePath)
		{
			currentPath /= part;

			const DWORD attributes = GetFileAttributesW(currentPath.c_str());

			if (attributes == INVALID_FILE_ATTRIBUTES)
			{
				const DWORD error = GetLastError();
				outError = std::error_code(static_cast<int>(error), std::system_category());
				return false;
			}

			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			{
				outError = std::make_error_code(std::errc::permission_denied);
				return false;
			}
		}

		return true;
	}

	/**
	 * root와 directory는 canonical로 구한 절대 경로다.
	 * directory가 저장소의 두 Content 루트 중 하나에 속하는지 확인한다.
	 */
	bool IsInsideContentRoot(const std::filesystem::path& root, const std::filesystem::path& directory)
	{
		auto rootPart = root.begin();
		auto directoryPart = directory.begin();

		while (rootPart != root.end())
		{
			if (directoryPart == directory.end()) { return false; }

			const bool isEqual = CompareStringOrdinal(rootPart->c_str(), -1, directoryPart->c_str(), -1, TRUE) == CSTR_EQUAL;
			if (!isEqual) { return false; }

			++rootPart;
			++directoryPart;
		}

		if (directoryPart == directory.end()) { return false; }

		std::filesystem::path relativePath;
		while (directoryPart != directory.end())
		{
			relativePath /= *directoryPart;
			++directoryPart;
		}

		return IsContentRelativePath(relativePath);
	}

	/**
	 * 일반 파일의 저장소 기준 상대 경로를 받아 표시용 유형을 반환한다.
	 * 파일을 열거나 에셋을 로딩하지 않는다.
	 */
	ContentBrowserEntryType ClassifyFile(const std::filesystem::path& relativePath)
	{
		const std::u8string filenameUtf8 = relativePath.filename().generic_u8string();
		const std::string filename(filenameUtf8.begin(), filenameUtf8.end());
		const std::string key = Paths::MakeAssetKey(filename);

		if (key.ends_with(".prefab.json")) { return ContentBrowserEntryType::Prefab; }
		if (key.ends_with(".gltf")) { return ContentBrowserEntryType::Model; }

		if (key.ends_with(".json"))
		{
			const std::filesystem::path parentPath = relativePath.parent_path();

			for (const std::filesystem::path& part : parentPath)
			{
				const bool isMaps = CompareStringOrdinal(part.c_str(), -1, L"Maps", -1, TRUE) == CSTR_EQUAL;
				if (isMaps) { return ContentBrowserEntryType::Map; }
			}
		}

		if (key.ends_with(".png") || key.ends_with(".jpg") || key.ends_with(".jpeg"))
		{
			return ContentBrowserEntryType::Image;
		}

		if (key.ends_with(".bin")) { return ContentBrowserEntryType::Binary; }

		return ContentBrowserEntryType::File;
	}

	/**
	 * left를 right보다 앞에 배치해야 하면 true를 반환한다.
	 * 폴더를 먼저 배치하고, 같은 종류끼리는 ASCII 대소문자를 무시한 이름순으로 비교한다.
	 */
	bool IsEntryLess(const ContentBrowserEntry& left, const ContentBrowserEntry& right)
	{
		const bool isLeftFolder = left.type == ContentBrowserEntryType::Folder;
		const bool isRightFolder = right.type == ContentBrowserEntryType::Folder;

		// 한쪽만 폴더이면 이름과 관계없이 폴더를 앞에 배치한다.
		if (isLeftFolder != isRightFolder)
		{
			return isLeftFolder;
		}

		// 표시할 원본 이름은 유지하고, 비교용 키에만 소문자 변환을 적용한다.
		const std::string leftKey = Paths::MakeAssetKey(left.name);
		const std::string rightKey = Paths::MakeAssetKey(right.name);

		if (leftKey != rightKey)
		{
			return leftKey < rightKey;
		}

		// 대소문자만 다른 이름도 순서가 일정하도록 원본 이름으로 비교한다.
		return left.name < right.name;
	}

	/**
	 * 콘텐츠 항목의 표시용 유형 이름을 반환한다.
	 * 반환 문자열은 문자열 리터럴이며, 호출자가 해제하거나 수정하지 않는다.
	 */
	const char* GetEntryTypeLabel(ContentBrowserEntryType type)
	{
		switch (type)
		{
		case ContentBrowserEntryType::Folder:
			return "Folder";

		case ContentBrowserEntryType::Model:
			return "Model";

		case ContentBrowserEntryType::Prefab:
			return "Prefab";

		case ContentBrowserEntryType::Map:
			return "Map";

		case ContentBrowserEntryType::Image:
			return "Image";

		case ContentBrowserEntryType::Binary:
			return "Binary";

		case ContentBrowserEntryType::File:
			return "File";
		}

		// 알 수 없는 유형도 일반 파일로 표시한다.
		return "File";
	}
}

bool ContentBrowserPanel::Initialize()
{
	std::filesystem::path directory;
	if (!Paths::GetRootDirectory(directory)) { return false; }

	// 루트 경로를 확보한 뒤 이전 탐색 상태를 초기화한다.
	rootDirectory = std::move(directory);
	currentDirectory.clear();
	currentEntries.clear();

	// 이전 탐색에서 선택한 파일이 새 초기화 상태에 남지 않도록 해제한다.
	selectedFilePath.clear();

	// 이전 폴더의 조회 결과와 오류 캐시를 모두 제거한다.
	directoryCache.clear();

	// 초기 폴더를 다시 열기 전에 이전 방문 기록과 위치를 초기화한다.
	history.clear();
	historyIndex = 0;

	// 초기 폴더 조회에 사용할 오류 변수는 오류가 없는 상태로 생성한다.
	std::error_code error;

	// Game/Content를 우선 조회하고, 실패하면 Engine/Content를 시도한다.
	return OpenInitialDirectory(error);
}

bool ContentBrowserPanel::Refresh(std::error_code& outError)
{
	if (currentDirectory.empty())
	{
		outError = std::make_error_code(std::errc::operation_not_permitted);
		return false;
	}

	// 정상 조회 결과와 실패 결과를 모두 제거해 이후 요청 시 다시 조회하게 한다.
	directoryCache.clear();

	// 폴더 재조회 결과를 보관하고 방문 기록은 추가하지 않는다.
	const bool refreshed = NavigateTo(currentDirectory, outError, false);

	// 폴더 조회가 실패했더라도 삭제되거나 접근할 수 없는 선택은 해제한다.
	ValidateSelection();

	return refreshed;
}

std::string ContentBrowserPanel::GetSelectedFilePath()
{
	// 삭제, 이동 또는 접근 오류로 무효해진 선택을 반환하지 않는다.
	ValidateSelection();

	return selectedFilePath;
}

void ContentBrowserPanel::Draw()
{
	ValidateSelection();

	if (ImGui::Begin("Content Browser"))
	{
		// 툴바 내부의 배치와 개별 작업 오류 처리는 DrawToolbar에 맡긴다.
		const std::error_code toolbarError = DrawToolbar();

		ImGui::Separator();

		std::error_code folderError;
		std::error_code contentError;

		// 아래쪽 선택 경로 표시를 위한 한 줄 높이를 남긴다.
		const float footerHeight = ImGui::GetTextLineHeightWithSpacing();

		const ImVec2 availableSize = ImGui::GetContentRegionAvail();
		const float areaHeight = availableSize.y - footerHeight;
		const float areaWidth = availableSize.x - ImGui::GetStyle().ItemSpacing.x;

		// 영역 사이 간격을 제외하고 왼쪽에 약 25%를 배분한다.
		// 공간이 없을 때 0 또는 음수 크기가 남은 영역 전체로 해석되지 않도록 한다.
		if (areaWidth >= 16.0f && areaHeight > 0.0f)
		{
			const float folderWidth = areaWidth * 0.25f;

			// 두 영역은 각각 스크롤하며, 아래쪽 선택 상태는 함께 스크롤하지 않는다.
			if (ImGui::BeginChild("FolderArea", ImVec2(folderWidth, areaHeight), ImGuiChildFlags_Borders))
			{
				folderError = DrawFolderTree();
			}
			ImGui::EndChild();

			ImGui::SameLine();

			// 트리에서 이동한 결과를 같은 프레임의 오른쪽 목록에 반영한다.
			if (ImGui::BeginChild("ContentArea", ImVec2(0.0f, areaHeight), ImGuiChildFlags_Borders))
			{
				// 현재 콘텐츠 창에만 스크롤 초기화를 요청하고, 처리한 요청은 해제한다.
				if (resetContentScroll)
				{
					ImGui::SetScrollY(0.0f);
					resetContentScroll = false;
				}

				contentError = DrawContentGrid();
			}
			ImGui::EndChild();
		}

		// 트리와 타일의 입력 처리 후 선택 상태를 표시한다.
		ImGui::Text("Selected: %s", selectedFilePath.empty() ? "(none)" : selectedFilePath.c_str());

		// 툴바, 폴더 트리와 콘텐츠 조작 실패를 공통 팝업으로 알린다.
		if (toolbarError || folderError || contentError)
		{
			ImGui::OpenPopup("Content Browser Error");
		}

		// 상세 오류를 보관하지 않고 작업 실패 사실만 표시한다.
		if (ImGui::BeginPopup("Content Browser Error"))
		{
			ImGui::TextUnformatted("Could not complete the content browser operation.");

			if (ImGui::Button("Close"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	// Begin의 반환값과 관계없이 End를 호출해야 한다.
	ImGui::End();
}

std::error_code ContentBrowserPanel::DrawToolbar()
{
	std::error_code error;

	const ImGuiStyle& style = ImGui::GetStyle();

	// 버튼 한 줄, 위아래 여백과 가로 스크롤바가 들어갈 높이를 확보한다.
	const float toolbarHeight = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f + style.ScrollbarSize;

	// 툴바만 가로 스크롤되도록 콘텐츠 영역과 분리한다.
	if (ImGui::BeginChild("ToolbarArea", ImVec2(0.0f, toolbarHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
	{
		const std::error_code navigationError = DrawNavigationButtons();

		ImGui::SameLine();
		const std::error_code breadcrumbError = DrawBreadcrumb();

		ImGui::SameLine();
		DrawSearchInput();

		ImGui::SameLine();
		const std::error_code refreshError = DrawRefreshButton();

		// 뒤의 정상 결과가 앞서 발생한 오류를 덮어쓰지 않도록 한다.
		if (navigationError)
		{
			error = navigationError;
		}
		else if (breadcrumbError)
		{
			error = breadcrumbError;
		}
		else if (refreshError)
		{
			error = refreshError;
		}
	}

	// BeginChild의 반환값과 관계없이 호출한다.
	ImGui::EndChild();

	return error;
}

std::error_code ContentBrowserPanel::DrawNavigationButtons()
{
	std::error_code error;

	// 이전 기록이 없으면 버튼을 비활성화한다.
	ImGui::BeginDisabled(!CanGoBack());
	if (ImGui::Button("<"))
	{
		GoBack(error);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	ImGui::BeginDisabled(!CanGoForward());
	if (ImGui::Button(">"))
	{
		GoForward(error);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	// Content 루트에서는 상위 폴더 이동 버튼을 비활성화한다.
	ImGui::BeginDisabled(!CanGoUp());
	if (ImGui::Button("Up"))
	{
		GoUp(error);
	}
	ImGui::EndDisabled();

	return error;
}

std::error_code ContentBrowserPanel::DrawRefreshButton()
{
	std::error_code error;

	// 다시 조회할 현재 폴더가 있을 때만 버튼 입력을 허용한다.
	ImGui::BeginDisabled(currentDirectory.empty());

	if (ImGui::Button("Refresh"))
	{
		Refresh(error);
	}

	ImGui::EndDisabled();

	return error;
}

std::error_code ContentBrowserPanel::DrawBreadcrumb()
{
	std::error_code error;

	if (currentDirectory.empty())
	{
		ImGui::TextUnformatted("(none)");
		return error;
	}

	// 경로를 그리는 도중 현재 폴더를 변경하지 않도록 복사본을 순회한다.
	const std::u8string pathUtf8(currentDirectory.begin(), currentDirectory.end());
	const std::filesystem::path directory(pathUtf8);

	std::filesystem::path breadcrumbPath;
	std::string requestedDirectory;
	int depth = 0;

	for (const std::filesystem::path& part : directory)
	{
		// 끝 구분자로 만들어지는 빈 요소는 표시하지 않는다.
		if (part.empty()) { continue; }

		breadcrumbPath /= part;

		const std::u8string nameUtf8 = part.generic_u8string();
		const std::string name(nameUtf8.begin(), nameUtf8.end());

		if (depth > 0)
		{
			ImGui::SameLine();
			ImGui::TextUnformatted("/");
			ImGui::SameLine();
		}

		if (depth == 0)
		{
			// Game 또는 Engine 자체는 허용된 Content 탐색 경로가 아니다.
			ImGui::TextUnformatted(name.c_str());
		}
		else
		{
			const std::u8string breadcrumbUtf8 = breadcrumbPath.generic_u8string();
			const std::string buttonPath(breadcrumbUtf8.begin(), breadcrumbUtf8.end());

			// 같은 이름의 구간도 전체 상대 경로로 구분한다.
			ImGui::PushID(buttonPath.c_str());

			if (ImGui::SmallButton(name.c_str()))
			{
				requestedDirectory = buttonPath;
			}

			ImGui::PopID();
		}

		++depth;
	}

	// 경로 표시를 마친 뒤 이동해 이번 순회의 경로와 상태 변경을 분리한다.
	if (!requestedDirectory.empty())
	{
		NavigateTo(requestedDirectory, error, true);
	}

	return error;
}

void ContentBrowserPanel::DrawSearchInput()
{
	// 툴바 전체 배치를 구성하기 전까지 사용할 입력창 너비다.
	ImGui::SetNextItemWidth(200.0f);

	ImGui::InputTextWithHint("##ContentSearch", "Search...", searchText.data(), searchText.size());
}

bool ContentBrowserPanel::MatchesSearch(const ContentBrowserEntry& entry) const
{
	std::string query(searchText.data());

	if (query.empty()) { return true; }

	std::string name = entry.name;

	// 검색어는 경로가 아니므로 경로 정규화 없이 ASCII 영문만 소문자로 바꾼다.
	for (char& character : query)
	{
		if (character >= 'A' && character <= 'Z')
		{
			character = static_cast<char>(character + ('a' - 'A'));
		}
	}

	for (char& character : name)
	{
		if (character >= 'A' && character <= 'Z')
		{
			character = static_cast<char>(character + ('a' - 'A'));
		}
	}

	return name.find(query) != std::string::npos;
}

std::error_code ContentBrowserPanel::DrawEntryTile(const ContentBrowserEntry& entry, float tileWidth, std::string& outRequestedDirectory)
{
	std::error_code error;

	const bool isFolder = entry.type == ContentBrowserEntryType::Folder;
	bool isSelected = false;

	if (!isFolder)
	{
		const std::string selectedKey = Paths::MakeAssetKey(selectedFilePath);
		const std::string entryKey = Paths::MakeAssetKey(entry.relativePath);

		isSelected = selectedKey == entryKey;
	}

	const ImVec2 tilePosition = ImGui::GetCursorScreenPos();
	const ImVec2 tileSize(tileWidth, 80.0f);

	// 표시 이름과 별개로 전체 상대 경로를 ID에 사용한다.
	ImGui::PushID(entry.relativePath.c_str());

	// 타일 전체가 클릭 영역이며 선택 상태에 따라 배경을 표시한다.
	if (ImGui::Selectable("##Tile", isSelected, ImGuiSelectableFlags_AllowDoubleClick, tileSize))
	{
		if (isFolder)
		{
			// 순회 중 목록이 교체되지 않도록 실제 이동은 호출자에게 맡긴다.
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				outRequestedDirectory = entry.relativePath;
			}
		}
		else
		{
			SelectFile(entry.relativePath, error);
		}
	}

	// 이름에 ## 같은 문자가 있어도 그대로 표시하도록 버튼 라벨과 분리한다.
	std::string text = GetEntryTypeLabel(entry.type);
	text += "\n";
	text += entry.name;

	const ImVec2 textPosition(tilePosition.x + 6.0f, tilePosition.y + 6.0f);
	const ImVec2 tileEnd(tilePosition.x + tileSize.x, tilePosition.y + tileSize.y);

	// 긴 이름은 타일 안에서 줄바꿈하고 타일 밖으로 그리지 않는다.
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(tilePosition, tileEnd, true);
	drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPosition, ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), nullptr, tileWidth - 12.0f);
	drawList->PopClipRect();

	ImGui::PopID();

	return error;
}

std::error_code ContentBrowserPanel::DrawContentGrid()
{
	std::error_code error;

	if (currentDirectory.empty())
	{
		ImGui::TextUnformatted("No folder selected.");
		return error;
	}

	const float availableWidth = ImGui::GetContentRegionAvail().x;

	// 타일 내부 여백과 텍스트 영역을 확보할 수 없는 너비에서는 그리지 않는다.
	if (availableWidth < 24.0f) { return error; }

	constexpr float PreferredTileWidth = 128.0f;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;

	int columnCount = static_cast<int>((availableWidth + spacing) / (PreferredTileWidth + spacing));
	if (columnCount < 1)
	{
		columnCount = 1;
	}

	// 열 사이 간격을 제외한 너비를 각 타일에 균등하게 배분한다.
	const float tileWidth = (availableWidth - spacing * (columnCount - 1)) / columnCount;

	int column = 0;
	int visibleCount = 0;
	std::string requestedDirectory;

	for (const ContentBrowserEntry& entry : currentEntries)
	{
		if (!MatchesSearch(entry)) { continue; }

		if (column > 0)
		{
			ImGui::SameLine();
		}

		const std::error_code tileError = DrawEntryTile(entry, tileWidth, requestedDirectory);

		// 뒤의 정상 항목이 앞서 발생한 오류를 덮어쓰지 않도록 한다.
		if (tileError && !error)
		{
			error = tileError;
		}

		++visibleCount;
		++column;

		if (column == columnCount)
		{
			column = 0;
		}
	}

	if (visibleCount == 0)
	{
		ImGui::TextUnformatted(currentEntries.empty() ? "This folder is empty." : "No matching items.");
	}

	// NavigateTo가 현재 목록을 교체하므로 항목 순회가 끝난 뒤 호출한다.
	if (!requestedDirectory.empty())
	{
		std::error_code navigationError;
		NavigateTo(requestedDirectory, navigationError, true);

		if (!error)
		{
			error = navigationError;
		}
	}

	return error;
}

void ContentBrowserPanel::DrawFolderNode(std::string_view relativePath, std::string_view displayName, std::string& outRequestedDirectory)
{
	const std::string path(relativePath);
	const std::string name(displayName);

	std::string key = Paths::MakeAssetKey(path);
	if (!key.empty() && key.back() == '/')
	{
		key.pop_back();
	}

	std::string currentKey = Paths::MakeAssetKey(currentDirectory);
	if (!currentKey.empty() && currentKey.back() == '/')
	{
		currentKey.pop_back();
	}

	// 삼각형으로만 펼침 상태를 바꾸고, 이름 클릭은 폴더 이동에 사용한다.
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	if (key == currentKey)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	// 표시 이름과 별개로 전체 경로를 노드 ID에 사용한다.
	const bool isOpen = ImGui::TreeNodeEx(path.c_str(), flags, "%s", name.c_str());

	// 펼침 상태를 바꾼 삼각형 클릭은 이동 요청으로 처리하지 않는다.
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
	{
		outRequestedDirectory = path;
	}

	if (isOpen)
	{
		// 접힌 폴더의 내부는 조회하지 않는다. 실패 결과도 캐시에서 재사용한다.
		const ContentBrowserDirectoryCache& cache = GetDirectoryEntries(path);

		if (cache.error)
		{
			ImGui::TextDisabled("Unable to read this folder.");
		}
		else
		{
			for (const ContentBrowserEntry& entry : cache.entries)
			{
				if (entry.type != ContentBrowserEntryType::Folder) { continue; }

				DrawFolderNode(entry.relativePath, entry.name, outRequestedDirectory);
			}
		}

		ImGui::TreePop();
	}
}

std::error_code ContentBrowserPanel::DrawFolderTree()
{
	std::error_code error;
	std::string requestedDirectory;

	ImGui::TextUnformatted("Folders");
	ImGui::Separator();

	// 표시 이름에도 Content를 포함해 실제 탐색 시작 위치를 명확히 한다.
	DrawFolderNode("Game/Content", "Game/Content", requestedDirectory);
	DrawFolderNode("Engine/Content", "Engine/Content", requestedDirectory);

	// 이동 시 캐시가 교체될 수 있으므로 두 트리의 순회가 끝난 뒤 처리한다.
	if (!requestedDirectory.empty())
	{
		NavigateTo(requestedDirectory, error, true);
	}

	return error;
}

bool ContentBrowserPanel::ResolveDirectory(std::string_view relativePath, std::filesystem::path& outDirectory, std::error_code& outError) const
{
	if (rootDirectory.empty())
	{
		outError = std::make_error_code(std::errc::operation_not_permitted);
		return false;
	}

	// Windows의 시스템 코드 페이지 대신 UTF-8로 해석하도록 char8_t 문자열을 사용한다.
	const std::u8string pathUtf8(relativePath.begin(), relativePath.end());
	const std::filesystem::path requestedPath(pathUtf8);

	// 호출자가 저장소 기준 경로 대신 드라이브나 루트를 지정하지 못하게 한다.
	if (requestedPath.empty() || requestedPath.has_root_path())
	{
		outError = std::make_error_code(std::errc::invalid_argument);
		return false;
	}

	// .과 ..을 정리한 결과를 기준으로 Content 경계를 검사한다.
	const std::filesystem::path normalizedPath = requestedPath.lexically_normal();

	if (!IsContentRelativePath(normalizedPath))
	{
		outError = std::make_error_code(std::errc::permission_denied);
		return false;
	}

	const std::filesystem::path root = std::filesystem::canonical(rootDirectory, outError);
	if (outError) { return false; }

	// 실제 경로를 해석하기 전에 중간 폴더를 포함한 링크와 junction을 거부한다.
	if (!CheckNoReparsePoints(root, normalizedPath, outError))
	{
		return false;
	}

	const std::filesystem::path directory = std::filesystem::canonical(root / normalizedPath, outError);
	if (outError) { return false; }

	// 입력 경로 검사와 별도로, 해석된 실제 경로도 허용 범위인지 확인한다.
	if (!IsInsideContentRoot(root, directory))
	{
		outError = std::make_error_code(std::errc::permission_denied);
		return false;
	}

	const bool isDirectory = std::filesystem::is_directory(directory, outError);
	if (outError) { return false; }

	if (!isDirectory)
	{
		outError = std::make_error_code(std::errc::not_a_directory);
		return false;
	}

	// 모든 검사가 성공한 뒤에만 출력값을 바꿔 실패 시 기존 값을 보존한다.
	outDirectory = directory;
	return true;
}

bool ContentBrowserPanel::ReadDirectory(std::string_view relativePath, std::vector<ContentBrowserEntry>& outEntries, std::error_code& outError) const
{
	std::filesystem::path directory;
	if (!ResolveDirectory(relativePath, directory, outError)) { return false; }

	// 조회된 절대 경로에서 저장소 기준 상대 경로를 만들 때 사용한다.
	const std::filesystem::path root = std::filesystem::canonical(rootDirectory, outError);
	if (outError) { return false; }

	std::filesystem::directory_iterator iterator(directory, outError);
	if (outError) { return false; }

	const std::filesystem::directory_iterator end;
	std::vector<ContentBrowserEntry> entries;

	while (iterator != end)
	{
		const std::filesystem::directory_entry& entry = *iterator;
		const DWORD attributes = GetFileAttributesW(entry.path().c_str());

		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			const DWORD error = GetLastError();
			outError = std::error_code(static_cast<int>(error), std::system_category());
			return false;
		}

		// 항목 자체가 링크나 junction이면 목록에 포함하지 않는다.
		if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
		{
			const bool isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			const bool isRegularFile = !isDirectory && entry.is_regular_file(outError);
			if (outError) { return false; }

			if (isDirectory || isRegularFile)
			{
				const std::filesystem::path entryRelativePath = entry.path().lexically_relative(root);
				const std::u8string nameUtf8 = entry.path().filename().generic_u8string();
				const std::u8string relativePathUtf8 = entryRelativePath.generic_u8string();

				ContentBrowserEntry item;
				item.name.assign(nameUtf8.begin(), nameUtf8.end());
				item.relativePath.assign(relativePathUtf8.begin(), relativePathUtf8.end());
				item.type = isDirectory ? ContentBrowserEntryType::Folder : ClassifyFile(entryRelativePath);

				entries.push_back(std::move(item));
			}
		}

		// 다음 항목으로 이동하는 과정에서도 디스크 조회 오류가 발생할 수 있다.
		iterator.increment(outError);
		if (outError) { return false; }
	}

	std::sort(entries.begin(), entries.end(), IsEntryLess);

	// 조회와 정렬이 완료된 결과만 반영해 실패 시 기존 목록을 보존한다.
	outEntries = std::move(entries);
	return true;
}

const ContentBrowserDirectoryCache& ContentBrowserPanel::GetDirectoryEntries(std::string_view relativePath)
{
	std::string key = Paths::MakeAssetKey(relativePath);

	// 끝 구분자 유무만 다른 경로도 같은 캐시를 사용한다.
	if (!key.empty() && key.back() == '/')
	{
		key.pop_back();
	}

	auto cached = directoryCache.find(key);

	if (cached != directoryCache.end())
	{
		return cached->second;
	}

	// 새 항목의 빈 목록과 오류 없는 error를 조회 함수에 전달한다.
	ContentBrowserDirectoryCache cache;

	// 조회 실패 시에도 오류를 보관해 같은 폴더를 반복 조회하지 않는다.
	ReadDirectory(relativePath, cache.entries, cache.error);

	// 저장된 항목의 반복자를 받은 뒤 패널이 소유한 캐시를 반환한다.
	auto inserted = directoryCache.emplace(std::move(key), std::move(cache)).first;
	return inserted->second;
}

bool ContentBrowserPanel::NavigateTo(std::string_view relativePath, std::error_code& outError, bool recordHistory)
{
	// 조회 실패 시 선택을 포함한 기존 탐색 상태를 유지한다.
	std::vector<ContentBrowserEntry> entries;
	if (!ReadDirectory(relativePath, entries, outError)) { return false; }

	// 현재 폴더는 저장소 기준 UTF-8 상대 경로로 보관한다.
	const std::u8string pathUtf8(relativePath.begin(), relativePath.end());
	const std::filesystem::path requestedPath(pathUtf8);
	const std::u8string normalizedUtf8 = requestedPath.lexically_normal().generic_u8string();
	std::string directoryPath(normalizedUtf8.begin(), normalizedUtf8.end());

	std::string key = Paths::MakeAssetKey(directoryPath);

	// 캐시 조회 함수와 동일하게 끝 구분자를 제거한다.
	if (!key.empty() && key.back() == '/')
	{
		key.pop_back();
	}

	std::string currentKey = Paths::MakeAssetKey(currentDirectory);

	// 대소문자와 끝 구분자 차이만으로 다른 폴더로 판단하지 않는다.
	if (!currentKey.empty() && currentKey.back() == '/')
	{
		currentKey.pop_back();
	}

	const bool isDirectoryChanged = currentKey != key;

	// 현재 목록과 트리 캐시가 각각 소유하도록 조회 결과를 복사한다.
	ContentBrowserDirectoryCache cache;
	cache.entries = entries;

	// 기존 실패 결과도 성공한 최신 목록과 오류 없는 상태로 교체한다.
	directoryCache.insert_or_assign(std::move(key), std::move(cache));

	currentDirectory = std::move(directoryPath);
	currentEntries = std::move(entries);

	// 같은 폴더의 새로고침에서는 유지하고, 다른 폴더로 이동하면 해제한다.
	if (isDirectoryChanged)
	{
		selectedFilePath.clear();

		// 콘텐츠 영역을 그릴 때 새 폴더의 첫 줄부터 표시하도록 요청한다.
		resetContentScroll = true;
	}

	// 기록을 요청한 이동만 방문 기록에 반영한다.
	if (recordHistory)
	{
		RecordCurrentDirectory();
	}

	return true;
}

bool ContentBrowserPanel::OpenInitialDirectory(std::error_code& outError)
{
	// Game 조회 실패는 대체 경로를 시도할 수 있도록 지역 오류로 받는다.
	std::error_code gameError;

	// 조회에 성공한 Game 폴더를 최초 방문 기록에 추가한다.
	if (NavigateTo("Game/Content", gameError, true))
	{
		return true;
	}

	// Game 조회가 실패하면 Engine을 시도하고, 성공한 경우에만 기록한다.
	return NavigateTo("Engine/Content", outError, true);
}

bool ContentBrowserPanel::GoUp(std::error_code& outError)
{
	if (currentDirectory.empty())
	{
		outError = std::make_error_code(std::errc::operation_not_permitted);
		return false;
	}

	const std::u8string pathUtf8(currentDirectory.begin(), currentDirectory.end());
	std::filesystem::path directory(pathUtf8);

	// 끝에 구분자가 있더라도 실제 마지막 폴더를 기준으로 부모를 구한다.
	if (!directory.has_filename())
	{
		directory = directory.parent_path();
	}

	const std::filesystem::path parent = directory.parent_path();
	const std::u8string parentUtf8 = parent.generic_u8string();
	const std::string parentPath(parentUtf8.begin(), parentUtf8.end());

	// Content 경계를 검사하고, 상위 폴더 이동 성공 시 방문 기록에 추가한다.
	return NavigateTo(parentPath, outError, true);
}

bool ContentBrowserPanel::CanGoUp() const
{
	if (currentDirectory.empty()) { return false; }

	const std::u8string pathUtf8(currentDirectory.begin(), currentDirectory.end());
	std::filesystem::path directory(pathUtf8);

	// 끝 구분자를 제외한 실제 마지막 폴더를 기준으로 부모를 구한다.
	if (!directory.has_filename())
	{
		directory = directory.parent_path();
	}

	const std::filesystem::path parent = directory.parent_path();

	// 부모 경로도 두 Content 루트 중 하나에 속해야 한다.
	return IsContentRelativePath(parent);
}

bool ContentBrowserPanel::CanGoBack() const
{
	return !history.empty() && historyIndex > 0;
}

bool ContentBrowserPanel::GoBack(std::error_code& outError)
{
	if (!CanGoBack())
	{
		outError = std::make_error_code(std::errc::operation_not_permitted);
		return false;
	}

	const std::size_t previousIndex = historyIndex - 1;

	// 기록을 추가하지 않고 이전 경로를 다시 조회한다.
	if (!NavigateTo(history[previousIndex], outError, false))
	{
		return false;
	}

	// 실제 이동이 성공한 뒤에만 현재 기록 위치를 변경한다.
	historyIndex = previousIndex;
	return true;
}

bool ContentBrowserPanel::GoForward(std::error_code& outError)
{
	// 다음 기록이 있는지 확인한 뒤에만 인덱스를 계산한다.
	if (!CanGoForward())
	{
		outError = std::make_error_code(std::errc::operation_not_permitted);
		return false;
	}

	const std::size_t nextIndex = historyIndex + 1;

	// 기록을 추가하지 않고 다음 경로를 다시 조회한다.
	if (!NavigateTo(history[nextIndex], outError, false))
	{
		return false;
	}

	// 실제 이동이 성공한 뒤에만 현재 기록 위치를 변경한다.
	historyIndex = nextIndex;
	return true;
}

bool ContentBrowserPanel::CanGoForward() const
{
	// 빈 기록에서는 size() - 1을 계산하지 않는다.
	return !history.empty() && historyIndex < history.size() - 1;
}

void ContentBrowserPanel::RecordCurrentDirectory()
{
	if (currentDirectory.empty()) { return; }

	std::string currentKey = Paths::MakeAssetKey(currentDirectory);

	// 끝 구분자 유무만 다른 경로도 같은 폴더로 비교한다.
	if (currentKey.back() == '/')
	{
		currentKey.pop_back();
	}

	if (!history.empty())
	{
		std::string recordedKey = Paths::MakeAssetKey(history[historyIndex]);

		if (recordedKey.back() == '/')
		{
			recordedKey.pop_back();
		}

		// 새로고침이나 같은 폴더 선택으로 연속된 중복 기록을 만들지 않는다.
		if (currentKey == recordedKey) { return; }

		// 현재 기록까지만 남겨 기존 앞으로 가기 경로를 제거한다.
		history.resize(historyIndex + 1);
	}

	history.push_back(currentDirectory);
	historyIndex = history.size() - 1;
}

bool ContentBrowserPanel::SelectFile(std::string_view relativePath, std::error_code& outError)
{
	const std::string key = Paths::MakeAssetKey(relativePath);

	for (const ContentBrowserEntry& entry : currentEntries)
	{
		if (entry.type == ContentBrowserEntryType::Folder) { continue; }
		if (Paths::MakeAssetKey(entry.relativePath) != key) { continue; }

		// 캐시된 목록과 별개로 현재 부모 폴더가 여전히 탐색 가능한지 확인한다.
		std::filesystem::path directory;
		if (!ResolveDirectory(currentDirectory, directory, outError)) { return false; }

		const std::u8string nameUtf8(entry.name.begin(), entry.name.end());
		const std::filesystem::path filePath = directory / std::filesystem::path(nameUtf8);

		const DWORD attributes = GetFileAttributesW(filePath.c_str());

		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			const DWORD error = GetLastError();
			outError = std::error_code(static_cast<int>(error), std::system_category());
			return false;
		}

		// 목록 조회 이후 파일이 링크로 교체되었을 수도 있으므로 다시 확인한다.
		if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			outError = std::make_error_code(std::errc::permission_denied);
			return false;
		}

		const bool isRegularFile = std::filesystem::is_regular_file(filePath, outError);
		if (outError) { return false; }

		if (!isRegularFile)
		{
			outError = std::make_error_code(std::errc::invalid_argument);
			return false;
		}

		// 입력 표기 대신 목록에 보관한 파일 경로를 선택 상태로 저장한다.
		selectedFilePath = entry.relativePath;
		return true;
	}

	outError = std::make_error_code(std::errc::invalid_argument);
	return false;
}

void ContentBrowserPanel::ValidateSelection()
{
	if (selectedFilePath.empty()) { return; }

	std::error_code error;

	// 기존 선택 함수를 재사용해 목록 소속, 부모 경로와 실제 파일을 검사한다.
	if (!SelectFile(selectedFilePath, error))
	{
		selectedFilePath.clear();
	}
}
