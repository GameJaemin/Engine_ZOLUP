#pragma once

#include "Asset.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

/** OTF 또는 TTF 파일의 원본 데이터를 보관한다. */
class Font : public Asset
{
public:
	Font(std::string newName, std::string newPath, std::vector<std::byte> fontBytes);

	/** 반환한 뷰는 Font가 살아 있고 이동되지 않은 동안 읽을 수 있다. */
	std::span<const std::byte> GetBytes() const;

private:
	std::vector<std::byte> bytes;
};
