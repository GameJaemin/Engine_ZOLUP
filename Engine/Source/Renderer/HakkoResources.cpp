#include "HakkoResources.h"

#include "Asset.h"
#include "Core/Log.h"
#include "Mesh/StaticMesh.h"
#include "Paths.h"
#include "Renderer/Material.h"
#include "Texture/Texture.h"

#include "DataHub.h"
#include "DataHub_Interface.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	// Hakko에 등록할 에셋 리소스 이름이다. 경로 대신 에셋 객체의 실행 중 번호를 써서, 파일 없이 만든 에셋도 등록할 수 있다.
	// 같은 파일은 AssetManager가 같은 객체로 돌려주므로 번호가 같으면 같은 리소스다.
	std::string MakeHakkoAssetName(std::string_view kind, const Asset& asset)
	{
		return std::format("Asset.{}.{}", kind, asset.GetInstanceID());
	}

	// Hakko의 텍스처와 메시 이름은 wchar_t 문자열이다. 위 이름은 ASCII 글자만 쓰므로 글자마다 그대로 넓힌다.
	std::wstring ToWideName(const std::string& name)
	{
		return std::wstring(name.begin(), name.end());
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

		// 같은 파일도 형식이 다르면 서로 다른 Texture 객체라 번호만으로 구분된다.
		const std::string hakkoName = MakeHakkoAssetName("Texture", texture);
		const std::wstring name = ToWideName(hakkoName);

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
			// 파일 없이 만든 텍스처는 경로가 비어 있어 이름과 경로를 함께 남긴다.
			Log::Write(LogLevel::Error, "RenderScene", std::format("Failed to register texture {} ('{}')", hakkoName, texture.GetPath()));
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
	const std::string hakkoName = MakeHakkoAssetName("StaticMesh", mesh);
	const std::wstring name = ToWideName(hakkoName);
	if (Hakko::DataHub::GetMeshId(name) != 0) { return true; }

	// 스킨 메시는 정점마다 뼈 번호와 가중치를 더한 스킨 정점으로 등록한다. skinWeights는 vertices와 같은 개수와 순서다.
	const bool isSkinned = !mesh.skinWeights.empty();

	Hakko::DataHub::Mesh renderMesh{};
	renderMesh.vertexType = isSkinned ? Hakko::DataHub::VertexType_Skinned : Hakko::DataHub::VertexType_Static;
	if (isSkinned) { renderMesh.verticesSkinned.reserve(mesh.vertices.size()); }
	else { renderMesh.verticesStatic.reserve(mesh.vertices.size()); }

	for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
	{
		const MeshVertex& vertex = mesh.vertices[i];

		Hakko::DataHub::VertexStatic renderVertex{};
		renderVertex.Pos = { vertex.position.x, vertex.position.y, vertex.position.z };
		renderVertex.UV = { vertex.uv.x, vertex.uv.y };
		renderVertex.Normal = { vertex.normal.x, vertex.normal.y, vertex.normal.z };
		renderVertex.Tangent = { vertex.tangent.x, vertex.tangent.y, vertex.tangent.z };
		// 엔진 정점에는 색상이 없으므로 흰색으로 채운다.
		renderVertex.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

		if (!isSkinned)
		{
			renderMesh.verticesStatic.push_back(renderVertex);
			continue;
		}

		// 스킨 정점은 일반 정점을 상속하므로 공통 칸을 복사한 뒤 뼈 칸만 채운다.
		Hakko::DataHub::VertexSkinned skinnedVertex{};
		static_cast<Hakko::DataHub::VertexStatic&>(skinnedVertex) = renderVertex;
		for (std::size_t c = 0; c < 4; ++c)
		{
			skinnedVertex.BoneIdx[c] = mesh.skinWeights[i].joints[c];
			skinnedVertex.BoneWeight[c] = mesh.skinWeights[i].weights[c];
		}

		renderMesh.verticesSkinned.push_back(skinnedVertex);
	}

	// Hakko의 인덱스 타입이 std::uint32_t로 전환된 것을 전제로 한다.
	renderMesh.indices = mesh.indices;

	const Hakko::DataHub::MeshID meshId = Hakko::DataHub::CreateMesh(name.c_str(), std::move(renderMesh));

	if (meshId == 0)
	{
		Log::Write(LogLevel::Error, "RenderScene", std::format("Failed to register static mesh {} ('{}')", hakkoName, mesh.GetPath()));

		return false;
	}

	Hakko::DataHub::RTB_UpdateResources upload{};
	upload.command = Hakko::DataHub::RTBCMD_Upload_Resource;
	Hakko::DataHub::SendRTB(upload);

	return true;
}

