#include "HakkoResources.h"

#include "Core/Log.h"
#include "Mesh/StaticMesh.h"
#include "Paths.h"
#include "Renderer/Material.h"
#include "Texture/Texture.h"

#include "DataHub.h"
#include "DataHub_Interface.h"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>

namespace
{
	// Hakko의 메시와 텍스처 이름에 쓸 UTF-8 경로 키를 wchar_t 문자열로 변환한다.
	std::wstring MakeHakkoAssetName(const std::string& path)
	{
		const std::string assetKey = Paths::MakeAssetKey(path);
		const std::u8string assetKeyUtf8(assetKey.begin(), assetKey.end());
		const std::filesystem::path keyPath(assetKeyUtf8);
		return keyPath.generic_wstring();
	}

	/** 텍스처 조합 이름에 쓸 "경로 키|형식" 조각을 만든다. 지원하지 않는 형식이면 출력값을 유지하고 false를 반환한다. */
	bool MakeTexturePropNamePart(const Texture& texture, std::string& outName)
	{
		std::string name = Paths::MakeAssetKey(texture.GetPath());

		switch (texture.GetFormat())
		{
		case TextureFormat::RGBA8UNorm:
			name += "|RGBA8UNorm";
			break;
		case TextureFormat::RGBA8SRGB:
			name += "|RGBA8SRGB";
			break;
		default:
			return false;
		}

		outName = std::move(name);
		return true;
	}

	bool PrepareTexture(const Texture& texture, Hakko::DataHub::TexID& outTextureID)
	{
		outTextureID = 0;

		const TextureFormat format = texture.GetFormat();
		switch (format)
		{
		case TextureFormat::RGBA8UNorm:
		case TextureFormat::RGBA8SRGB:
			break;
		default:
			Log::Write(LogLevel::Error, "RenderScene", "Unsupported texture format");
			return false;
		}

		if (texture.GetPath().empty())
		{
			Log::Write(LogLevel::Error, "RenderScene", "Texture registration requires an asset path");
			return false;
		}
		if (texture.GetWidth() == 0 || texture.GetHeight() == 0)
		{
			Log::Write(LogLevel::Error, "RenderScene", "Texture dimensions must be nonzero");
			return false;
		}

		// RGBA8의 행과 전체 이미지 크기를 바이트 단위로 계산한다.
		const std::uint64_t rowPitch = static_cast<std::uint64_t>(texture.GetWidth()) * 4;
		const std::uint32_t maxPitch = (std::numeric_limits<std::uint32_t>::max)();
		if (rowPitch > maxPitch)
		{
			Log::Write(LogLevel::Error, "RenderScene", "Texture row pitch exceeds the Hakko limit");
			return false;
		}

		const std::uint64_t slicePitch = rowPitch * texture.GetHeight();
		if (slicePitch > maxPitch)
		{
			Log::Write(LogLevel::Error, "RenderScene", "Texture slice pitch exceeds the Hakko limit");
			return false;
		}
		if (slicePitch != texture.GetPixels().size())
		{
			Log::Write(LogLevel::Error, "RenderScene", "Texture pixel size does not match its dimensions");
			return false;
		}

		const std::string& path = texture.GetPath();
		std::wstring name = MakeHakkoAssetName(path);

		// 같은 파일도 GPU 형식이 다르면 별도 자원으로 구분한다.
		switch (format)
		{
		case TextureFormat::RGBA8UNorm:
			name += L"|RGBA8UNorm";
			break;
		case TextureFormat::RGBA8SRGB:
			name += L"|RGBA8SRGB";
			break;
		default:
			return false;
		}

		const Hakko::DataHub::TexID existingTextureID = Hakko::DataHub::GetTexId(name);
		if (existingTextureID != 0)
		{
			outTextureID = existingTextureID;
			return true;
		}

		Hakko::DataHub::Texture renderTexture{};

		switch (format)
		{
		case TextureFormat::RGBA8UNorm:
			renderTexture.data.format = Hakko::DataHub::TextureReadFormat::RGBA8_UNorm;
			break;
		case TextureFormat::RGBA8SRGB:
			renderTexture.data.format = Hakko::DataHub::TextureReadFormat::RGBA8_SRGB;
			break;
		default:
			return false;
		}

		renderTexture.data.textureType = Hakko::DataHub::TextureType_2D;
		renderTexture.data.width = texture.GetWidth();
		renderTexture.data.height = texture.GetHeight();
		renderTexture.data.depth = 1;
		renderTexture.data.mipLevels = 1;
		renderTexture.data.arraySize = 1;
		renderTexture.usage = Hakko::DataHub::TextureUsage_ReadOnly;

		// 공유 CPU 에셋의 픽셀은 유지하고 업로드용 복사본을 만든다.
		renderTexture.rawData = texture.GetPixels();

		// 원본 해상도의 2D 이미지 한 장이며 픽셀 배열의 처음부터 읽는다.
		renderTexture.subresources.push_back({
			.offset = 0,
			.rowPitch = static_cast<std::uint32_t>(rowPitch),
			.slicePitch = static_cast<std::uint32_t>(slicePitch)
		});

		const Hakko::DataHub::TexID textureID = Hakko::DataHub::CreateTexture(name.c_str(), std::move(renderTexture));
		if (textureID == 0)
		{
			Log::Write(LogLevel::Error, "RenderScene", "Failed to register texture: " + path);
			return false;
		}

		// 다른 자원 요청 비트를 유지하면서 이번 프레임의 업로드를 요청한다.
		Hakko::DataHub::RTB_UpdateResources upload{};
		upload.command = Hakko::DataHub::RTBCMD_Upload_Resource;
		Hakko::DataHub::SendRTB(upload);

		outTextureID = textureID;
		return true;
	}
}

