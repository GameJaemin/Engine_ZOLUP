#pragma once

#include "Asset.h"
#include "Core/Math/Vector3.h"

#include <memory>
#include <string>
#include <utility>

class Texture;

/** 표면의 알파를 처리하는 방식이다. */
enum class MaterialAlphaMode
{
	Opaque,
	Blend
};

/** 표면 텍스처와 텍스처 값에 곱할 계수다. 값이라 자유롭게 복사해 다른 재질을 만들 때 쓴다. */
struct MaterialParameters
{
	// 색상 텍스처다. nullptr이면 미설정 상태다.
	std::shared_ptr<const Texture> baseColorTexture;

	// 색상 텍스처에 성분별로 곱할 선형 RGB 계수다.
	// 각 성분은 0부터 1까지이며, 흰색이면 텍스처 색상을 유지한다.
	Vector3 baseColor = { 1.0f, 1.0f, 1.0f };

	// 표면을 불투명하게 그릴지, 알파로 배경과 합성할지 지정한다.
	MaterialAlphaMode alphaMode = MaterialAlphaMode::Opaque;

	// Blend에서 색상 텍스처의 알파에 곱하는 선형 계수다. 범위는 0부터 1까지다.
	float baseColorAlpha = 1.0f;

	// 접선 공간 노멀 텍스처다. RGB가 방향 값이라 선형(RGBA8UNorm) 형식이어야 한다.
	// nullptr이면 노멀맵이 없는 정상 상태이며 평평한 기본 노멀을 쓴다.
	std::shared_ptr<const Texture> normalTexture;

	// 차폐-거칠기-금속성(ORM) 텍스처다. R은 차폐, G는 거칠기, B는 금속성이며 선형(RGBA8UNorm) 형식이어야 한다.
	// nullptr이면 ORM 텍스처가 없는 정상 상태이며 차폐 없이 거칠기와 금속성 계수만 쓴다.
	std::shared_ptr<const Texture> ormTexture;

	// 선형 RGB 발광 계수가 반영된 sRGB 텍스처다. nullptr이면 발광이 없다.
	std::shared_ptr<const Texture> emissiveTexture;

	// ORM의 G에 곱할 거칠기 계수다. 범위는 0부터 1까지다.
	// 1이면 텍스처 값을 유지하고, 0이면 최종 거칠기가 0이 된다.
	float roughness = 1.0f;

	// ORM의 B에 곱할 금속성 계수다. 범위는 0부터 1까지다.
	// 1이면 텍스처 값을 유지하고, 0이면 최종 금속성이 0이 된다.
	float metallic = 1.0f;
};

/**
 * 표면을 그리는 방법이다. 이름과 경로는 정체성이라 복사하지 않고, 값은 parameters에 있다.
 * 다른 재질을 만들 때는 parameters를 복사해 새 이름과 경로로 만든다.
 */
class Material : public Asset
{
public:
	/** UTF-8 이름, 저장소 루트 기준 경로와 값을 받는다. 파일에서 다시 만들 수 없는 재질은 빈 경로를 쓴다. */
	Material(std::string newName, std::string newPath, MaterialParameters newParameters)
		: Asset(std::move(newName), std::move(newPath)), parameters(std::move(newParameters))
	{
	}

public:
	MaterialParameters parameters;
};
