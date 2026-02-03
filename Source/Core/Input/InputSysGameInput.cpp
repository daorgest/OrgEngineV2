//
// Created by Orgest on 10/1/2025.
//

#include "InputSysGameInput.h"

#include "InputSys.h"
#include "Platform.h"

#define HR_FAIL(hr) (FAILED((hr)))

// Button Mappings
static u32 MapButtons(const GI::GameInputGamepadState& s)
{
    u32 b = 0;
    if (s.buttons & GI::GameInputGamepadA) b |= (1 << Gamepad::Button::A);
    if (s.buttons & GI::GameInputGamepadB) b |= (1 << Gamepad::Button::B);
    if (s.buttons & GI::GameInputGamepadX) b |= (1 << Gamepad::Button::X);
    if (s.buttons & GI::GameInputGamepadY) b |= (1 << Gamepad::Button::Y);
    if (s.buttons & GI::GameInputGamepadMenu) b |= (1 << Gamepad::Button::Start);
    if (s.buttons & GI::GameInputGamepadView) b |= (1 << Gamepad::Button::Select);
    if (s.buttons & GI::GameInputGamepadLeftShoulder) b |= (1 << Gamepad::Button::L1);
    if (s.buttons & GI::GameInputGamepadRightShoulder) b |= (1 << Gamepad::Button::R1);
    if (s.buttons & GI::GameInputGamepadLeftThumbstick) b |= (1 << Gamepad::Button::L3);
    if (s.buttons & GI::GameInputGamepadRightThumbstick) b |= (1 << Gamepad::Button::R3);
    if (s.buttons & GI::GameInputGamepadDPadUp) b |= (1 << Gamepad::Button::DpadUp);
    if (s.buttons & GI::GameInputGamepadDPadDown) b |= (1 << Gamepad::Button::DpadDown);
    if (s.buttons & GI::GameInputGamepadDPadLeft) b |= (1 << Gamepad::Button::DpadLeft);
    if (s.buttons & GI::GameInputGamepadDPadRight) b |= (1 << Gamepad::Button::DpadRight);

    // ada
    if (s.leftTrigger > 0.5f) b |= (1 << Gamepad::Button::L2);
    if (s.rightTrigger > 0.5f) b |= (1 << Gamepad::Button::R2);
    return b;
}

inline Keyboard::Key MapKey(const GI::GameInputKeyState& k)
{
    const u8 vk = k.virtualKey;

    switch (vk)
    {
    // navigation / control
    case VK_ESCAPE: return Keyboard::Escape;
    case VK_TAB: return Keyboard::Tab;
    case VK_RETURN: return Keyboard::Enter;
    case VK_BACK: return Keyboard::Backspace;
    case VK_SPACE: return Keyboard::Space;

    case VK_INSERT: return Keyboard::Insert;
    case VK_DELETE: return Keyboard::Delete;
    case VK_HOME: return Keyboard::Home;
    case VK_END: return Keyboard::End;
    case VK_LEFT: return Keyboard::Left;
    case VK_RIGHT: return Keyboard::Right;
    case VK_UP: return Keyboard::Up;
    case VK_DOWN: return Keyboard::Down;


    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT: return Keyboard::Shift;

    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL: return Keyboard::Ctrl;

    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU: return Keyboard::Alt;

    // function keys
    case VK_F1: return Keyboard::F1;
    case VK_F2: return Keyboard::F2;
    case VK_F3: return Keyboard::F3;
    case VK_F4: return Keyboard::F4;
    case VK_F5: return Keyboard::F5;
    case VK_F6: return Keyboard::F6;
    case VK_F7: return Keyboard::F7;
    case VK_F8: return Keyboard::F8;
    case VK_F9: return Keyboard::F9;
    case VK_F10: return Keyboard::F10;
    case VK_F11: return Keyboard::F11;
    case VK_F12: return Keyboard::F12;

    // locks & system
    case VK_CAPITAL: return Keyboard::CapsLock;
    case VK_NUMLOCK: return Keyboard::NumLock;
    case VK_SCROLL: return Keyboard::ScrollLock;
    case VK_SNAPSHOT: return Keyboard::PrintScreen;
    case VK_PAUSE: return Keyboard::Pause;
    case VK_APPS: return Keyboard::Menu;

    // OEM
    case VK_OEM_1: return Keyboard::SemiColon; // ; :
    case VK_OEM_2: return Keyboard::Question_BackSlash; // / ?
    case VK_OEM_3: return Keyboard::Tilde; // ` ~
    case VK_OEM_4: return Keyboard::SquareBracketsOpen; // [
    case VK_OEM_5: return Keyboard::Backslash; // \ |
    case VK_OEM_6: return Keyboard::SquareBracketsClose; // ]
    case VK_OEM_7: return Keyboard::Quotes; // ' "
    case VK_OEM_COMMA: return Keyboard::Comma_LeftArrow; // ,
    case VK_OEM_PERIOD: return Keyboard::Period_RightArrow; // .
    case VK_OEM_PLUS: return Keyboard::Plus_Equal; // =
    case VK_OEM_MINUS: return Keyboard::Minus_Underscore; // -

    default: break;
    }

    // A–Z
    if (vk >= 'A' && vk <= 'Z')
        return static_cast<Keyboard::Key>(Keyboard::A + (vk - 'A'));

    // 0–9 (top row)
    if (vk >= '0' && vk <= '9')
        return static_cast<Keyboard::Key>(Keyboard::Num0 + (vk - '0'));

    return Keyboard::Unknown;
}


