#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

/** World 안에서 GameObject를 구분하는 128비트 영속 식별자다. 모든 비트가 0이면 무효다. */
struct ObjectID
{
	// 문자열 표기의 앞 16자리가 high, 뒤 16자리가 low다.
	std::uint64_t high = 0;
	std::uint64_t low = 0;

	/** 새 무작위 ID를 만든다. 생성에 실패하면 무효 ID를 반환한다. */
	[[nodiscard]] static ObjectID Generate();

	/**
	 * "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" 형식의 16진수를 읽는다. 대문자와 소문자를 모두 받는다.
	 * 형식이 다르거나 무효 ID이면 nullopt를 반환한다.
	 */
	[[nodiscard]] static std::optional<ObjectID> Parse(std::string_view text);

	bool IsValid() const noexcept { return high != 0 || low != 0; }

	/** Parse가 읽을 수 있는 소문자 형식으로 바꾼다. */
	std::string ToString() const;

	bool operator==(const ObjectID&) const = default;
};

/** unordered_map의 키로 쓸 수 있게 한다. */
template <>
struct std::hash<ObjectID>
{
	std::size_t operator()(const ObjectID& id) const noexcept;
};
