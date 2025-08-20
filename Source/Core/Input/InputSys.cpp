//
// Created by Orgest on 7/26/2025.
//

#include "InputSys.h"

void Input::ProcessEventButton(ButtonState& state, const bool isPressed)
{
	if (isPressed)
	{
		if (!state.held)
			state.pressed = true;

		state.held = true;
		state.released = false;
	}
	else
	{
		state.pressed = false;
		state.held = false;
		state.released = true;
	}
}

// Clear per-frame states: pressed/released
void Input::EndFrameInputUpdate()
{
	for (auto& key : input.keyboard)
	{
		key.pressed = false;
		key.released = false;
	}

	for (auto& btn : input.mouseButtons)
	{
		btn.pressed = false;
		btn.released = false;
	}

	for (auto& btn : input.gamepadButtons)
	{
		btn.pressed = false;
		btn.released = false;
	}

	input.xrel = 0.0f;
	input.yrel = 0.0f;
	input.scrollDelta = 0;
}

// Reset all input state (used when focus is lost)
void Input::ResetInputOnFocusLoss()
{
	Input temp{};


	temp.cursorX = input.cursorX;
	temp.cursorY = input.cursorY;
	temp.lastX   = input.lastX;
	temp.lastY   = input.lastY;

	temp.leftMotorVibration  = input.leftMotorVibration;
	temp.rightMotorVibration = input.rightMotorVibration;

	// temp.focused         = input.focused;
	// temp.mouseLookActive = input.mouseLookActive;
	temp.useRawInput     = input.useRawInput;

	input = temp;
}
