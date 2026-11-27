#include "Paths.h"

#include <Windows.h>

#include <string>

bool Paths::GetRootDirectory(std::filesystem::path& outPath)
{
	// MAX_PATH(260자)보다 긴 경로도 받도록 Windows 최대 경로 길이로 잡는다.
	std::wstring executablePath(32768, L'\0');
	const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));

	// 0은 실패, 버퍼 크기 이상이면 경로가 잘렸다는 뜻이다.
	if (length == 0 || length >= executablePath.size()) { return false; }
	executablePath.resize(length);

	// <루트>/Binaries/<도구 집합>/<플랫폼>/<구성>/SeonEngine.exe에서 네 번 올라간 곳이 Binaries가 아니면
	// 출력 배치가 바뀐 것이므로 엉뚱한 폴더를 루트로 쓰지 않는다.
	const std::filesystem::path binariesDirectory = std::filesystem::path(executablePath).parent_path().parent_path().parent_path().parent_path();
	// Windows에서 실행할 때 사용한 경로의 대소문자가 그대로 전달될 수 있다.
	const std::filesystem::path binariesName = binariesDirectory.filename();
	const bool isBinariesDirectory = CompareStringOrdinal(binariesName.c_str(), -1, L"Binaries", -1, TRUE) == CSTR_EQUAL;
	if (!isBinariesDirectory) { return false; }

	outPath = binariesDirectory.parent_path();

	return true;
}

bool Paths::GetAbsolutePath(std::string_view relativePath, std::filesystem::path& outPath)
{
	std::filesystem::path rootDirectory;
	if (!GetRootDirectory(rootDirectory)) { return false; }

	// Windows에서 std::string으로 만든 path는 CP949로 해석되므로 UTF-8로 해석하게 한다.
	outPath = rootDirectory / std::filesystem::path(std::u8string(relativePath.begin(), relativePath.end()));

	return true;
}

std::optional<std::string> Paths::FindDiskPath(std::string_view relativePath)
{
	// FindFirstFileW는 *와 ?를 패턴으로 해석해 다른 파일을 찾을 수 있으므로 거부한다.
	if (relativePath.find_first_of("*?") != std::string_view::npos) { return std::nullopt; }

	// 디스크를 보지 않고 .과 ..을 정리한다. 파일 이름이 없거나 절대 경로면 거부한다.
	const std::filesystem::path requested = std::filesystem::path(std::u8string(relativePath.begin(), relativePath.end())).lexically_normal();
	if (!requested.has_filename() || requested.has_root_path()) { return std::nullopt; }

	std::filesystem::path current;
	if (!GetRootDirectory(current)) { return std::nullopt; }

	// 루트부터 한 단계씩 내려가며 각 이름을 디스크의 표기로 바꾼다.
	// FindFirstFileW에 정확한 이름 하나를 넘기면 대소문자가 달라도 찾고, cFileName에 디스크의 이름을 돌려준다.
	std::filesystem::path diskPath;
	for (const std::filesystem::path& part : requested)
	{
		// 정리한 뒤에도 남은 ..는 루트 밖을 가리킨다.
		if (part == "..") { return std::nullopt; }

		WIN32_FIND_DATAW found;
		const HANDLE search = FindFirstFileW((current / part).c_str(), &found);
		if (search == INVALID_HANDLE_VALUE) { return std::nullopt; }
		FindClose(search);

		current /= found.cFileName;
		diskPath /= found.cFileName;
	}

	const std::u8string diskPathUtf8 = diskPath.generic_u8string();
	return std::string(diskPathUtf8.begin(), diskPathUtf8.end());
}

std::string Paths::MakeAssetKey(std::string_view path)
{
	const std::u8string pathUtf8(path.begin(), path.end());
	const std::filesystem::path assetPath(pathUtf8);

	// 디스크를 조회하지 않고 경로 표기에서 .과 ..을 정리한다.
	const std::filesystem::path normalizedPath = assetPath.lexically_normal();
	const std::u8string normalizedUtf8 = normalizedPath.generic_u8string();

	// UTF-8의 멀티바이트 문자를 보존하기 위해 ASCII 대문자만 바꾼다.
	std::string key(normalizedUtf8.begin(), normalizedUtf8.end());
	for (char& character : key)
	{
		if (character >= 'A' && character <= 'Z')
		{
			character = static_cast<char>(character + ('a' - 'A'));
		}
	}

	return key;
}

std::string Paths::MakeSubAssetPath(std::string_view filePath, std::string_view subAssetName)
{
	std::string path(filePath);
	path += SubAssetSeparator;
	path += subAssetName;
	return path;
}

std::optional<std::pair<std::string_view, std::string_view>> Paths::SplitSubAssetPath(std::string_view path)
{
	const std::size_t separator = path.rfind(SubAssetSeparator);
	if (separator == std::string_view::npos) { return std::nullopt; }
	return std::pair{ path.substr(0, separator), path.substr(separator + 1) };
}
