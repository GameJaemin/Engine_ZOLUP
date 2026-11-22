#include "EditorApplication.h"
#include "Engine.h"
#include "GameApplication.h"

#include <Windows.h>

#include <cstdio>
#include <string>
#include <string_view>

int wmain(int argc, wchar_t* argv[])
{
	bool runEditor = false;
	std::string mapCameraName;
	for (int index = 1; index < argc; ++index)
	{
		const std::wstring_view argument(argv[index]);
		if (argument == L"--editor")
		{
			runEditor = true;
		}
		else if (argument == L"--map-camera")
		{
			if (!mapCameraName.empty() || index + 1 >= argc)
			{
				std::fputs("Specify --map-camera once with a map object name.\n", stderr);
				return 1;
			}

			const std::wstring_view name(argv[++index]);
			if (name.empty() || name.starts_with(L"--"))
			{
				std::fputs("--map-camera requires a map object name.\n", stderr);
				return 1;
			}
			// 맵 오브젝트 이름은 UTF-8로 저장하므로 Windows 인자를 같은 인코딩으로 바꾼다.
			const int utf8Size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name.data(), -1, nullptr, 0, nullptr, nullptr);
			if (utf8Size == 0)
			{
				std::fputs("Invalid Unicode in --map-camera name.\n", stderr);
				return 1;
			}

			mapCameraName.resize(static_cast<std::size_t>(utf8Size));
			if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name.data(), -1, mapCameraName.data(), utf8Size, nullptr, nullptr) == 0)
			{
				std::fputs("Could not convert --map-camera name to UTF-8.\n", stderr);
				return 1;
			}
			mapCameraName.pop_back();
		}
	}

	Engine engine;
	if (runEditor)
	{
		EditorApplication editor;
		return engine.Run(editor);
	}

	GameApplication game(mapCameraName);
	return engine.Run(game);
}
