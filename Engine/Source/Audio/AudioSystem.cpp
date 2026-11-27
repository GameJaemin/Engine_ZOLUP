#include "AudioSystem.h"

#include "Core/Log.h"
#include "Core/Math/Vector3.h"
#include "PlayingSound.h"

#include <miniaudio.h>

#include <cmath>
#include <format>

namespace
{
	constexpr float centimetersToMeters = 0.01f;

	/** 엔진의 월드 위치(cm, +Z 정면)를 miniaudio 위치(m, -Z 정면)로 변환한다. */
	Vector3 ToAudioPosition(const Vector3& position)
	{
		return Vector3{ position.x * centimetersToMeters, position.y * centimetersToMeters, -position.z * centimetersToMeters };
	}
}

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

	// 재생 객체를 해제할 때는 오디오 엔진이 살아 있어야 한다.
	playingSounds.clear();
	ma_engine_uninit(audioEngine.get());
	audioEngine.reset();
	Log::Write(LogLevel::Info, "Audio", "Audio engine shut down");
}

std::optional<SoundID> AudioSystem::PlaySound2D(std::shared_ptr<const AudioClip> clip)
{
	if (!audioEngine || !clip || nextSoundID == 0) { return std::nullopt; }

	const SoundID soundID{ nextSoundID };
	std::unique_ptr<PlayingSound> sound = std::make_unique<PlayingSound>(std::move(clip), soundID);
	if (!sound->Start(*audioEngine)) { return std::nullopt; }

	playingSounds.push_back(std::move(sound));
	++nextSoundID;
	return soundID;
}

std::optional<SoundID> AudioSystem::PlaySoundAtLocation(std::shared_ptr<const AudioClip> clip, const Vector3& position, float minDistanceCm)
{
	if (!audioEngine || !clip || nextSoundID == 0) { return std::nullopt; }

	const float minDistanceMeters = minDistanceCm * centimetersToMeters;
	if (!std::isfinite(minDistanceMeters) || minDistanceMeters <= 0.0f) { return std::nullopt; }

	const Vector3 audioPosition = ToAudioPosition(position);
	const SoundID soundID{ nextSoundID };
	std::unique_ptr<PlayingSound> sound = std::make_unique<PlayingSound>(std::move(clip), soundID);
	if (!sound->Start(*audioEngine, audioPosition, minDistanceMeters)) { return std::nullopt; }

	playingSounds.push_back(std::move(sound));
	++nextSoundID;
	return soundID;
}

void AudioSystem::StopSound(SoundID soundID)
{
	for (auto iterator = playingSounds.begin(); iterator != playingSounds.end(); ++iterator)
	{
		if ((*iterator)->GetSoundID() != soundID) { continue; }

		// 소멸자가 ma_sound_uninit으로 재생을 멈추고 자원을 해제한다.
		playingSounds.erase(iterator);
		return;
	}
}

void AudioSystem::SetSoundLocation(SoundID soundID, const Vector3& position)
{
	for (const std::unique_ptr<PlayingSound>& sound : playingSounds)
	{
		if (sound->GetSoundID() != soundID) { continue; }

		sound->SetPosition(ToAudioPosition(position));
		return;
	}
}

void AudioSystem::SetListenerPose(const Vector3& position, const Vector3& forward, const Vector3& up)
{
	if (!audioEngine) { return; }

	constexpr ma_uint32 listenerIndex = 0;
	const Vector3 audioPosition = ToAudioPosition(position);
	ma_engine_listener_set_position(audioEngine.get(), listenerIndex, audioPosition.x, audioPosition.y, audioPosition.z);

	// 엔진의 정면은 +Z이고 miniaudio의 기본 정면은 -Z다.
	ma_engine_listener_set_direction(audioEngine.get(), listenerIndex, forward.x, forward.y, -forward.z);
	ma_engine_listener_set_world_up(audioEngine.get(), listenerIndex, up.x, up.y, -up.z);
}

void AudioSystem::Update()
{
	auto iterator = playingSounds.begin();
	while (iterator != playingSounds.end())
	{
		if ((*iterator)->HasEnded())
		{
			iterator = playingSounds.erase(iterator);
			continue;
		}

		++iterator;
	}
}
