#include "EditorApplication.h"
#include "Engine.h"
#include "GameApplication.h"
#include "ThirdPerson/ThirdPersonTests.h"
#include "BabyDoll/BabyDollApplication.h"
#include "BabyDoll/PrototypeTests.h"

#include <string_view>

int wmain(int argc, wchar_t* argv[])
{
	bool runEditor = false;
	bool runThirdPerson = false;
	for (int index = 1; index < argc; ++index)
	{
		const std::wstring_view argument(argv[index]);
		if (argument == L"--babydoll-test")
		{
			return PrototypeTests::Run();
		}
		if (argument == L"--third-person")
		{
			runThirdPerson = true;
		}
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

	if (runThirdPerson)
	{
		GameApplication game;
		return engine.Run(game);
	}
	BabyDollApplication prototype;
	return engine.Run(prototype);
}
