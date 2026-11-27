#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/** 파일을 읽고 쓰는 도우미 함수를 모은다. */
namespace FileHelper
{
	/** 파일 전체를 바이트 배열로 읽는다. 열거나 읽지 못하면 출력값을 유지하고 false를 반환한다. */
	bool ReadAllBytes(const std::filesystem::path& path, std::vector<std::byte>& outBytes);

	/** 텍스트 파일 전체를 텍스트 모드로 읽는다. 열거나 읽지 못하면 출력값을 유지하고 false를 반환한다. */
	bool ReadAllText(const std::filesystem::path& path, std::string& outText);

	/**
	 * text를 바꾸지 않고 그대로 path에 쓴다. 같은 폴더의 path.tmp에 모두 쓴 뒤 바꿔치기하므로 실패해도 기존 파일은 그대로다.
	 * 쓰거나 바꾸지 못하면 임시 파일을 지우고 false를 반환한다.
	 */
	bool WriteAllText(const std::filesystem::path& path, std::string_view text);
}
