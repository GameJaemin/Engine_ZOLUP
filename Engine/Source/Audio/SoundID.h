#pragma once

#include <cstdint>

/** AudioSystem이 발급하는 재생 ID다. 발급한 시스템에서만 사용한다. */
struct SoundID
{
	std::uint64_t value = 0;

	bool operator==(const SoundID&) const = default;
};
