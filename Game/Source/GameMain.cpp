#include "EditorApplication.h"
#include "Engine.h"
#include "GameApplication.h"

#include <string_view>

int main(int argc, char* argv[])
{
	Engine engine;

	for (int index = 1; index < argc; ++index)
	{
		if (std::string_view{ argv[index] } == "--editor")
		{
			EditorApplication editor;
			return engine.Run(editor);
		}
	}

	GameApplication game;
	return engine.Run(game);
}
