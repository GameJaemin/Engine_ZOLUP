#pragma once

/** 메인 스레드에서 Jolt의 프로세스 전역 팩토리와 타입 등록의 수명을 관리한다. */
class JoltRuntime final
{
public:
	JoltRuntime() = default;
	~JoltRuntime();

	JoltRuntime(const JoltRuntime&) = delete;
	JoltRuntime& operator=(const JoltRuntime&) = delete;
	JoltRuntime(JoltRuntime&&) = delete;
	JoltRuntime& operator=(JoltRuntime&&) = delete;

	/** Jolt 객체를 만들기 전에 호출한다. 팩토리가 이미 있거나 헤더와 라이브러리가 맞지 않으면 실패한다. */
	[[nodiscard]] bool Initialize();

	/** 모든 Jolt 객체를 정리한 뒤 호출한다. 소유한 팩토리가 없으면 바로 반환한다. */
	void Shutdown();

private:
	bool ownsFactory = false;
};
