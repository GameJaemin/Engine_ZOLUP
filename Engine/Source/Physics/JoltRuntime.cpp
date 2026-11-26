#include "JoltRuntime.h"

#include "Core/Log.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>

JoltRuntime::~JoltRuntime()
{
	Shutdown();
}

bool JoltRuntime::Initialize()
{
	if (JPH::Factory::sInstance != nullptr)
	{
		Log::Write(LogLevel::Error, "Jolt", "A Jolt factory is already active");
		return false;
	}

	JPH::RegisterDefaultAllocator();
	// 설정이 다르면 RegisterTypes가 프로세스를 중단하므로 먼저 검사해 초기화 실패로 반환한다.
	if (!JPH::VerifyJoltVersionID())
	{
		Log::Write(LogLevel::Error, "Jolt", "Jolt headers and library configuration differ");
		return false;
	}

	JPH::Factory::sInstance = new JPH::Factory();
	// 타입 등록 중 예외가 발생해도 소유한 팩토리는 정리한다.
	ownsFactory = true;
	JPH::RegisterTypes();
	Log::Write(LogLevel::Info, "Jolt", "Runtime initialized");
	return true;
}

void JoltRuntime::Shutdown()
{
	if (!ownsFactory) { return; }

	// 타입 등록 해제에 전역 팩토리가 필요하므로 팩토리는 그 뒤에 삭제한다.
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
	ownsFactory = false;
	Log::Write(LogLevel::Info, "Jolt", "Runtime shut down");
}
