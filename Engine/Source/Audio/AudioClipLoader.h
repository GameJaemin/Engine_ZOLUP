#pragma once

#include <filesystem>
#include <memory>
#include <string>

class AudioClip;

namespace AudioClipLoader
{
	/** absolutePath에서 WAV를 읽고 assetPath를 클립의 저장소 루트 기준 UTF-8 경로로 기록한다. 읽기 또는 디코딩 실패 시 로그를 남기고 nullptr를 반환한다. */
	[[nodiscard]] std::shared_ptr<const AudioClip> LoadWavFile(const std::filesystem::path& absolutePath, std::string assetPath);
}