bool InputSysGameInput::Init()
{
    if (FAILED(GameInputCreate(&gi))) return false;

    if (SUCCEEDED(gi->CreateDispatcher(&dispatcher))) // for hotplugging and background work)
    {
        gi->RegisterDeviceCallback(nullptr, GI::GameInputKindGamepad | GI::GameInputKindController, GI::GameInputDeviceConnected,
                                   GI::GameInputAsyncEnumeration,
                                   this, DeviceCallback, &giToken);
    }
    return true;
}

void InputSysGameInput::Shutdown()
{
    if (dispatcher && giToken)
    {
        dispatcher->Release();
        giToken = 0;
    }

    for (auto* pad : gamepads)
    {
        if (pad)
        {
            pad->Release();
        }
    }
    gamepads.fill(nullptr);

    if (prevReading)
    {
        prevReading->Release();
        prevReading = nullptr;
    }

    if (gi)
    {
        gi->Release();
        gi = nullptr;
    }

    dispatcher = nullptr;
    connectedCount = 0;
}


// Matches typedef: void CALLBACK(...)
void CALLBACK InputSysGameInput::DeviceCallback(GI::GameInputCallbackToken /*token*/, void* context,
                                                GI::IGameInputDevice* device,
                                                uint64_t /*timestamp*/,
                                                GI::GameInputDeviceStatus currentStatus,
                                                GI::GameInputDeviceStatus previousStatus)
{
    auto* self = static_cast<InputSysGameInput*>(context);
    if (!self || !device) return;

    const bool nowConnected = (currentStatus & GI::GameInputDeviceConnected) != 0;
    const bool wasConnected = (previousStatus & GI::GameInputDeviceConnected) != 0;

    if (nowConnected && !wasConnected)
    {
        // plug-in: add to first free slot
        for (u32 i = 0; i < MAX_GAMEPADS; ++i)
        {
            if (!self->gamepads[i])
            {
                self->gamepads[i] = device;
                self->gamepads[i]->AddRef();
                break;
            }
        }
    }
    else if (!nowConnected && wasConnected)
    {
        // unplug: remove matching slot
        for (u32 i = 0; i < MAX_GAMEPADS; ++i)
        {
            if (self->gamepads[i] == device)
            {
                self->gamepads[i]->Release();
                self->gamepads[i] = nullptr;
            }
        }
    }

    // recompute
    self->connectedCount = 0;
    for (u32 i = 0; i < MAX_GAMEPADS; ++i)
    {
        if (self->gamepads[i]) self->connectedCount++;
    }
}

void HandleKeyboard(GI::IGameInputReading* reading)
{
    Array<GI::GameInputKeyState, 32> keys{};
    const u32 numKeys = reading->GetKeyState(keys.size(), keys.data());

    // Track what keys are present in THIS specific reading
    bool isDownNow[Keyboard::ButtonCount] = {};
    for (u32 i = 0; i < numKeys; ++i)
    {
        const Keyboard::Key k = MapKey(keys[i]);
        if (k != Keyboard::Unknown) isDownNow[k] = true;
    }

    // Process state changes
    for (i32 k = 0; k < Keyboard::ButtonCount; ++k)
    {
        if (input.keyboard[k].held != isDownNow[k])
        {
            Input::ProcessEventButton(input.keyboard[k], isDownNow[k]);
        }
    }
}


void InputSysGameInput::HandleMouse(GI::IGameInputReading* reading)
{
    GI::GameInputMouseState curr{};
    if (!reading->GetMouseState(&curr))
        return;

    // This is so the mouse won't jump
    if (!hasBaseline)
    {
        for (i32 i = 0; i < Mouse::ButtonCount; ++i)
        {
            const bool down = (curr.buttons & (1u << i)) != 0;
            Input::ProcessEventButton(input.mouseButtons[i], down);
        }

        lastState = curr;
        hasBaseline = true;
    }

    const GI::GameInputMouseState& prev = lastState;

    // i64 deltas (GameInputMouseState uses INT64)
    const i64 dx = curr.positionX - prev.positionX;
    const i64 dy = curr.positionY - prev.positionY;

    // Motion
    if (dx != 0 || dy != 0)
    {
        input.xrel = static_cast<f32>(dx);
        input.yrel = static_cast<f32>(dy);
        input.usingMouse = true;
    }

    // Buttons (XOR to detect changes)
    if (const u32 deltaButtons = curr.buttons ^ prev.buttons)
    {
        for (i32 i = 0; i < Mouse::ButtonCount; ++i)
        {
            const u32 mask = (1u << i);
            // Only process the SPECIFIC button that changed
            if (deltaButtons & mask)
            {
                const bool down = (curr.buttons & (1u << i)) != 0;
                Input::ProcessEventButton(input.mouseButtons[i], down);
                input.usingMouse = true;
            }
        }
    }

    // Wheel (normalize to ticks)
    const i64 dwx = curr.wheelX - prev.wheelX;
    const i64 dwy = curr.wheelY - prev.wheelY;

    if (dwx || dwy)
    {
        input.scrollX = dwx / WHEEL_DELTA;
        input.scrollY = dwy / WHEEL_DELTA;
        input.usingMouse = true;
    }

    lastState = curr;
}

