#include "AudioSystem.h"

#include "Core/Log.h"

#include <miniaudio.h>

#include <format>

AudioSystem::AudioSystem() = default;

AudioSystem::~AudioSystem()
{
	Shutdown();
}

bool AudioSystem::Initialize()
{
	if (audioEngine)
	{
		Log::Write(LogLevel::Error, "Audio", "Audio engine is already initialized");
		return false;
	}

	std::unique_ptr<ma_engine> newAudioEngine = std::make_unique<ma_engine>();
	// 설정을 nullptr로 넘기면 기본 출력 장치와 그 장치의 채널 수, 샘플 레이트를 사용한다.
	const ma_result result = ma_engine_init(nullptr, newAudioEngine.get());
	if (result != MA_SUCCESS)
	{
		Log::Write(LogLevel::Error, "Audio", std::format("Failed to initialize audio engine: {}", ma_result_description(result)));
		return false;
	}

	audioEngine = std::move(newAudioEngine);
	Log::Write(LogLevel::Info, "Audio", "Audio engine initialized");
	return true;
}

void AudioSystem::Shutdown()
{
	if (!audioEngine) { return; }

	// 장치를 멈추고, 엔진이 관리하던 소리를 함께 해제한다.
	ma_engine_uninit(audioEngine.get());
	audioEngine.reset();
	Log::Write(LogLevel::Info, "Audio", "Audio engine shut down");
}
