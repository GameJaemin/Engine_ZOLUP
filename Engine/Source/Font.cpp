#include "Font.h"

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

Font::Font(std::string newName, std::string newPath, std::vector<std::byte> fontBytes)
	: Asset(std::move(newName), std::move(newPath)), bytes(std::move(fontBytes))
{
}

std::span<const std::byte> Font::GetBytes() const
{
	return bytes;
}
