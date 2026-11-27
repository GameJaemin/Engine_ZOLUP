#include "PlayingSound.h"

#include "AudioClip.h"
#include "Core/Math/Vector3.h"

#include <cstdint>
#include <span>
#include <utility>

PlayingSound::PlayingSound(std::shared_ptr<const AudioClip> newClip, SoundID newSoundID)
	: clip(std::move(newClip)), soundID(newSoundID)
{
}

PlayingSound::~PlayingSound()
{
	if (isSoundInitialized)
	{
		ma_sound_uninit(&sound);
	}

	if (isBufferInitialized)
	{
		ma_audio_buffer_uninit(&buffer);
	}
}

bool PlayingSound::Start(ma_engine& engine)
{
	if (!InitializeSound(engine, MA_SOUND_FLAG_NO_SPATIALIZATION)) { return false; }

	return ma_sound_start(&sound) == MA_SUCCESS;
}

bool PlayingSound::Start(ma_engine& engine, const Vector3& audioPosition, float minDistanceMeters)
{
	if (!InitializeSound(engine, 0)) { return false; }

	SetPosition(audioPosition);
	ma_sound_set_min_distance(&sound, minDistanceMeters);
	return ma_sound_start(&sound) == MA_SUCCESS;
}

bool PlayingSound::InitializeSound(ma_engine& engine, ma_uint32 soundFlags)
{
	if (!clip || isBufferInitialized) { return false; }

	const std::span<const float> samples = clip->GetSamples();
	const std::uint32_t channelCount = clip->GetChannelCount();
	const std::uint32_t sampleRate = clip->GetSampleRate();
	if (channelCount == 0 || channelCount > MA_MAX_CHANNELS || sampleRate == 0) { return false; }
	if (samples.empty() || samples.size() % channelCount != 0) { return false; }

	const ma_uint64 frameCount = samples.size() / channelCount;
	ma_audio_buffer_config config = ma_audio_buffer_config_init(ma_format_f32, channelCount, frameCount, samples.data(), nullptr);
	config.sampleRate = sampleRate;

	if (ma_audio_buffer_init(&config, &buffer) != MA_SUCCESS) { return false; }
	isBufferInitialized = true;

	if (ma_sound_init_from_data_source(&engine, &buffer, soundFlags, nullptr, &sound) != MA_SUCCESS) { return false; }
	isSoundInitialized = true;

	return true;
}

bool PlayingSound::SetPosition(const Vector3& audioPosition)
{
	if (!isSoundInitialized) { return false; }

	ma_sound_set_position(&sound, audioPosition.x, audioPosition.y, audioPosition.z);
	return true;
}

bool PlayingSound::HasEnded() const
{
	return isSoundInitialized && ma_sound_at_end(&sound) == MA_TRUE;
}