bool HakkoResources::PrepareStaticMesh(const StaticMesh& mesh)
{
	const std::string& path = mesh.GetPath();
	if (path.empty())
	{
		Log::Write(LogLevel::Error, "RenderScene", "Static mesh registration requires an asset path");
		return false;
	}

	const std::wstring name = MakeHakkoAssetName(path);
	if (Hakko::DataHub::GetMeshId(name) != 0) { return true; }

	Hakko::DataHub::Mesh renderMesh{};
	renderMesh.vertexType = Hakko::DataHub::VertexType_Static;
	renderMesh.verticesStatic.reserve(mesh.vertices.size());

	for (const MeshVertex& vertex : mesh.vertices)
	{
		Hakko::DataHub::VertexStatic renderVertex{};
		renderVertex.Pos = { vertex.position.x, vertex.position.y, vertex.position.z };
		renderVertex.UV = { vertex.uv.x, vertex.uv.y };
		renderVertex.Normal = { vertex.normal.x, vertex.normal.y, vertex.normal.z };
		renderVertex.Tangent = { vertex.tangent.x, vertex.tangent.y, vertex.tangent.z };
		// 엔진 정점에는 색상이 없으므로 흰색으로 채운다.
		renderVertex.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

		renderMesh.verticesStatic.push_back(renderVertex);
	}

	// Hakko의 인덱스 타입이 std::uint32_t로 전환된 것을 전제로 한다.
	renderMesh.indices = mesh.indices;

	const Hakko::DataHub::MeshID meshId = Hakko::DataHub::CreateMesh(name.c_str(), std::move(renderMesh));

	if (meshId == 0)
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to register static mesh: " + path);

		return false;
	}

	Hakko::DataHub::RTB_UpdateResources upload{};
	upload.command = Hakko::DataHub::RTBCMD_Upload_Resource;
	Hakko::DataHub::SendRTB(upload);

	return true;
}

Hakko::DataHub::MeshID HakkoResources::FindStaticMeshId(const StaticMesh& mesh)
{
	return Hakko::DataHub::GetMeshId(MakeHakkoAssetName(mesh.GetPath()));
}

bool HakkoResources::PrepareTextureProp(const Material* material, Hakko::DataHub::TexPropID& outTexturePropID)
{
	outTexturePropID = 0;

	// 미지정 재질이나 색상 텍스처는 오류용 조합 0을 사용한다.
	if (material == nullptr) { return true; }
	if (material->baseColorTexture == nullptr) { return true; }

	const Texture& texture = *material->baseColorTexture;

	Hakko::DataHub::TexID baseColorTextureID = 0;
	if (!PrepareTexture(texture, baseColorTextureID))
	{
		return false;
	}

	const std::string& path = texture.GetPath();
	std::string name;
	if (!MakeTexturePropNamePart(texture, name)) { return false; }

	// 노멀 텍스처가 없으면 평평한 기본 노멀을 쓰고 조합 이름도 색 텍스처 조각만 쓴다.
	// 있으면 이름에 노멀 조각을 더해, 색이 같고 노멀만 다른 재질이 같은 조합을 쓰지 않게 한다.
	Hakko::DataHub::TexID normalTextureID = RESERVED_TEX_NONE_NORMAL_ID;
	if (material->normalTexture != nullptr)
	{
		const Texture& normalTexture = *material->normalTexture;
		if (!PrepareTexture(normalTexture, normalTextureID)) { return false; }

		std::string normalName;
		if (!MakeTexturePropNamePart(normalTexture, normalName)) { return false; }
		name += "|N=" + normalName;
	}

	// ORM 텍스처가 없으면 흰색(모두 1)을 써서 차폐 없이 거칠기와 금속성 계수를 그대로 적용한다.
	// 있으면 이름에 ORM 조각을 더해, 색과 노멀이 같고 ORM만 다른 재질이 같은 조합을 쓰지 않게 한다.
	// 노멀과 ORM 조각은 형식이 같을 수 있어 N=, O= 표시로 자리를 구분한다.
	Hakko::DataHub::TexID ormTextureID = RESERVED_TEX_NONE_DIFFUSE_ID;
	if (material->ormTexture != nullptr)
	{
		const Texture& ormTexture = *material->ormTexture;
		if (!PrepareTexture(ormTexture, ormTextureID)) { return false; }

		std::string ormName;
		if (!MakeTexturePropNamePart(ormTexture, ormName)) { return false; }
		name += "|O=" + ormName;
	}

	const Hakko::DataHub::TexPropID existingTexturePropID = Hakko::DataHub::GetTexPropId(name);
	if (existingTexturePropID != 0)
	{
		outTexturePropID = existingTexturePropID;
		return true;
	}

	Hakko::DataHub::TextureProp prop{};
	prop.texId[0] = baseColorTextureID;
	prop.texId[1] = normalTextureID;
	prop.texId[2] = ormTextureID;
	prop.texId[3] = RESERVED_TEX_NONE_EMISSION_ID;

	const Hakko::DataHub::TexPropID texturePropID = Hakko::DataHub::CreateTexProp(name.c_str(), prop);
	if (texturePropID == 0)
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to register texture combination: " + path);
		return false;
	}

	Hakko::DataHub::RTB_UpdateResources upload{};
	upload.command = Hakko::DataHub::RTBCMD_Upload_Prop;
	Hakko::DataHub::SendRTB(upload);

	outTexturePropID = texturePropID;

	return true;
}
