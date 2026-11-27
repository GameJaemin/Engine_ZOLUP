#pragma once

#include "SoundID.h"

#include <miniaudio.h>

#include <memory>

class AudioClip;
struct Vector3;

class PlayingSound
{
public:
	PlayingSound(std::shared_ptr<const AudioClip> newClip, SoundID newSoundID);
	~PlayingSound();

	PlayingSound(const PlayingSound&) = delete;
	PlayingSound& operator=(const PlayingSound&) = delete;
	PlayingSound(PlayingSound&&) = delete;
	PlayingSound& operator=(PlayingSound&&) = delete;

	/** 공간 효과 없이 한 번 재생한다. 실패한 인스턴스는 소멸할 때 정리한다. */
	bool Start(ma_engine& engine);

	/** miniaudio 좌표계의 위치(m)에서 한 번 재생한다. 거리 감쇠는 minDistanceMeters를 넘으면 시작한다. 실패한 인스턴스는 소멸할 때 정리한다. */
	bool Start(ma_engine& engine, const Vector3& audioPosition, float minDistanceMeters);

	/** 초기화된 소리의 위치를 바꾼다. 위치는 miniaudio 좌표계의 미터 단위다. */
	bool SetPosition(const Vector3& audioPosition);

	/** 재생이 클립의 끝에 도달했으면 true를 반환한다. */
	bool HasEnded() const;
	SoundID GetSoundID() const noexcept { return soundID; }

private:
	bool InitializeSound(ma_engine& engine, ma_uint32 soundFlags);

private:
	// 재생 인스턴스가 살아 있는 동안 클립의 PCM 데이터를 유지한다.
	std::shared_ptr<const AudioClip> clip;
	// AudioSystem이 이 재생을 찾을 때 쓰는 ID다.
	SoundID soundID;
	ma_audio_buffer buffer;
	ma_sound sound;
	bool isBufferInitialized = false;
	bool isSoundInitialized = false;
};
