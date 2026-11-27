#pragma once

#include "SoundID.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

struct ma_engine;
struct Vector3;
class AudioClip;
class PlayingSound;

/** miniaudio 엔진과 기본 출력 장치의 수명을 관리한다. 메인 스레드에서 사용한다. */
class AudioSystem
{
public:
	AudioSystem();
	~AudioSystem();

	AudioSystem(const AudioSystem&) = delete;
	AudioSystem& operator=(const AudioSystem&) = delete;
	AudioSystem(AudioSystem&&) = delete;
	AudioSystem& operator=(AudioSystem&&) = delete;

	/** 기본 출력 장치를 연다. 이미 열었거나 장치를 열지 못하면 false를 반환한다. */
	bool Initialize();

	/** 재생 중인 소리를 모두 멈추고 장치를 닫는다. 열지 않았으면 바로 반환한다. */
	void Shutdown();

	/** 클립을 공간 효과 없이 한 번 재생한다. 종료 시 재생 인스턴스를 정리하며, 시작하지 못하면 nullopt를 반환한다. */
	std::optional<SoundID> PlaySound2D(std::shared_ptr<const AudioClip> clip);

	/** 월드 위치(cm)에서 클립을 한 번 재생한다. minDistanceCm은 유한한 양수이며 이 거리까지는 감쇠를 적용하지 않는다. 시작하지 못하면 nullopt를 반환한다. */
	std::optional<SoundID> PlaySoundAtLocation(std::shared_ptr<const AudioClip> clip, const Vector3& position, float minDistanceCm = 100.0f);

	/** 재생을 바로 멈추고 정리한다. 이미 끝났거나 찾을 수 없는 ID이면 아무것도 하지 않는다. */
	void StopSound(SoundID soundID);

	/** 재생의 월드 위치(cm)를 바꾼다. 찾을 수 없는 ID이면 아무것도 하지 않으며, 2D 재생에는 효과가 없다. */
	void SetSoundLocation(SoundID soundID, const Vector3& position);

	/** 기본 청취자의 월드 위치(cm)와 방향을 설정한다. forward와 up은 서로 직교하는 단위 벡터다. */
	void SetListenerPose(const Vector3& position, const Vector3& forward, const Vector3& up);

	/** 재생이 끝난 인스턴스를 목록에서 제거한다. */
	void Update();

private:
	// 초기화에 성공한 엔진만 소유한다. nullptr이면 장치가 열려 있지 않다.
	std::unique_ptr<ma_engine> audioEngine;
	std::vector<std::unique_ptr<PlayingSound>> playingSounds;
	// 다음에 발급할 재생 ID다. 장치를 다시 열어도 초기화하지 않는다.
	std::uint64_t nextSoundID = 1;
};
