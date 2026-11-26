#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

/** 자산 파일 경로와 조회 키를 다룬다. */
namespace Paths
{
	/**
	 * 저장소 루트 폴더를 구한다. 실행 파일이 Directory.Build.props의 OutDir(Binaries 아래)에 있어야 하며,
	 * 아니면 출력값을 유지하고 false를 반환한다.
	 * 저장소의 원본 폴더를 가리키므로 개발 중 실행에서만 동작한다.
	 */
	bool GetRootDirectory(std::filesystem::path& outPath);

	/**
	 * 저장소 루트 기준 UTF-8 상대 경로(예: Game/Content/Meshes/Box.gltf)를 절대 경로로 바꾼다.
	 * GetRootDirectory가 실패하면 출력값을 유지하고 false를 반환한다.
	 */
	bool GetAbsolutePath(std::string_view relativePath, std::filesystem::path& outPath);

	/**
	 * UTF-8 자산 경로의 .과 ..을 정리하고 구분자를 /로 통일해 조회 키를 만든다.
	 * ASCII A-Z만 소문자로 바꾼다. 파일을 열 때는 원래 경로를 사용한다.
	 */
	std::string MakeAssetKey(std::string_view path);

	// 에셋 경로 규칙이다. 파일 하나가 에셋 하나면 "파일경로"이고, 파일 안의 에셋이면 "파일경로|하위이름"이다.
	// '|'는 Windows 파일 이름에 쓸 수 없으므로, 에셋 타입을 몰라도 '|'가 있으면 항상 그 자리에서 나눌 수 있다.
	inline constexpr char SubAssetSeparator = '|';

	/** 파일 안의 에셋 경로를 만든다. */
	std::string MakeSubAssetPath(std::string_view filePath, std::string_view subAssetName);

	/** 마지막 구분 기호로 파일 경로와 하위 이름을 나눈다. 구분 기호가 없으면(파일 하나가 에셋 하나) nullopt를 반환한다. */
	std::optional<std::pair<std::string_view, std::string_view>> SplitSubAssetPath(std::string_view path);
}
