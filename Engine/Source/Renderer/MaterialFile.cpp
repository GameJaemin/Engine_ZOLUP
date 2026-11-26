#include "Renderer/MaterialFile.h"

#include "Core/FileHelper.h"
#include "Core/Log.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	void LogFailure(std::string_view pathText, std::string_view reason)
	{
		Log::Write(LogLevel::Error, "Material", std::format("{}: {}", pathText, reason));
	}

	/** 0부터 1까지의 유한한 숫자면 그 값을, 아니면 nullopt를 반환한다. */
	std::optional<float> ReadUnitFloat(const rapidjson::Value& value)
	{
		if (!value.IsNumber()) { return std::nullopt; }

		const double number = value.GetDouble();
		if (!std::isfinite(number) || number < 0.0 || number > 1.0) { return std::nullopt; }
		return static_cast<float>(number);
	}

	/** 0부터 1까지의 숫자 3개로 된 배열이면 Vector3를, 아니면 nullopt를 반환한다. */
	std::optional<Vector3> ReadUnitVector3(const rapidjson::Value& value)
	{
		if (!value.IsArray() || value.Size() != 3) { return std::nullopt; }

		const std::optional<float> x = ReadUnitFloat(value[0]);
		const std::optional<float> y = ReadUnitFloat(value[1]);
		const std::optional<float> z = ReadUnitFloat(value[2]);
		if (!x.has_value() || !y.has_value() || !z.has_value()) { return std::nullopt; }
		return Vector3{ *x, *y, *z };
	}

	/** 비어 있지 않은 저장소 루트 기준 상대 경로면 그 값을, 절대 경로나 루트 밖 경로면 nullopt를 반환한다. */
	std::optional<std::string> ReadRelativePath(const rapidjson::Value& value)
	{
		if (!value.IsString()) { return std::nullopt; }

		const std::string_view text(value.GetString(), value.GetStringLength());
		if (text.empty() || text.find('\0') != std::string_view::npos) { return std::nullopt; }

		const std::filesystem::path path(std::u8string(text.begin(), text.end()));
		const std::filesystem::path normalizedPath = path.lexically_normal();
		const bool goesOutsideRoot = normalizedPath.begin() != normalizedPath.end() && *normalizedPath.begin() == "..";
		if (path.has_root_path() || goesOutsideRoot) { return std::nullopt; }
		return std::string(text);
	}

	/** "Opaque" 또는 "Blend"면 그 값을, 아니면 nullopt를 반환한다. */
	std::optional<MaterialAlphaMode> ReadAlphaMode(const rapidjson::Value& value)
	{
		if (!value.IsString()) { return std::nullopt; }

		const std::string_view text(value.GetString(), value.GetStringLength());
		if (text == "Opaque") { return MaterialAlphaMode::Opaque; }
		if (text == "Blend") { return MaterialAlphaMode::Blend; }
		return std::nullopt;
	}

	/** 읽은 값을 target에 넣는다. 값이 없거나(잘못된 값), target에 이미 값이 있으면(같은 항목 반복) false를 반환한다. */
	template <typename T>
	bool Assign(std::optional<T>& target, std::optional<T> value)
	{
		if (target.has_value() || !value.has_value()) { return false; }
		target = std::move(value);
		return true;
	}
}

bool ReadMaterialFileDefinition(const std::filesystem::path& absolutePath, MaterialFileDefinition& outDefinition)
{
	const std::u8string utf8Path = absolutePath.u8string();
	const std::string_view pathText(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

	std::vector<std::byte> bytes;
	if (!FileHelper::ReadAllBytes(absolutePath, bytes))
	{
		LogFailure(pathText, "could not read material file");
		return false;
	}

	// RapidJSON이 원시 NUL 바이트를 입력 끝으로 처리할 수 있다.
	if (bytes.empty() || std::ranges::find(bytes, std::byte{ 0 }) != bytes.end())
	{
		LogFailure(pathText, "file is empty or contains a raw NUL byte");
		return false;
	}

	rapidjson::Document document;
	document.Parse<rapidjson::kParseValidateEncodingFlag>(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	if (document.HasParseError())
	{
		LogFailure(pathText, std::format("invalid JSON at byte {}: {}", document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError())));
		return false;
	}
	if (!document.IsObject())
	{
		LogFailure(pathText, "JSON root must be an object");
		return false;
	}

	// 사람이 쓰는 고정 형식이라 오타를 조용히 넘기지 않도록 모르는 항목은 실패로 둔다.
	MaterialFileDefinition definition;
	for (const auto& member : document.GetObject())
	{
		const std::string_view name(member.name.GetString(), member.name.GetStringLength());
		const rapidjson::Value& value = member.value;

		bool assigned = false;
		if (name == "base") { assigned = Assign(definition.basePath, ReadRelativePath(value)); }
		else if (name == "baseColorTexture") { assigned = Assign(definition.baseColorTexturePath, ReadRelativePath(value)); }
		else if (name == "normalTexture") { assigned = Assign(definition.normalTexturePath, ReadRelativePath(value)); }
		else if (name == "ormTexture") { assigned = Assign(definition.ormTexturePath, ReadRelativePath(value)); }
		else if (name == "emissiveTexture") { assigned = Assign(definition.emissiveTexturePath, ReadRelativePath(value)); }
		else if (name == "baseColor") { assigned = Assign(definition.baseColor, ReadUnitVector3(value)); }
		else if (name == "alphaMode") { assigned = Assign(definition.alphaMode, ReadAlphaMode(value)); }
		else if (name == "baseColorAlpha") { assigned = Assign(definition.baseColorAlpha, ReadUnitFloat(value)); }
		else if (name == "roughness") { assigned = Assign(definition.roughness, ReadUnitFloat(value)); }
		else if (name == "metallic") { assigned = Assign(definition.metallic, ReadUnitFloat(value)); }
		else
		{
			LogFailure(pathText, std::format("unknown member '{}'", name));
			return false;
		}

		if (!assigned)
		{
			LogFailure(pathText, std::format("'{}' has an invalid value or appears more than once", name));
			return false;
		}
	}

	outDefinition = std::move(definition);
	return true;
}
