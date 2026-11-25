#include "EditorApplication.h"
#include "Engine.h"
#include "GameApplication.h"
#include "ThirdPerson/ThirdPersonTests.h"

#include <string_view>

int wmain(int argc, wchar_t* argv[])
{
	bool runEditor = false;
	for (int index = 1; index < argc; ++index)
	{
		const std::wstring_view argument(argv[index]);
		if (argument == L"--third-person-test")
		{
			return ThirdPersonTests::Run();
		}
		if (argument == L"--editor")
		{
			runEditor = true;
		}
	}

	Engine engine;
	if (runEditor)
	{
		EditorApplication editor;
		return engine.Run(editor);
	}

	GameApplication game;
	return engine.Run(game);
}