Hakko::DataHub::MeshID HakkoResources::FindStaticMeshId(const StaticMesh& mesh)
{
	return Hakko::DataHub::GetMeshId(ToWideName(MakeHakkoAssetName("StaticMesh", mesh)));
}

bool HakkoResources::PrepareTextureProp(const Material* material, Hakko::DataHub::TexPropID& outTexturePropID)
{
	outTexturePropID = 0;

	// 재질 미지정은 오류용 조합 0을 사용한다.
	if (material == nullptr) { return true; }

	// 색상 텍스처가 없으면 흰색을 곱해 재질의 기본 색상 계수를 그대로 적용한다.
	Hakko::DataHub::TexID baseColorTextureID = RESERVED_TEX_NONE_DIFFUSE_ID;
	std::string name = "__DEFAULT_BASE_COLOR";
	if (material->parameters.baseColorTexture != nullptr)
	{
		const Texture& texture = *material->parameters.baseColorTexture;
		if (!PrepareTexture(texture, baseColorTextureID)) { return false; }
		name = MakeHakkoAssetName("Texture", texture);
	}

	// 노멀 텍스처가 없으면 평평한 기본 노멀을 쓰고 조합 이름도 색 텍스처 조각만 쓴다.
	// 있으면 이름에 노멀 조각을 더해, 색이 같고 노멀만 다른 재질이 같은 조합을 쓰지 않게 한다.
	Hakko::DataHub::TexID normalTextureID = RESERVED_TEX_NONE_NORMAL_ID;
	if (material->parameters.normalTexture != nullptr)
	{
		const Texture& normalTexture = *material->parameters.normalTexture;
		if (!PrepareTexture(normalTexture, normalTextureID)) { return false; }
		name += "|N=" + MakeHakkoAssetName("Texture", normalTexture);
	}

	// ORM 텍스처가 없으면 흰색(모두 1)을 써서 차폐 없이 거칠기와 금속성 계수를 그대로 적용한다.
	// 있으면 이름에 ORM 조각을 더해, 색과 노멀이 같고 ORM만 다른 재질이 같은 조합을 쓰지 않게 한다.
	// 같은 텍스처가 여러 칸에 쓰일 수 있어 N=, O=, E= 표시로 자리를 구분한다.
	Hakko::DataHub::TexID ormTextureID = RESERVED_TEX_NONE_DIFFUSE_ID;
	if (material->parameters.ormTexture != nullptr)
	{
		const Texture& ormTexture = *material->parameters.ormTexture;
		if (!PrepareTexture(ormTexture, ormTextureID)) { return false; }
		name += "|O=" + MakeHakkoAssetName("Texture", ormTexture);
	}

	Hakko::DataHub::TexID emissiveTextureID = RESERVED_TEX_NONE_EMISSION_ID;
	if (material->parameters.emissiveTexture != nullptr)
	{
		const Texture& emissiveTexture = *material->parameters.emissiveTexture;
		if (!PrepareTexture(emissiveTexture, emissiveTextureID)) { return false; }
		name += "|E=" + MakeHakkoAssetName("Texture", emissiveTexture);
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
	prop.texId[3] = emissiveTextureID;

	const Hakko::DataHub::TexPropID texturePropID = Hakko::DataHub::CreateTexProp(name.c_str(), prop);
	if (texturePropID == 0)
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to register texture combination: " + name);
		return false;
	}

	Hakko::DataHub::RTB_UpdateResources upload{};
	upload.command = Hakko::DataHub::RTBCMD_Upload_Prop;
	Hakko::DataHub::SendRTB(upload);

	outTexturePropID = texturePropID;

	return true;
}

