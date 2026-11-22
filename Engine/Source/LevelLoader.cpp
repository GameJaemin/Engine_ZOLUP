#include "LevelLoader.h"

#include "AssetManager.h"
#include "Core/FileHelper.h"
#include "Core/Log.h"
#include "Core/Math/Transform.h"
#include "Prefab.h"
#include "World.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	bool IsFloatArray(const rapidjson::Value& value, rapidjson::SizeType size)
	{
		if (!value.IsArray() || value.Size() != size) { return false; }

		// Transform은 float를 사용하므로 변환 전에 double로 읽어 범위를 확인한다.
		constexpr double MaxFloat = (std::numeric_limits<float>::max)();
		for (const rapidjson::Value& entry : value.GetArray())
		{
			if (!entry.IsNumber()) { return false; }

			const double number = entry.GetDouble();
			if (!std::isfinite(number) || std::abs(number) > MaxFloat) { return false; }
		}

		return true;
	}

	std::string_view ValidateTransform(const rapidjson::Value& fields)
	{
		if (!fields.IsObject()) { return ".transform must be an object"; }

		const auto position = fields.FindMember("position");
		if (position == fields.MemberEnd() || !IsFloatArray(position->value, 3)) { return ".transform.position must contain three finite floats"; }

		const auto rotation = fields.FindMember("rotation");
		if (rotation == fields.MemberEnd() || !IsFloatArray(rotation->value, 4)) { return ".transform.rotation must contain four finite floats"; }

		// Transform은 단위 쿼터니언을 요구한다. 손으로 입력한 값의 반올림 오차는 허용한다.
		constexpr double UnitLengthTolerance = 0.0001;
		double lengthSquared = 0.0;
		for (const rapidjson::Value& entry : rotation->value.GetArray())
		{
			const double component = entry.GetDouble();
			lengthSquared += component * component;
		}
		if (std::abs(std::sqrt(lengthSquared) - 1.0) > UnitLengthTolerance) { return ".transform.rotation must be a unit quaternion"; }

		const auto scale = fields.FindMember("scale");
		if (scale == fields.MemberEnd() || !IsFloatArray(scale->value, 3)) { return ".transform.scale must contain three finite floats"; }

		return {};
	}

	// 배열 인덱스 뒤에 붙일 오류를 반환한다. 빈 문자열이면 항목이 유효하다.
	std::string_view ValidateObject(const rapidjson::Value& object)
	{
		if (!object.IsObject()) { return " must be an object"; }

		const auto prefab = object.FindMember("prefab");
		const bool hasPrefab = prefab != object.MemberEnd();
		const auto name = object.FindMember("name");
		if (name == object.MemberEnd() && !hasPrefab) { return ".name must be a string"; }
		if (name != object.MemberEnd() && !name->value.IsString()) { return ".name must be a string"; }

		const auto transform = object.FindMember("transform");
		if (transform == object.MemberEnd() && !hasPrefab) { return ".transform must be an object"; }
		if (transform != object.MemberEnd())
		{
			const std::string_view error = ValidateTransform(transform->value);
			if (!error.empty()) { return error; }
		}

		if (hasPrefab)
		{
			if (!prefab->value.IsString()) { return ".prefab must be a string"; }

			const std::string_view prefabPath(prefab->value.GetString(), prefab->value.GetStringLength());
			if (prefabPath.empty() || prefabPath.find('\0') != std::string_view::npos)
			{
				return ".prefab must be non-empty and contain no NUL";
			}

			const std::filesystem::path path(std::u8string(prefabPath.begin(), prefabPath.end()));
			const std::filesystem::path normalizedPath = path.lexically_normal();
			const bool goesOutsideRoot = normalizedPath.begin() != normalizedPath.end() && *normalizedPath.begin() == "..";
			if (path.has_root_path() || goesOutsideRoot) { return ".prefab must be a repository-relative path"; }
		}

		return {};
	}

	// 검증된 맵 항목에서 루트 인스턴스의 배치 값을 읽는다.
	PrefabPlacement ReadPlacement(const rapidjson::Value& entry)
	{
		PrefabPlacement placement;
		const auto name = entry.FindMember("name");
		if (name != entry.MemberEnd())
		{
			placement.name.emplace(name->value.GetString(), name->value.GetStringLength());
		}

		const auto transform = entry.FindMember("transform");
		if (transform != entry.MemberEnd())
		{
			const rapidjson::Value& fields = transform->value;
			const rapidjson::Value::ConstArray position = fields["position"].GetArray();
			const rapidjson::Value::ConstArray rotation = fields["rotation"].GetArray();
			const rapidjson::Value::ConstArray scale = fields["scale"].GetArray();
			placement.transform = Transform{
				.position = { position[0].GetFloat(), position[1].GetFloat(), position[2].GetFloat() },
				.rotation = { rotation[0].GetFloat(), rotation[1].GetFloat(), rotation[2].GetFloat(), rotation[3].GetFloat() },
				.scale = { scale[0].GetFloat(), scale[1].GetFloat(), scale[2].GetFloat() }
			};
		}
		return placement;
	}
}

