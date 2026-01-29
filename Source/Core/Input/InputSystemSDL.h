//
// Created by Orgest on 1/6/2026.
//

#pragma once
#include "InputSys.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

struct SDLGamepadSlot
{
    SDL_Gamepad* handle = nullptr;
    SDL_JoystickID id = 0;
};

class InputSystemSDL
{
public:
    static int GetControllerIndex(SDL_JoystickID id);
    static void ProcessGamepadEvents(const SDL_Event& event);
    static void UpdateGamepadAxes();
    static void ProcessEvents(const SDL_Event& event);
    static Gamepad::Button MapSDLToEngineKey(SDL_GamepadButton button);
    static Keyboard::Key MapSDLToEngineKey(SDL_Scancode code);
};