bool HakkoResources::PrepareSkyboxTexture(const std::array<const Texture*, 6>& faces, Hakko::DataHub::TexID& outTextureID)
{
	outTextureID = 0;

	const Texture* firstFace = faces.front();
	if (firstFace == nullptr)
	{
		Log::Write(LogLevel::Error, "RenderScene", "Skybox is missing the +X face");
		return false;
	}

	const std::uint32_t size = firstFace->GetWidth();
	const TextureFormat format = firstFace->GetFormat();
	if (size == 0 ||
		(format != TextureFormat::RGBA8SRGB && format != TextureFormat::RGBA8UNorm))
	{
		Log::Write(LogLevel::Error, "RenderScene", "Invalid skybox size or format");
		return false;
	}

	for (std::size_t index = 0; index < faces.size(); ++index)
	{
		const Texture* face = faces[index];
		if (face == nullptr ||
			face->GetWidth() != size || face->GetHeight() != size ||
			face->GetFormat() != format)
		{
			Log::Write(LogLevel::Error, "RenderScene", "Invalid skybox face at index " + std::to_string(index));
			return false;
		}
	}

	// 면 순서대로 번호를 이어 붙인다. 번호는 숫자뿐이라 경계를 따로 표시할 필요가 없다.
	// 형식은 면 텍스처 객체가 정하므로 이름에 넣지 않는다.
	std::string skyboxName = "Asset.Skybox";
	for (const Texture* face : faces)
	{
		skyboxName += "." + std::to_string(face->GetInstanceID());
	}
	const std::wstring name = ToWideName(skyboxName);

	const Hakko::DataHub::TexID existingID = Hakko::DataHub::GetTexId(name);
	if (existingID != 0)
	{
		outTextureID = existingID;
		return true;
	}

	// RGBA8의 한 줄과 한 면의 크기를 바이트 단위로 계산한다.
	constexpr std::uint32_t maxPitch = (std::numeric_limits<std::uint32_t>::max)();
	const std::uint64_t rowPitch = static_cast<std::uint64_t>(size) * 4;
	if (rowPitch > maxPitch)
	{
		Log::Write(LogLevel::Error, "RenderScene", "Skybox row pitch exceeds the Hakko limit");
		return false;
	}

	const std::uint64_t slicePitch = rowPitch * size;
	if (slicePitch > maxPitch)
	{
		Log::Write(LogLevel::Error, "RenderScene", "Skybox slice pitch exceeds the Hakko limit");
		return false;
	}

	const Hakko::DataHub::TextureReadFormat readFormat = format == TextureFormat::RGBA8SRGB ? Hakko::DataHub::TextureReadFormat::RGBA8_SRGB : Hakko::DataHub::TextureReadFormat::RGBA8_UNorm;

	Hakko::DataHub::Texture cube{};
	cube.data.format = readFormat;
	cube.data.textureType = Hakko::DataHub::TextureType_Cube;
	cube.data.width = size;
	cube.data.height = size;
	cube.data.arraySize = static_cast<std::uint32_t>(faces.size());
	cube.usage = Hakko::DataHub::TextureUsage_ReadOnly;

	if (slicePitch > cube.rawData.max_size() / faces.size())
	{
		Log::Write(LogLevel::Error, "RenderScene", "Skybox pixel data is too large");
		return false;
	}

	const std::uint32_t faceRowPitch = static_cast<std::uint32_t>(rowPitch);
	const std::uint32_t faceSlicePitch = static_cast<std::uint32_t>(slicePitch);
	cube.rawData.reserve(static_cast<std::size_t>(slicePitch) * faces.size());
	cube.subresources.reserve(faces.size());

	for (const Texture* face : faces)
	{
		// 현재 면의 픽셀을 붙이기 전의 바이트 수가 그 면의 시작 위치다.
		const std::size_t faceOffset = cube.rawData.size();
		const Hakko::DataHub::SubresourceData subresource{ .offset = faceOffset, .rowPitch = faceRowPitch, .slicePitch = faceSlicePitch };
		cube.subresources.push_back(subresource);

		const std::vector<std::byte>& pixels = face->GetPixels();
		cube.rawData.insert(cube.rawData.end(), pixels.begin(), pixels.end());
	}

	const Hakko::DataHub::TexID textureID = Hakko::DataHub::CreateTexture(name.c_str(), std::move(cube));
	if (textureID == 0)
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to register skybox texture");
		return false;
	}

	Hakko::DataHub::RTB_UpdateResources upload{};
	upload.command = Hakko::DataHub::RTBCMD_Upload_Resource;
	Hakko::DataHub::SendRTB(upload);

	outTextureID = textureID;

	return true;
}