std::optional<std::vector<GameObject*>> LoadLevel(const std::filesystem::path& path, World& world)
{
	const std::u8string utf8Path = path.u8string();
	const std::string_view pathText(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

	// ----------
	// 파일 읽기와 JSON 파싱
	// ----------
	std::vector<std::byte> bytes;
	if (!FileHelper::ReadAllBytes(path, bytes))
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: could not read file", pathText));
		return std::nullopt;
	}

	// 원시 NUL 바이트가 있으면 파서가 그 위치를 입력 끝으로 볼 수 있어 미리 거부한다.
	if (bytes.empty() || std::ranges::find(bytes, std::byte{0}) != bytes.end())
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: file is empty or contains a raw NUL byte", pathText));
		return std::nullopt;
	}

	rapidjson::Document document;
	document.Parse<rapidjson::kParseValidateEncodingFlag>(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	if (document.HasParseError())
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: invalid JSON at byte {}: {}", pathText, document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError())));
		return std::nullopt;
	}

	// ----------
	// 맵 구조와 객체 데이터 검증
	// ----------
	if (!document.IsObject())
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: JSON root must be an object", pathText));
		return std::nullopt;
	}

	const auto objects = document.FindMember("objects");
	if (objects == document.MemberEnd() || !objects->value.IsArray())
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: objects must be an array", pathText));
		return std::nullopt;
	}

	// 뒤쪽 항목의 검증 실패로 일부 객체만 등록되는 일을 막기 위해 배열 전체를 먼저 검증한다.
	const rapidjson::Value::Array entries = objects->value.GetArray();
	for (rapidjson::SizeType i = 0; i < entries.Size(); ++i)
	{
		const std::string_view error = ValidateObject(entries[i]);
		if (!error.empty())
		{
			Log::Write(LogLevel::Error, "Level", std::format("{}: objects[{}]{}", pathText, i, error));
			return std::nullopt;
		}
	}

	// ----------
	// 참조된 프리팹 로드
	// ----------
	std::vector<std::shared_ptr<const Prefab>> prefabs(entries.Size());
	for (rapidjson::SizeType i = 0; i < entries.Size(); ++i)
	{
		const rapidjson::Value& entry = entries[i];
		const auto prefab = entry.FindMember("prefab");
		if (prefab == entry.MemberEnd()) { continue; }

		const std::string_view prefabPath(prefab->value.GetString(), prefab->value.GetStringLength());
		prefabs[i] = AssetManager::Get().LoadPrefab(prefabPath);
		if (prefabs[i] == nullptr)
		{
			Log::Write(LogLevel::Error, "Level", std::format("{}: objects[{}].prefab could not load {}", pathText, i, prefabPath));
			return std::nullopt;
		}
	}

	// ----------
	// 월드에 루트 객체 복원
	// ----------
	std::vector<GameObject*> loadedObjects;
	loadedObjects.reserve(entries.Size());
	for (rapidjson::SizeType i = 0; i < entries.Size(); ++i)
	{
		const PrefabPlacement placement = ReadPlacement(entries[i]);
		if (prefabs[i] != nullptr)
		{
			loadedObjects.push_back(world.InstantiatePrefab(*prefabs[i], placement));
			continue;
		}

		// 프리팹이 없는 항목은 검증 단계에서 이름과 배치 Transform을 요구한다.
		loadedObjects.push_back(world.CreateGameObject(placement.name, *placement.transform));
	}
	return loadedObjects;
}
