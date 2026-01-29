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
    }
    else
    {
        // If it was held before, this is a "release" event
        if (state.held)
        {
            state.released = true;
        }
        state.held = false;
    }
}

// Resetting
void Input::EndFrameInputUpdate()
{
    input.xrel = 0.0;
    input.yrel = 0.0;
    input.scrollX = 0;
    input.scrollY = 0;

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
    Input temp{};

    temp.bindings = input.bindings;

    temp.cursorX = input.cursorX;
    temp.cursorY = input.cursorY;
    temp.lastX = input.lastX;
    temp.lastY = input.lastY;

    // Preserve raw input mode (we're not using this for now)
    temp.useRawInput = input.useRawInput;

    std::swap(input, temp);
}

void Input::ProcessEvents()
{
}