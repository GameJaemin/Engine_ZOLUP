#include "ObjectID.h"

#include <Windows.h>
#include <objbase.h>

#include <format>

namespace
{
	// 하이픈 4개를 포함한 "8-4-4-4-12" 형식의 길이다.
	constexpr std::size_t TextLength = 36;

	// 앞 16자리는 high, 뒤 16자리는 low에 들어간다.
	constexpr std::size_t HighDigitCount = 16;

	bool IsHyphenPosition(std::size_t index)
	{
		return index == 8 || index == 13 || index == 18 || index == 23;
	}

	std::optional<std::uint64_t> ReadHexDigit(char digit)
	{
		if (digit >= '0' && digit <= '9') { return static_cast<std::uint64_t>(digit - '0'); }
		if (digit >= 'a' && digit <= 'f') { return static_cast<std::uint64_t>(digit - 'a' + 10); }
		if (digit >= 'A' && digit <= 'F') { return static_cast<std::uint64_t>(digit - 'A' + 10); }
		return std::nullopt;
	}
}

ObjectID ObjectID::Generate()
{
	GUID guid{};
	if (FAILED(CoCreateGuid(&guid))) { return {}; }

	// Windows GUID 문자열 표기와 같은 자리 순서로 담는다.
	ObjectID id;
	id.high = (static_cast<std::uint64_t>(guid.Data1) << 32) | (static_cast<std::uint64_t>(guid.Data2) << 16) | guid.Data3;
	for (const unsigned char byte : guid.Data4)
	{
		id.low = (id.low << 8) | byte;
	}
	return id;
}

std::optional<ObjectID> ObjectID::Parse(std::string_view text)
{
	if (text.size() != TextLength) { return std::nullopt; }

	ObjectID id;
	std::size_t digitCount = 0;
	for (std::size_t index = 0; index < text.size(); ++index)
	{
		if (IsHyphenPosition(index))
		{
			if (text[index] != '-') { return std::nullopt; }
			continue;
		}

		const std::optional<std::uint64_t> value = ReadHexDigit(text[index]);
		if (!value.has_value()) { return std::nullopt; }

		std::uint64_t& part = digitCount < HighDigitCount ? id.high : id.low;
		part = (part << 4) | value.value();
		++digitCount;
	}

	if (!id.IsValid()) { return std::nullopt; }
	return id;
}

std::string ObjectID::ToString() const
{
	return std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
		high >> 32, (high >> 16) & 0xFFFF, high & 0xFFFF, low >> 48, low & 0xFFFFFFFFFFFF);
}

std::size_t std::hash<ObjectID>::operator()(const ObjectID& id) const noexcept
{
	// 두 값 모두 무작위라 합치기만 해도 고르게 퍼진다.
	return std::hash<std::uint64_t>{}(id.high ^ id.low);
}
