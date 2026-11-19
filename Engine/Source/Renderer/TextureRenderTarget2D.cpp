#include "TextureRenderTarget2D.h"

#include "DataHub_Interface.h"

#include <utility>

bool TextureRenderTarget2D::Initialize(std::wstring newName, std::uint32_t newWidth, std::uint32_t newHeight)
{
	if (newName.empty() || newWidth == 0 || newHeight == 0) { return false; }
	if (!name.empty()) { return false; }

	const Hakko::DataHub::ViewTarget target = Hakko::DataHub::CreateViewTarget(newName.c_str(), newWidth, newHeight);
	if (target.width == 0) { return false; }

	Hakko::DataHub::RTB_UpdateResources upload{};
	upload.command = Hakko::DataHub::RTBCMD_Upload_Resource;
	Hakko::DataHub::SendRTB(upload);

	name = std::move(newName);
	width = newWidth;
	height = newHeight;

	return true;
}

bool TextureRenderTarget2D::Shutdown()
{
	if (name.empty()) { return true; }

	if (!Hakko::DataHub::ReleaseTexture(name)) { return false; }

	Hakko::DataHub::RTB_UpdateResources release{};
	release.command = Hakko::DataHub::RTBCMD_Release_Resource;
	Hakko::DataHub::SendRTB(release);

	name.clear();
	width = 0;
	height = 0;

	return true;
}

bool TextureRenderTarget2D::Resize(std::uint32_t newWidth, std::uint32_t newHeight)
{
	if (name.empty() || newWidth == 0 || newHeight == 0) { return false; }
	if (width == newWidth && height == newHeight) { return true; }

	Hakko::DataHub::Texture texture{};
	texture.data.format = Hakko::DataHub::TextureReadFormat::RGBA8_SRGB;
	texture.data.textureType = Hakko::DataHub::TextureType_2D;
	texture.data.width = newWidth;
	texture.data.height = newHeight;
	texture.usage = Hakko::DataHub::TextureUsage_RT;

	if (!Hakko::DataHub::UpdateTexture(name, std::move(texture))) { return false; }

	Hakko::DataHub::RTB_UpdateResources update{};
	update.command = Hakko::DataHub::RTBCMD_Update_Resource;
	Hakko::DataHub::SendRTB(update);

	width = newWidth;
	height = newHeight;

	return true;
}

Hakko::DataHub::TexID TextureRenderTarget2D::FindTextureID() const
{
	if (name.empty()) { return 0; }

	return Hakko::DataHub::GetTexId(name);
}
