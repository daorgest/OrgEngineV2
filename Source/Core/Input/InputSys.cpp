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

f32 Input::ApplyDeadzone(const f32 value, const f32 deadzone)
{
    if (std::abs(value) < deadzone) return 0.0f;
    return (value - (value > 0.0f ? deadzone : -deadzone)) / (1.0f - deadzone);
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

    std::swap(input, temp);
}

void Input::ProcessEvents()
{
}