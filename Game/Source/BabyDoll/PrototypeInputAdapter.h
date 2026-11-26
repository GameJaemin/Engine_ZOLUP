#pragma once

#include "PrototypeTypes.h"

#include "Input/InputState.h"
#include "Input/Key.h"

struct PrototypeInputFrame
{
	PrototypeCommand command;
	bool togglePause = false;
	bool toggleDiagnostics = false;
	bool leftDown = false;
	bool rightDown = false;
};

/** 하드웨어 키를 게임 명령으로 바꾸는 유일한 위치다. 입력 수집은 기존 엔진이 담당한다. */
class PrototypeInputAdapter
{
public:
	Key leftFoot = Key::S;
	Key rightFoot = Key::Period;

	PrototypeInputFrame Read(const InputState& input, bool solo = false) const
	{
		PrototypeInputFrame frame;
		for (const KeyEvent& event : input.GetFrameEvents())
		{
			if (event.type != KeyEventType::Pressed) { continue; }
			if (event.key == leftFoot) { frame.command.feet.push_back(0); }
			if (event.key == (solo ? Key::D : rightFoot)) { frame.command.feet.push_back(1); }
			if (event.key == Key::T || (solo && event.key == Key::MouseRight)) { frame.command.interact = true; }
			if (event.key == Key::Space) { frame.command.pulse = true; }
			if (event.key == Key::Escape) { frame.togglePause = true; }
			if (event.key == Key::Tab) { frame.toggleDiagnostics = true; }
		}
		const RawMouseDelta mouse = input.GetRawMouseDelta();
		frame.command.mouseX = static_cast<float>(mouse.x);
		frame.command.mouseY = static_cast<float>(mouse.y);
		frame.command.forward = static_cast<float>(input.IsKeyDown(Key::W)) - static_cast<float>(input.IsKeyDown(Key::S));
		frame.command.right = static_cast<float>(input.IsKeyDown(Key::D)) - static_cast<float>(input.IsKeyDown(Key::A));
		frame.command.run = input.IsKeyDown(Key::Shift);
		frame.leftDown = input.IsKeyDown(leftFoot);
		frame.rightDown = input.IsKeyDown(solo ? Key::D : rightFoot);
		return frame;
	}
};
