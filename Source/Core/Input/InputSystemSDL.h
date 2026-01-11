//
// Created by Orgest on 1/6/2026.
//

#pragma once
#include "InputSys.h"
#include "SDL3/SDL_events.h"

class InputSystemSDL
{
public:
    static void ProcessEvent(const SDL_Event& event);
    static Keyboard::Key MapSDLToEngineKey(SDL_Scancode code);
};