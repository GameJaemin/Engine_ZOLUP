#include "AudioClip.h"

#include <utility>

AudioClip::AudioClip(std::string name, std::string path, std::vector<float> samples, std::uint32_t channelCount, std::uint32_t sampleRate)
	: Asset(std::move(name), std::move(path)), samples(std::move(samples)), channelCount(channelCount), sampleRate(sampleRate)
{
}
