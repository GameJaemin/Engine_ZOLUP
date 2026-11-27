#pragma once

#include "Asset.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

/** 디코딩한 PCM 샘플과 원본 채널 수 및 샘플레이트를 보관한다. */
class AudioClip : public Asset
{
public:
	/** 샘플은 프레임마다 채널 순서로 배치하며, 채널 수와 샘플레이트는 0보다 커야 한다. */
	AudioClip(std::string name, std::string path, std::vector<float> samples, std::uint32_t channelCount, std::uint32_t sampleRate);

	/** 반환한 뷰는 클립이 살아 있고 이동되지 않은 동안 읽을 수 있다. */
	std::span<const float> GetSamples() const { return samples; }
	std::uint32_t GetChannelCount() const { return channelCount; }
	/** 초당 PCM 프레임 수(Hz)를 반환한다. */
	std::uint32_t GetSampleRate() const { return sampleRate; }

private:
	// 스테레오는 L, R, L, R 순서다. PCM 프레임 하나에 채널 수만큼 샘플이 있다.
	std::vector<float> samples;
	std::uint32_t channelCount;
	std::uint32_t sampleRate;
};