bool HakkoResources::PreparePostProcessShader(const std::string& shaderPath, Hakko::DataHub::DrawPropID& outDrawPropID)
{
	outDrawPropID = 0;

	// 경로 키로 이름을 정해 같은 셰이더를 다시 요청하면 등록된 그리기 설정을 재사용한다.
	const std::string drawPropName = "__POST_PROCESS|" + Paths::MakeAssetKey(shaderPath);
	const Hakko::DataHub::DrawPropID existingDrawPropID = Hakko::DataHub::GetDrawPropId(drawPropName);
	if (existingDrawPropID != 0)
	{
		outDrawPropID = existingDrawPropID;
		return true;
	}

	std::filesystem::path absolutePath;
	if (!Paths::GetAbsolutePath(shaderPath, absolutePath))
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to resolve post process shader path: " + shaderPath);
		return false;
	}

	// 셰이더 등록 후 그리기 설정 등록만 실패했던 경로는 셰이더를 다시 등록하지 않고 재사용한다.
	const std::wstring shaderName = absolutePath.wstring();
	Hakko::DataHub::ShaderID pixelShaderID = Hakko::DataHub::GetShaderId(shaderName);
	if (pixelShaderID == 0)
	{
		pixelShaderID = Hakko::DataHub::RegisterShader(shaderName.c_str(), Hakko::DataHub::ShaderType_PS);
	}
	if (pixelShaderID == 0)
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to register post process shader: " + shaderPath);
		return false;
	}

	// 렌더러 기본 후처리와 같은 설정이다. 화면 전체 삼각형으로 그리고 깊이와 블렌드는 쓰지 않는다.
	Hakko::DataHub::DrawProp prop{};
	prop.depthUsage = Hakko::DataHub::DrawProp_DepthUsage_Screen;
	prop.cullType = Hakko::DataHub::DrawProp_CullType_None;
	prop.blendType = Hakko::DataHub::DrawProp_BlendType_None;
	prop.vertexType = Hakko::DataHub::VertexType_Simple;
	prop.VSId = RESERVED_SHADER_PP_VS_ID;
	prop.PSId = pixelShaderID;

	const Hakko::DataHub::DrawPropID drawPropID = Hakko::DataHub::CreateDrawProp(drawPropName.c_str(), prop);
	if (drawPropID == 0)
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to register post process draw setting: " + shaderPath);
		return false;
	}

	// 셰이더(자원)와 그리기 설정(Prop)을 모두 이번 프레임에 올린다.
	Hakko::DataHub::RTB_UpdateResources upload{};
	upload.command = Hakko::DataHub::RTBCMD_Upload;
	Hakko::DataHub::SendRTB(upload);

	outDrawPropID = drawPropID;
	return true;
}
