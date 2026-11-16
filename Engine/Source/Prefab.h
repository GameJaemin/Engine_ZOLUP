#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct Material;
struct Model;

struct Prefab
{
	std::shared_ptr<const Model> model;

	// 비어 있으면 모든 메시가 기본 재질을 쓴다. 그 외에는 모델 메시와 같은 순서이며 nullptr는 기본 재질을 뜻한다.
	std::vector<std::shared_ptr<const Material>> meshMaterialOverrides;
};

struct PrefabMaterialOverride
{
	// 0부터 시작하는 모델 메시 번호다. 실제 메시 범위는 모델을 읽은 뒤 확인한다.
	std::size_t meshIndex;
	std::string baseColorTexturePath;
};

struct PrefabDefinition
{
	std::optional<std::string> modelPath;
	std::vector<PrefabMaterialOverride> materialOverrides;
};

/**
 * 절대 경로에서 프리팹 JSON을 읽고 모델 경로와 재질 오버라이드 설정을 검증한다.
 * 모델과 텍스처 경로는 저장소 루트 기준 UTF-8 상대 경로다.
 * 읽기 또는 검증에 실패하면 outDefinition을 유지하고 false를 반환한다.
 */
[[nodiscard]] bool ReadPrefabDefinition(const std::filesystem::path& absolutePath, PrefabDefinition& outDefinition);
