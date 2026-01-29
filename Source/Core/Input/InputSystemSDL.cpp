//
// Created by Orgest on 1/6/2026.
//

#include "InputSystemSDL.h"

#include "imgui_impl_sdl3.h"
#include "tracy/Tracy.hpp"

static Array<SDLGamepadSlot, CONTROLLER_COUNT> gamepads;

i32 InputSystemSDL::GetControllerIndex(const SDL_JoystickID id)
{
    for (int i = 0; i < CONTROLLER_COUNT; ++i)
    {
        if (gamepads[i].id == id)
        {
            return i;
        }
    }
    return -1;
}

void InputSystemSDL::ProcessGamepadEvents(const SDL_Event& event)
{

    switch (event.type)
    {
    case SDL_EVENT_GAMEPAD_ADDED:

        for (i32 i = 0; i < CONTROLLER_COUNT; i++)
        {
            SDLGamepadSlot& slot = gamepads[i];

            if (!slot.handle)
            {
                if (SDL_Gamepad* newPad = SDL_OpenGamepad(event.gdevice.which))
                {
                    slot.handle = newPad;
                    slot.id = event.gdevice.which;
                    input.controllers[i].connected = true;

                    input.controllers[i].leftX = 0.0f;
                    input.controllers[i].leftY = 0.0f;
                    input.controllers[i].rightX = 0.0f;
                    input.controllers[i].rightY = 0.0f;
                    input.controllers[i].leftTrigger = 0.0f;
                    input.controllers[i].rightTrigger = 0.0f;

                    break; // Found a slot, stop looking
                }
            }
        }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        {
            i32 idx = GetControllerIndex(event.gdevice.which);
            if (idx != -1)
            {
                SDL_CloseGamepad(gamepads[idx].handle);
                gamepads[idx] = {};
                input.controllers[idx].connected = false;
            }
            break;
        }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        {
            i32 idx = GetControllerIndex(event.gbutton.which);
            if (idx != -1)
            {
                const Gamepad::Button btn = MapSDLToEngineKey(static_cast<SDL_GamepadButton>(event.gbutton.button));

                if (btn != Gamepad::Unknown && btn != Gamepad::ButtonCount)
                {
                    bool isDown = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                    Input::ProcessEventButton(input.controllers[idx].buttons[btn], isDown);
                }
            }
            break;
        }
    }
}

void InputSystemSDL::UpdateGamepadAxes()
{
    // Define your Deadzone Helper
    auto ApplyDeadzone = [](const f32 value, const f32 deadzone = 0.15f)
    {
        if (fabsf(value) < deadzone) return 0.0f;
        return (value - (value > 0 ? deadzone : -deadzone)) / (1.0f - deadzone);
    };

    for (i32 i = 0; i < CONTROLLER_COUNT; ++i)
    {
        SDLGamepadSlot& slot = gamepads[i];
        if (!slot.handle) continue; // Skip disconnected

        auto& ctrl = input.controllers[i];

        // ---------------------------------------------------------
        // 1. POLL AXES (Super fast, no event queue overhead)
        // ---------------------------------------------------------
        const Sint16 rawLX = SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_LEFTX);
        const Sint16 rawLY = SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_LEFTY);
        const Sint16 rawRX = SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_RIGHTX);
        const Sint16 rawRY = SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_RIGHTY);
        const Sint16 rawLT = SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        const Sint16 rawRT = SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

        ctrl.leftX  = ApplyDeadzone(NORM_THUMB(rawLX));
        ctrl.leftY  = ApplyDeadzone(-NORM_THUMB(rawLY));

        ctrl.rightX = ApplyDeadzone(NORM_THUMB(rawRX));
        ctrl.rightY = ApplyDeadzone(-NORM_THUMB(rawRY));

        // Triggers (0..32767 -> 0.0..1.0)
        // Note: Use 32767.0f to hit exactly 1.0f
        ctrl.leftTrigger  = (f32)rawLT / 32767.0f;
        ctrl.rightTrigger = (f32)rawRT / 32767.0f;


        const bool moved =
            fabsf(ctrl.leftX) > 0.0f || fabsf(ctrl.leftY) > 0.0f ||
            fabsf(ctrl.rightX) > 0.0f || fabsf(ctrl.rightY) > 0.0f ||
            ctrl.leftTrigger > 0.001f || ctrl.rightTrigger > 0.001f;

        if (moved) input.usingController = true;
    }
}

