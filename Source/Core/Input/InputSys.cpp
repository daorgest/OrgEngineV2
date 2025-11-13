//
// Created by Orgest on 7/26/2025.
//

#include "InputSys.h"

void Input::ProcessEventButton(ButtonState& state, const bool isPressed)
{
    if (isPressed)
    {
        if (!state.held)
        {
            state.pressed = true;
        }

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

// Resetting
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

    for (auto& controller : input.controllers)
    {
        for (auto& btn : controller.buttons)
        {
            btn.pressed = false;
            btn.released = false;
        }
    }
}

// Reset all input state (used when focus is lost)
void Input::ResetInputOnFocusLoss()
{
    Input temp{}; // clears EVERYTHING

    // Preserve absolute cursor positions so the engine doesn't jump
    temp.cursorX = input.cursorX;
    temp.cursorY = input.cursorY;
    temp.lastX = input.lastX;
    temp.lastY = input.lastY;

    // Preserve raw input mode
    temp.useRawInput = input.useRawInput;

    // Deltas must be zeroed explicitly to avoid ghost movement on refocus
    temp.xrel = 0.0f;
    temp.yrel = 0.0f;
    temp.scrollX = 0;
    temp.scrollY = 0;

    std::swap(input, temp);
}