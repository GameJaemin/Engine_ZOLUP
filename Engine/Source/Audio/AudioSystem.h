#pragma once

#include <memory>

struct ma_engine;

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

private:
	// 초기화에 성공한 엔진만 소유한다. nullptr이면 장치가 열려 있지 않다.
	std::unique_ptr<ma_engine> audioEngine;
};
