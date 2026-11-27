#pragma once

#include "Core/Math/Vector3.h"
#include "Renderer/Material.h"

#include <filesystem>
#include <optional>
#include <string>

/** 재질 파일(.material.json)에 적힌 값이다. 비어 있는 항목은 base 재질의 값(base가 없으면 MaterialParameters 기본값)을 쓴다. */
struct MaterialFileDefinition
{
	// 값을 복사해 올 재질의 경로다.
	std::optional<std::string> basePath;
	// 저장소 루트 기준 텍스처 경로다. 형식은 칸마다 정해져 있다(색과 발광은 sRGB, 노멀과 ORM은 UNorm).
	std::optional<std::string> baseColorTexturePath;
	std::optional<std::string> normalTexturePath;
	std::optional<std::string> ormTexturePath;
	std::optional<std::string> emissiveTexturePath;
	// 성분마다 0부터 1까지의 선형 RGB다.
	std::optional<Vector3> baseColor;
	std::optional<MaterialAlphaMode> alphaMode;
	// 아래 세 값은 0부터 1까지다.
	std::optional<float> baseColorAlpha;
	std::optional<float> roughness;
	std::optional<float> metallic;
};

/**
 * 절대 경로의 재질 파일을 읽고 검사한다. 에셋은 불러오지 않는다.
 * 모르는 항목, 같은 항목의 반복, 타입이나 범위가 틀린 값이 있으면 로그를 남기고 outDefinition을 유지한 채 false를 반환한다.
 */
bool ReadMaterialFileDefinition(const std::filesystem::path& absolutePath, MaterialFileDefinition& outDefinition);
