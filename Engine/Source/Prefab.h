#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

struct Model;

struct Prefab
{
	std::shared_ptr<const Model> model;
};

/**
 * 절대 경로에서 프리팹 JSON을 읽고 model 항목을 검증한다.
 * 성공하면 true를 반환하고, 저장소 루트 기준 UTF-8 상대 경로를 outModelPath에 저장한다.
 * model을 생략한 경우에도 성공하며, outModelPath를 nullopt로 비운다.
 * 읽기 또는 검증에 실패하면 outModelPath를 유지하고 false를 반환한다.
 */
[[nodiscard]] bool ReadPrefabModelPath(const std::filesystem::path& absolutePath, std::optional<std::string>& outModelPath);
