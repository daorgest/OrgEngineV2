//
// Created by Orgest on 10/1/2025.
//

#include "InputSysGameInput.h"

#include "InputSys.h"
#include "Platform.h"

#define HR_FAIL(hr) (FAILED((hr)))

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

    // Fallback: unknown
    (void)k.scanCode;
    (void)k.codePoint;
    (void)k.isDeadKey;
    return Keyboard::Unknown;
}


bool InputSysGameInput::Init()
{
    if (FAILED(GameInputCreate(&gi)))
    {
        return false;
    }

    gi->CreateDispatcher(&dispatcher); // for hotplugging
    if (dispatcher != nullptr)
    {
        gi->RegisterDeviceCallback(nullptr, GI::GameInputKindGamepad, GI::GameInputDeviceConnected,
                                   GI::GameInputAsyncEnumeration,
                                   this,
                                   DeviceCallback,
                                   &giToken);
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
}

void InputSysGameInput::HandleKeyboard(GI::IGameInputReading* reading)
{
    static bool lastState[Keyboard::ButtonCount] = {};
    bool currState[Keyboard::ButtonCount] = {};

    Array<GI::GameInputKeyState, 256> keys{};
    const u32 numKeys = reading->GetKeyState(keys.size(), keys.data());

    // Build current key bitmap
    for (u32 i = 0; i < numKeys; ++i)
    {
        const Keyboard::Key k = MapKey(keys[i]);
        if (k != Keyboard::Unknown)
            currState[k] = true;
    }

    // Diff vs last -> pressed/released; sustain held
    for (int k = 0; k < Keyboard::ButtonCount; ++k)
    {
        const bool was = lastState[k];
        const bool now = currState[k];

        if (now && !was) Input::ProcessEventButton(input.keyboard[k], true);
        if (!now && was) Input::ProcessEventButton(input.keyboard[k], false);
        if (now && was) input.keyboard[k].held = true;
    }

    std::memcpy(lastState, currState, sizeof(currState));
    input.usingKeyboard = (numKeys > 0);
}

void InputSysGameInput::InitialMouseReading(GI::IGameInputReading* reading)
{
    GI::GameInputMouseState state{};
    if (!reading->GetMouseState(&state))
        return;

    // Initialize buttons to current state (no synthetic presses on frame 0)
    for (int i = 0; i < 5; ++i)
    {
        const bool down = (state.buttons & (1u << i)) != 0;
        Input::ProcessEventButton(input.mouseButtons[i], down);
    }

    lastMouseState_ = state;
    haveMouseBaseline_ = true;
}


void InputSysGameInput::HandleMouse(GI::IGameInputReading* reading)
{
    GI::GameInputMouseState curr{};
    if (!reading->GetMouseState(&curr))
        return;

    const GI::GameInputMouseState& prev = lastMouseState_;

    // i64 deltas (GameInputMouseState uses INT64)
    const i64 dx = curr.positionX - prev.positionX;
    const i64 dy = curr.positionY - prev.positionY;
    const i64 dwx = curr.wheelX - prev.wheelX;
    const i64 dwy = curr.wheelY - prev.wheelY;

    // Motion
    if (dx || dy)
    {
        input.xrel = static_cast<f32>(dx);
        input.yrel = static_cast<f32>(dy);
        input.usingMouse = true;
    }

    // Buttons (XOR to detect changes)
    if (const u32 deltaButtons = curr.buttons ^ prev.buttons)
    {
        for (int i = 0; i < 5; ++i)
        {
            const u32 mask = (1u << i);
            if (deltaButtons & mask)
            {
                const bool down = (curr.buttons & mask) != 0;
                Input::ProcessEventButton(input.mouseButtons[i], down);
                input.usingMouse = true;
            }
        }
    }

    // Wheel (normalize to ticks)
    if (dwx || dwy)
    {
        input.scrollX = static_cast<i64>(dwx / WHEEL_DELTA);
        input.scrollY = static_cast<i64>(dwy / WHEEL_DELTA);
        input.usingMouse = true;
    }

    lastMouseState_ = curr;
}

void InputSysGameInput::HandleController(GI::IGameInputReading* reading)
{
    GI::GameInputGamepadState gs{};
    if (!reading->GetGamepadState(&gs))
        return;

    auto& controller = input.controllers[0]; // primary pad slot
    controller.connected = true;

    // --- Buttons ---
    const u32 mask = MapButtons(gs);
    for (u32 b = 0; b < Gamepad::Button::ButtonCount; ++b)
        Input::ProcessEventButton(controller.buttons[b], (mask & (1 << b)) != 0);

    // --- Analog ---
    controller.leftTrigger = gs.leftTrigger;
    controller.rightTrigger = gs.rightTrigger;
    controller.leftX = gs.leftThumbstickX;
    controller.leftY = gs.leftThumbstickY;
    controller.rightX = gs.rightThumbstickX;
    controller.rightY = gs.rightThumbstickY;

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

    input.usingKeyboard = false;
    input.usingMouse = false;
    input.usingController = false;

    const bool focused = windowContext.displayState.isFocused;

    input.xrel = 0.0f;
    input.yrel = 0.0f;
    input.scrollX = 0;
    input.scrollY = 0;

    // ============================================
    // KEYBOARD
    // ============================================
    {
        GI::IGameInputReading* r = nullptr;
        if (SUCCEEDED(gi->GetCurrentReading(GI::GameInputKindKeyboard, nullptr, &r)) && r)
        {
            HandleKeyboard(r);
            r->Release();
        }
        else
        {
            // sustain any held keys
            bool any = false;
            for (int i = 0; i < Keyboard::ButtonCount; ++i)
                if (input.keyboard[i].held)
                    any = true;

            input.usingKeyboard = any;
        }
    }

    // ============================================
    // GAMEPAD
    // ============================================
    for (u32 i = 0; i < MAX_GAMEPADS; ++i)
    {
        if (gamepads[i])
        {
            GI::IGameInputReading* r = nullptr;
            if (SUCCEEDED(gi->GetCurrentReading(GI::GameInputKindGamepad, gamepads[i], &r)) && r)
            {
                HandleController(r);
                r->Release();
            }
        }
    }

    // ============================================
    // MOUSE (STATE-ONLY, NO QUEUE)
    // ============================================
    if (focused)
    {
        GI::IGameInputReading* r = nullptr;

        if (SUCCEEDED(gi->GetCurrentReading(GI::GameInputKindMouse, nullptr, &r)) && r)
        {
            if (!haveMouseBaseline_)
            {
                // First frame after focus -> baseline only
                InitialMouseReading(r);
            }
            else
            {
                // Normal per-frame diff
                HandleMouse(r);
            }

            // Replace stored reading
            if (lastMouseReading_)
                lastMouseReading_->Release();

            lastMouseReading_ = r;
        }
    }
    else
    {
        // Lost focus → freeze input + clear baseline
        Input::ResetInputOnFocusLoss();

        input.xrel = 0;
        input.yrel = 0;
        input.scrollX = 0;
        input.scrollY = 0;

        if (lastMouseReading_)
        {
            lastMouseReading_->Release();
            lastMouseReading_ = nullptr;
        }

        haveMouseBaseline_ = false;
    }
}