void InputSystemSDL::ProcessEvents(const SDL_Event& event)
{
    FrameMark;
    ZoneScoped;
    ImGui_ImplSDL3_ProcessEvent(&event);

    const ImGuiIO& io = ImGui::GetIO();

    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        {
            // if (!io.WantCaptureKeyboard) break;
            if (event.key.repeat) break;

            Keyboard::Key key = MapSDLToEngineKey(event.key.scancode);
            if (key != Keyboard::Unknown)
            {
                Input::ProcessEventButton(input.keyboard[key], event.type == SDL_EVENT_KEY_DOWN);
                input.usingKeyboard = true;
                input.usingController = false;
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        {
            input.cursorX = event.motion.x;
            input.cursorY = event.motion.y;
            input.xrel = event.motion.xrel;
            input.yrel = event.motion.yrel;
            input.usingMouse = true;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            if (io.WantCaptureMouse) break;

            Mouse::Button btn = Mouse::ButtonCount;
            if (event.button.button == SDL_BUTTON_LEFT) btn = Mouse::Left;
            else if (event.button.button == SDL_BUTTON_RIGHT) btn = Mouse::Right;
            else if (event.button.button == SDL_BUTTON_MIDDLE) btn = Mouse::Middle;

            if (btn != Mouse::ButtonCount)
            {
                Input::ProcessEventButton(input.mouseButtons[btn], event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            }
        }
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        {
            if (io.WantCaptureMouse) break;
            input.scrollX = static_cast<i64>(event.wheel.x);
            input.scrollY = static_cast<i64>(event.wheel.y);
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        {
            ZoneScopedN("Event: Gamepad Added");
            ProcessGamepadEvents(event);
            break;
        }
    case SDL_EVENT_GAMEPAD_REMOVED:
        {
            ZoneScopedN("Event: Gamepad Removed");
            ProcessGamepadEvents(event);
            break;
        }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        {
            ProcessGamepadEvents(event);
            break;
        }
    }

    {
        ZoneScopedN("Input::UpdateAxes");
        UpdateGamepadAxes();
    }
}

Gamepad::Button InputSystemSDL::MapSDLToEngineKey(const SDL_GamepadButton button)
{
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH: return Gamepad::A;
    case SDL_GAMEPAD_BUTTON_EAST: return Gamepad::B;
    case SDL_GAMEPAD_BUTTON_WEST: return Gamepad::X;
    case SDL_GAMEPAD_BUTTON_NORTH: return Gamepad::Y;
    case SDL_GAMEPAD_BUTTON_BACK: return Gamepad::Select;
    case SDL_GAMEPAD_BUTTON_GUIDE: return Gamepad::ButtonCount;
    case SDL_GAMEPAD_BUTTON_START: return Gamepad::Start;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return Gamepad::LeftThumb;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return Gamepad::RightThumb;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return Gamepad::LeftShoulder;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return Gamepad::RightShoulder;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return Gamepad::DpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return Gamepad::DpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return Gamepad::DpadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return Gamepad::DpadRight;
    default: return Gamepad::Unknown;
    }
}

Keyboard::Key InputSystemSDL::MapSDLToEngineKey(const SDL_Scancode code)
{
    switch (code)
    {
    // Alphabet
    case SDL_SCANCODE_A: return Keyboard::A;
    case SDL_SCANCODE_B: return Keyboard::B;
    case SDL_SCANCODE_C: return Keyboard::C;
    case SDL_SCANCODE_D: return Keyboard::D;
    case SDL_SCANCODE_E: return Keyboard::E;
    case SDL_SCANCODE_F: return Keyboard::F;
    case SDL_SCANCODE_G: return Keyboard::G;
    case SDL_SCANCODE_H: return Keyboard::H;
    case SDL_SCANCODE_I: return Keyboard::I;
    case SDL_SCANCODE_J: return Keyboard::J;
    case SDL_SCANCODE_K: return Keyboard::K;
    case SDL_SCANCODE_L: return Keyboard::L;
    case SDL_SCANCODE_M: return Keyboard::M;
    case SDL_SCANCODE_N: return Keyboard::N;
    case SDL_SCANCODE_O: return Keyboard::O;
    case SDL_SCANCODE_P: return Keyboard::P;
    case SDL_SCANCODE_Q: return Keyboard::Q;
    case SDL_SCANCODE_R: return Keyboard::R;
    case SDL_SCANCODE_S: return Keyboard::S;
    case SDL_SCANCODE_T: return Keyboard::T;
    case SDL_SCANCODE_U: return Keyboard::U;
    case SDL_SCANCODE_V: return Keyboard::V;
    case SDL_SCANCODE_W: return Keyboard::W;
    case SDL_SCANCODE_X: return Keyboard::X;
    case SDL_SCANCODE_Y: return Keyboard::Y;
    case SDL_SCANCODE_Z: return Keyboard::Z;

    // Numbers
    case SDL_SCANCODE_1: return Keyboard::Num1;
    case SDL_SCANCODE_2: return Keyboard::Num2;
    case SDL_SCANCODE_3: return Keyboard::Num3;
    case SDL_SCANCODE_4: return Keyboard::Num4;
    case SDL_SCANCODE_5: return Keyboard::Num5;
    case SDL_SCANCODE_6: return Keyboard::Num6;
    case SDL_SCANCODE_7: return Keyboard::Num7;
    case SDL_SCANCODE_8: return Keyboard::Num8;
    case SDL_SCANCODE_9: return Keyboard::Num9;
    case SDL_SCANCODE_0: return Keyboard::Num0;

    // Modifiers
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT: return Keyboard::Shift;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL: return Keyboard::Ctrl;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT: return Keyboard::Alt;

    // Special
    case SDL_SCANCODE_TAB: return Keyboard::Tab;
    case SDL_SCANCODE_RETURN: return Keyboard::Enter;
    case SDL_SCANCODE_ESCAPE: return Keyboard::Escape;
    case SDL_SCANCODE_BACKSPACE: return Keyboard::Backspace;
    case SDL_SCANCODE_SPACE: return Keyboard::Space;
    case SDL_SCANCODE_INSERT: return Keyboard::Insert;
    case SDL_SCANCODE_DELETE: return Keyboard::Delete;
    case SDL_SCANCODE_HOME: return Keyboard::Home;
    case SDL_SCANCODE_END: return Keyboard::End;

    // Arrows
    case SDL_SCANCODE_UP: return Keyboard::Up;
    case SDL_SCANCODE_DOWN: return Keyboard::Down;
    case SDL_SCANCODE_LEFT: return Keyboard::Left;
    case SDL_SCANCODE_RIGHT: return Keyboard::Right;

    // Function Keys
    case SDL_SCANCODE_F1: return Keyboard::F1;
    case SDL_SCANCODE_F2: return Keyboard::F2;
    case SDL_SCANCODE_F3: return Keyboard::F3;
    case SDL_SCANCODE_F4: return Keyboard::F4;
    case SDL_SCANCODE_F5: return Keyboard::F5;
    case SDL_SCANCODE_F6: return Keyboard::F6;
    case SDL_SCANCODE_F7: return Keyboard::F7;
    case SDL_SCANCODE_F8: return Keyboard::F8;
    case SDL_SCANCODE_F9: return Keyboard::F9;
    case SDL_SCANCODE_F10: return Keyboard::F10;
    case SDL_SCANCODE_F11: return Keyboard::F11;
    case SDL_SCANCODE_F12: return Keyboard::F12;

    // Symbols
    case SDL_SCANCODE_GRAVE: return Keyboard::Tilde;
    case SDL_SCANCODE_MINUS: return Keyboard::Minus_Underscore;
    case SDL_SCANCODE_EQUALS: return Keyboard::Plus_Equal;
    case SDL_SCANCODE_LEFTBRACKET: return Keyboard::SquareBracketsOpen;
    case SDL_SCANCODE_RIGHTBRACKET: return Keyboard::SquareBracketsClose;
    case SDL_SCANCODE_BACKSLASH: return Keyboard::Backslash;
    case SDL_SCANCODE_SEMICOLON: return Keyboard::SemiColon;
    case SDL_SCANCODE_APOSTROPHE: return Keyboard::Quotes;
    case SDL_SCANCODE_COMMA: return Keyboard::Comma_LeftArrow;
    case SDL_SCANCODE_PERIOD: return Keyboard::Period_RightArrow;
    case SDL_SCANCODE_SLASH: return Keyboard::Slash;

    // Locks & Misc
    case SDL_SCANCODE_CAPSLOCK: return Keyboard::CapsLock;
    case SDL_SCANCODE_NUMLOCKCLEAR: return Keyboard::NumLock;
    case SDL_SCANCODE_SCROLLLOCK: return Keyboard::ScrollLock;
    case SDL_SCANCODE_PRINTSCREEN: return Keyboard::PrintScreen;
    case SDL_SCANCODE_PAUSE: return Keyboard::Pause;
    case SDL_SCANCODE_MENU: return Keyboard::Menu;

    default: return Keyboard::Unknown;
    }
}
