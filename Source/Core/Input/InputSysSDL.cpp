//
// Created by Orgest on 11/21/2025.
//

#include <SDL3/SDL.h>
#include "InputSys.h"
#include "Tools/Logger.h"

static Keyboard::Key SDLKeyToKey(SDL_Keycode key)
{
    using namespace Keyboard;

    switch (key)
    {
    case SDLK_A: return A;
    case SDLK_B: return B;
    case SDLK_C: return C;
    case SDLK_D: return D;
    case SDLK_E: return E;
    case SDLK_F: return F;
    case SDLK_G: return G;
    case SDLK_H: return H;
    case SDLK_I: return I;
    case SDLK_J: return J;
    case SDLK_K: return K;
    case SDLK_L: return L;
    case SDLK_M: return M;
    case SDLK_N: return N;
    case SDLK_O: return O;
    case SDLK_P: return P;
    case SDLK_Q: return Q;
    case SDLK_R: return R;
    case SDLK_S: return S;
    case SDLK_T: return T;
    case SDLK_U: return U;
    case SDLK_V: return V;
    case SDLK_W: return W;
    case SDLK_X: return X;
    case SDLK_Y: return Y;
    case SDLK_Z: return Z;

    case SDLK_0: return Num0;
    case SDLK_1: return Num1;
    case SDLK_2: return Num2;
    case SDLK_3: return Num3;
    case SDLK_4: return Num4;
    case SDLK_5: return Num5;
    case SDLK_6: return Num6;
    case SDLK_7: return Num7;
    case SDLK_8: return Num8;
    case SDLK_9: return Num9;

    case SDLK_LSHIFT:
    case SDLK_RSHIFT: return Shift;
    case SDLK_LCTRL:
    case SDLK_RCTRL: return Ctrl;
    case SDLK_LALT:
    case SDLK_RALT: return Alt;

    case SDLK_SPACE: return Space;
    case SDLK_ESCAPE: return Escape;
    case SDLK_RETURN: return Enter;
    case SDLK_BACKSPACE: return Backspace;
    case SDLK_TAB: return Tab;

    case SDLK_LEFT: return Left;
    case SDLK_RIGHT: return Right;
    case SDLK_UP: return Up;
    case SDLK_DOWN: return Down;

    case SDLK_F1: return F1;
    case SDLK_F2: return F2;
    case SDLK_F3: return F3;
    case SDLK_F4: return F4;
    case SDLK_F5: return F5;
    case SDLK_F6: return F6;
    case SDLK_F7: return F7;
    case SDLK_F8: return F8;
    case SDLK_F9: return F9;
    case SDLK_F10: return F10;
    case SDLK_F11: return F11;
    case SDLK_F12: return F12;

    default:
        return Unknown;
    }
}

static Mouse::Button SDLMouseToButton(const u8 btn)
{
    using namespace Mouse;
    switch (btn)
    {
    case SDL_BUTTON_LEFT: return Left;
    case SDL_BUTTON_RIGHT: return Right;
    case SDL_BUTTON_MIDDLE: return Middle;
    default: return ButtonCount;
    }
}


void Input::ProcessEvents()
{
    input.xrel = 0.0;
    input.yrel = 0.0;
    input.scrollX = 0;
    input.scrollY = 0;

    SDL_Event e;

    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        // ───────────────────────────────────────────────────────
        // KEYBOARD
        // ───────────────────────────────────────────────────────
        case SDL_EVENT_KEY_DOWN:
            {
                Keyboard::Key k = SDLKeyToKey(e.key.key);
                if (k != Keyboard::Unknown)
                {
                    Input::ProcessEventButton(input.keyboard[k], true);
                }
                input.usingKeyboard = true;
            }
            break;

        case SDL_EVENT_KEY_UP:
            {
                Keyboard::Key k = SDLKeyToKey(e.key.key);
                if (k != Keyboard::Unknown)
                {
                    Input::ProcessEventButton(input.keyboard[k], false);
                }
            }
            break;

        // ───────────────────────────────────────────────────────
        // MOUSE MOVEMENT
        // ───────────────────────────────────────────────────────
        case SDL_EVENT_MOUSE_MOTION:
            {
                input.cursorX = e.motion.x;
                input.cursorY = e.motion.y;
                input.xrel = e.motion.xrel;
                input.yrel = e.motion.yrel;
                input.usingMouse = true;
            }
            break;

        // ───────────────────────────────────────────────────────
        // MOUSE BUTTONS
        // ───────────────────────────────────────────────────────
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            {
                Mouse::Button button = SDLMouseToButton(e.button.button);
                if (button < Mouse::ButtonCount)
                {
                    bool pressed = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                    Input::ProcessEventButton(input.mouseButtons[button], pressed);
                }
                input.usingMouse = true;
            }
            break;

        // ───────────────────────────────────────────────────────
        // SCROLL WHEEL
        // ───────────────────────────────────────────────────────
        case SDL_EVENT_MOUSE_WHEEL:
            {
                input.scrollX = e.wheel.x;
                input.scrollY = e.wheel.y;
                input.usingMouse = true;
            }
            break;

        // ───────────────────────────────────────────────────────
        // WINDOW FOCUS LOSS
        // ───────────────────────────────────────────────────────
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            {
                LOG(Info, "[SDL3] Window lost focus,  resetting input state.");
                Input::ResetInputOnFocusLoss();
            }
            break;

        case SDL_EVENT_QUIT:
            // Set an internal "quit" flag if you want
            break;

        default:
            break;
        }
    }
}