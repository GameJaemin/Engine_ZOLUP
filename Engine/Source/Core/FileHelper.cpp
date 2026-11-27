#include "FileHelper.h"

#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

bool FileHelper::ReadAllBytes(const std::filesystem::path& path, std::vector<std::byte>& outBytes)
{
	// 끝에서 열어 현재 위치로 파일 크기를 얻는다.
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) { return false; }

	const std::streamsize size = file.tellg();
	if (size < 0) { return false; }

	// 실패해도 outBytes가 일부만 채워지지 않도록 지역 배열에 먼저 읽는다.
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) { return false; }

	outBytes = std::move(bytes);
	return true;
}

bool FileHelper::ReadAllText(const std::filesystem::path& path, std::string& outText)
{
	// 옵션 없이 열면 텍스트 모드다.
	std::ifstream file(path);
	if (!file) { return false; }

	// 줄바꿈 변환 때문에 읽은 양이 파일 크기와 다를 수 있어 크기를 미리 구하지 않고 끝까지 차례로 읽는다.
	std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	if (file.bad()) { return false; }

	outText = std::move(text);
	return true;
}

bool FileHelper::WriteAllText(const std::filesystem::path& path, std::string_view text)
{
	// 같은 폴더의 임시 파일에 끝까지 쓴 뒤 이름을 바꾼다. 쓰다가 실패해도 원래 파일은 그대로 남는다.
	std::filesystem::path temporaryPath = path;
	temporaryPath += ".tmp";
	{
		// 바이너리 모드로 열어 줄바꿈을 바꾸지 않는다. 텍스트 모드는 Windows에서 \n을 \r\n으로 바꾼다.
		std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
		if (!file) { return false; }
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
		// 쓰기 오류는 닫을 때 드러나기도 하므로 닫은 뒤 상태를 본다.
		file.close();
		if (!file)
		{
			std::error_code ignored;
			std::filesystem::remove(temporaryPath, ignored);
			return false;
		}
	}

	// 대상 파일이 이미 있으면 이름 바꾸기가 그 파일을 대신한다.
	std::error_code error;
	std::filesystem::rename(temporaryPath, path, error);
	if (error)
	{
		std::error_code ignored;
		std::filesystem::remove(temporaryPath, ignored);
		return false;
	}
	return true;
}