void InputSysGameInput::HandleController(GI::IGameInputReading* reading)
{
    GI::GameInputGamepadState gs{};
    if (!reading->GetGamepadState(&gs))
        return;

    // Find Index
    GI::IGameInputDevice* device = nullptr;
    reading->GetDevice(&device);
    if (!device) return;

    i32 idx = -1;
    for (i32 i = 0; i < MAX_GAMEPADS; ++i)
    {
        if (i >= connectedCount && connectedCount > 0) break;
        if (gamepads[i] == device)
        {
            idx = i;
            break;
        }
    }
    device->Release();

    if (idx == -1) return;

    auto& controller = input.controllers[idx];
    controller.connected = true;

    // Buttons
    const u32 mask = MapButtons(gs);
    for (u32 b = 0; b < Gamepad::Button::ButtonCount; ++b)
        Input::ProcessEventButton(controller.buttons[b], (mask & (1 << b)) != 0);

    // Analog
    auto ApplyDeadzone = [](const f32 value, const f32 deadzone = 0.1f)
    {
        if (fabsf(value) < deadzone) return 0.0f;
        return (value - (value > 0 ? deadzone : -deadzone)) / (1.0f - deadzone);
    };

    controller.leftX = ApplyDeadzone(gs.leftThumbstickX);
    controller.leftY = ApplyDeadzone(gs.leftThumbstickY);
    controller.rightX = ApplyDeadzone(gs.rightThumbstickX);
    controller.rightY = ApplyDeadzone(gs.rightThumbstickY);

    controller.leftTrigger = gs.leftTrigger;
    controller.rightTrigger = gs.rightTrigger;


    // --- Activity tracking ---
    const bool moved =
        mask ||
        fabsf(gs.leftTrigger) > 0.001f ||
        fabsf(gs.rightTrigger) > 0.001f ||
        fabsf(gs.leftThumbstickX) > 0.001f ||
        fabsf(gs.leftThumbstickY) > 0.001f ||
        fabsf(gs.rightThumbstickX) > 0.001f ||
        fabsf(gs.rightThumbstickY) > 0.001f;

    if (moved)
        input.usingController = true;
}


void InputSysGameInput::Update(const Platform::WindowContext& windowContext)
{
    if (dispatcher)
        dispatcher->Dispatch(0);

    if (!windowContext.displayState.isFocused)
    {
        hasBaseline = false;
        if (prevReading)
        {
            prevReading->Release();
            prevReading = nullptr;
        }
        return;
    }

    // Accumulation resets (Deltas only)
    input.xrel = 0.0f;
    input.yrel = 0.0f;
    input.scrollX = 0;
    input.scrollY = 0;

    auto ProcessReading = [&](GI::IGameInputReading* r) {
        const GI::GameInputKind kind = r->GetInputKind();
        if (kind & GI::GameInputKindKeyboard)   HandleKeyboard(r);
        if (kind & GI::GameInputKindMouse)      HandleMouse(r);
        if (kind & GI::GameInputKindController) HandleController(r);
    };

    if (!prevReading)
    {
        if (SUCCEEDED(gi->GetCurrentReading(
            GI::GameInputKindKeyboard | GI::GameInputKindMouse | GI::GameInputKindGamepad,
            nullptr, &prevReading)))
        {
            ProcessReading(prevReading);
        }
    }
    if (prevReading)
    {
        while (true)
        {
            GI::IGameInputReading* nextReading = nullptr;
            HRESULT hr = gi->GetNextReading(prevReading,
                GI::GameInputKindKeyboard | GI::GameInputKindMouse | GI::GameInputKindGamepad,
                nullptr, &nextReading);

            if (SUCCEEDED(hr))
            {

                ProcessReading(nextReading);

                // Release the old bookmark before overwriting
                prevReading->Release();
                prevReading = nextReading;
            }
            else
            {
                // Buffer Empty? We are done.
                if (hr == GI::GAMEINPUT_E_READING_NOT_FOUND)
                {
                    break;
                }

                // Error/Too Old? Nuke it and restart next frame.
                prevReading->Release();
                prevReading = nullptr;
                break;
            }
        }
    }


    // Final physical state check
    bool anyKeyHeld = false;
    for (i32 i = 0; i < Keyboard::ButtonCount; ++i) {
        if (input.keyboard[i].held) {
            anyKeyHeld = true;
            break;
        }
    }

    input.usingKeyboard = anyKeyHeld;
}