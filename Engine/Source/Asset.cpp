#include "Asset.h"

#include <atomic>
#include <utility>

namespace
{
	// 0은 "번호 없음"으로 남겨 두려고 1부터 준다. 에셋은 어느 스레드에서도 만들 수 있어 원자적으로 늘린다.
	std::atomic<std::uint64_t> nextInstanceID{ 1 };
}

Asset::Asset(std::string newName, std::string newPath)
	: name(std::move(newName)), path(std::move(newPath)), instanceID(nextInstanceID.fetch_add(1))
{
}

const std::string& Asset::GetName() const
{
	return name;
}

const std::string& Asset::GetPath() const
{
	return path;
}

std::uint64_t Asset::GetInstanceID() const
{
	return instanceID;
}
