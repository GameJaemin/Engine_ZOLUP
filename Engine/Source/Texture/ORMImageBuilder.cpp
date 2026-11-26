#include "ORMImageBuilder.h"

#include "Image/Image.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace
{
	bool IsValidImage(const Image& image)
	{
		if (image.width == 0 || image.height == 0) { return false; }

		const std::uint64_t pixelCount = static_cast<std::uint64_t>(image.width) * image.height;
		if (pixelCount > (std::numeric_limits<std::size_t>::max)() / 4) { return false; }

		return image.pixels.size() == static_cast<std::size_t>(pixelCount) * 4;
	}
}

bool ORMImageBuilder::Create(const Image* occlusion, const Image* metallicRoughness, Image& outImage)
{
	if (occlusion == nullptr && metallicRoughness == nullptr) { return false; }
	if (occlusion != nullptr && !IsValidImage(*occlusion)) { return false; }
	if (metallicRoughness != nullptr && !IsValidImage(*metallicRoughness)) { return false; }

	if (occlusion != nullptr && metallicRoughness != nullptr)
	{
		if (occlusion->width != metallicRoughness->width || occlusion->height != metallicRoughness->height) { return false; }
	}

	const Image& source = occlusion != nullptr ? *occlusion : *metallicRoughness;
	Image result;
	result.width = source.width;
	result.height = source.height;
	const std::size_t byteCount = source.pixels.size();
	// 미설정 채널은 선형 값 1로 두어 차폐를 없애고 재질 계수를 그대로 유지한다.
	result.pixels.resize(byteCount, std::byte{ 255 });

	for (std::size_t offset = 0; offset < byteCount; offset += 4)
	{
		if (occlusion != nullptr)
		{
			result.pixels[offset] = occlusion->pixels[offset];
		}

		if (metallicRoughness != nullptr)
		{
			result.pixels[offset + 1] = metallicRoughness->pixels[offset + 1];
			result.pixels[offset + 2] = metallicRoughness->pixels[offset + 2];
		}
	}

	outImage = std::move(result);
	return true;
}
