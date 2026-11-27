#include "AudioClipLoader.h"

#include "AudioClip.h"
#include "Core/FileHelper.h"
#include "Core/Log.h"

#include <miniaudio.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace
{
	struct DecodedAudio
	{
		std::vector<float> samples;
		std::uint32_t channelCount = 0;
		std::uint32_t sampleRate = 0;
	};

	std::optional<DecodedAudio> DecodeWav(std::span<const std::byte> bytes)
	{
		// PCM 형식만 변환하고 원본의 채널 수와 샘플레이트는 유지한다.
		ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
		config.encodingFormat = ma_encoding_format_wav;
		ma_decoder decoder;
		if (ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder) != MA_SUCCESS) { return std::nullopt; }

		// 디코더가 빌린 파일 바이트는 디코더 초기화 해제가 끝날 때까지 유효해야 한다.
		const std::unique_ptr<ma_decoder, decltype(&ma_decoder_uninit)> decoderLifetime(&decoder, ma_decoder_uninit);
		DecodedAudio audio;
		const ma_result formatResult = ma_decoder_get_data_format(&decoder, nullptr, &audio.channelCount, &audio.sampleRate, nullptr, 0);
		if (formatResult != MA_SUCCESS || audio.channelCount == 0 || audio.sampleRate == 0) { return std::nullopt; }

		ma_uint64 frameCount = 0;
		if (ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount) != MA_SUCCESS || frameCount == 0) { return std::nullopt; }
		if (frameCount > audio.samples.max_size() / audio.channelCount) { return std::nullopt; }
		audio.samples.resize(static_cast<std::size_t>(frameCount) * audio.channelCount);

		ma_uint64 framesRead = 0;
		const ma_result readResult = ma_decoder_read_pcm_frames(&decoder, audio.samples.data(), frameCount, &framesRead);
		if (readResult != MA_SUCCESS && readResult != MA_AT_END) { return std::nullopt; }
		if (framesRead != frameCount) { return std::nullopt; }

		return audio;
	}
}

std::shared_ptr<const AudioClip> AudioClipLoader::LoadWavFile(const std::filesystem::path& absolutePath, std::string assetPath)
{
	std::vector<std::byte> bytes;
	if (!FileHelper::ReadAllBytes(absolutePath, bytes) || bytes.empty())
	{
		Log::Write(LogLevel::Error, "AudioClipLoader", std::format("Failed to read WAV or file is empty: {}", assetPath));
		return nullptr;
	}

	std::optional<DecodedAudio> decoded = DecodeWav(bytes);
	if (!decoded)
	{
		Log::Write(LogLevel::Error, "AudioClipLoader", std::format("Failed to decode WAV: {}", assetPath));
		return nullptr;
	}

	const std::u8string nameUtf8 = absolutePath.stem().u8string();
	std::string name(nameUtf8.begin(), nameUtf8.end());
	return std::make_shared<const AudioClip>(std::move(name), std::move(assetPath), std::move(decoded->samples), decoded->channelCount, decoded->sampleRate);
}
