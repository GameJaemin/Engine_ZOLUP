#include "Prefab.h"

#include "Core/FileHelper.h"
#include "Core/Log.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

bool ReadPrefabModelPath(const std::filesystem::path& absolutePath, std::optional<std::string>& outModelPath)
{
	const std::u8string utf8Path = absolutePath.u8string();
	const std::string_view pathText(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

	std::vector<std::byte> bytes;
	if (!FileHelper::ReadAllBytes(absolutePath, bytes))
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: could not read prefab file", pathText));
		return false;
	}

	// RapidJSON이 원시 NUL 바이트를 입력 끝으로 처리할 수 있다.
	if (bytes.empty() || std::ranges::find(bytes, std::byte{0}) != bytes.end())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: file is empty or contains a raw NUL byte", pathText));
		return false;
	}

	rapidjson::Document document;
	document.Parse<rapidjson::kParseValidateEncodingFlag>(reinterpret_cast<const char*>(bytes.data()), bytes.size());

	if (document.HasParseError())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: invalid JSON at byte {}: {}", pathText, document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError())));
		return false;
	}

	if (!document.IsObject())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: JSON root must be an object", pathText));
		return false;
	}

	const auto model = document.FindMember("model");
	if (model != document.MemberEnd())
	{
		if (!model->value.IsString())
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: model must be a string", pathText));
			return false;
		}

		const std::string_view modelPath(model->value.GetString(), model->value.GetStringLength());
		if (modelPath.empty() || modelPath.find('\0') != std::string_view::npos)
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: model must be non-empty and contain no NUL", pathText));
			return false;
		}

		const std::filesystem::path path(std::u8string(modelPath.begin(), modelPath.end()));
		const std::filesystem::path normalizedPath = path.lexically_normal();
		const bool goesOutsideRoot = normalizedPath.begin() != normalizedPath.end() && *normalizedPath.begin() == "..";
		if (path.has_root_path() || goesOutsideRoot)
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: model must be a repository-relative path", pathText));
			return false;
		}

		outModelPath.emplace(modelPath);
	}
	else
	{
		outModelPath.reset();
	}

	return true;
}
