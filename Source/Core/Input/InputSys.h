//
// Created by Orgest on 7/26/2025.
//

#pragma once
#include "Tools/Array.h"

constexpr u32 CONTROLLER_COUNT = 1;
constexpr f32 MAX_THUMB_VALUE = 32767.0f;
constexpr f32 NORM_THUMB_VALUE = 1.0f / 32767.0f;
#define NORM_THUMB(v) (static_cast<f32>(v) / MAX_THUMB_VALUE)

namespace Mouse
{
    enum Button : u32
    {
        Left = 0,
        Right,
        Middle,
        Button4,
        Button5,
        ButtonCount
    };
}

namespace Gamepad
{
    enum Button : u32
    {
        A, B, X, Y,
        Start, Select,
        L1, R1,
        L2, R2,
        L3, R3,
        DpadUp, DpadDown, DpadLeft, DpadRight,
        LeftShoulder, RightShoulder,
        LeftThumb, RightThumb,
        ButtonCount
    };
} // namespace Gamepad

namespace Keyboard
{
    enum Key : u32
    {
        Unknown = 0,

        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

        Shift, Ctrl, Alt,

        Plus_Equal, Minus_Underscore, Period_RightArrow, Comma_LeftArrow, SemiColon, Question_BackSlash,
        Tilde, Quotes, SquareBracketsOpen, SquareBracketsClose, Backslash, Slash,

        Tab, Enter, Escape, Backspace, Space,
        Insert, Delete, Home, End,
        Up, Down, Left, Right,

        F1, F2, F3, F4, F5, F6,
        F7, F8, F9, F10, F11, F12,

        CapsLock, NumLock, ScrollLock,
        PrintScreen, Pause, Menu,

        ButtonCount
    };
} // namespace Keyboard

struct ButtonState
{
    u8 pressed : 1;
    u8 held : 1;
    u8 released : 1;
    u8 unused : 5;
};

struct InputConfig
{
    f32 mouseSensitivity = .5f;
    f32 thumbstickSensitivity = 0.5f;
    f32 panSensitivity = mouseSensitivity / 2;
    f32 movementSpeed = 70.0f;
};

struct ControllerState
{
    Array<ButtonState, Gamepad::Button::ButtonCount> buttons;
    f32 leftTrigger, rightTrigger;
    f32 leftX, leftY, rightX, rightY;
    f32 leftMotorVibration, rightMotorVibration;
    u32 lastPacket;
    bool connected;
};

struct ActionBinding
{
    Keyboard::Key key = Keyboard::Unknown;
    Gamepad::Button button = Gamepad::Button::ButtonCount;
    Mouse::Button mouse = Mouse::ButtonCount;
};

enum class Action : u32
{
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Jump,

    ToggleFPS,
    ToggleDebug,
    ToggleMenuBar,
    ToggleGPUInfo,
    ToggleVSync,
    ToggleUI,
    ToggleFrustum,
    CycleCamera,
    CycleDebugView,

    ActionCount
};

struct Input
{
    Array<ButtonState, Keyboard::Key::ButtonCount> keyboard;
    Array<ButtonState, Mouse::Button::ButtonCount> mouseButtons;
    Array<ControllerState, CONTROLLER_COUNT> controllers;

    Array<ActionBinding, static_cast<size_t>(Action::ActionCount)> bindings;

    void BindAction(Action act, Keyboard::Key k) { bindings[static_cast<u32>(act)].key = k; }
    void BindAction(Action act, Gamepad::Button b) { bindings[static_cast<u32>(act)].button = b; }

    // Logic Helpers
    [[nodiscard]] bool IsActionDown(Action act) const {
        const auto& b = bindings[static_cast<u32>(act)];

        bool active = false;
        if (b.key != Keyboard::Unknown) active |= IsKeyDown(b.key);
        if (b.button < Gamepad::Button::ButtonCount) active |= IsControllerDown(b.button);
        if (b.mouse < Mouse::Button::ButtonCount) active |= IsMouseDown(b.mouse);

        return active;
    }

    [[nodiscard]] bool IsActionHeld(Action act) const {
        const auto& b = bindings[static_cast<u32>(act)];

        bool active = false;
        if (b.key != Keyboard::Unknown) active |= IsKeyHeld(b.key);
        if (b.button < Gamepad::Button::ButtonCount) active |= IsControllerHeld(b.button);
        if (b.mouse < Mouse::Button::ButtonCount) active |= IsMouseHeld(b.mouse);

        return active;
    }

    f32 cursorX = 0.0f;
    f32 cursorY = 0.0f;
    f64 xrel = 0.0f, yrel = 0.0f;
    f64 lastX = 0.0f, lastY = 0.0f;
    i64 scrollX = 0, scrollY = 0;
    i64 prevWheelX = 0, prevWheelY = 0;

    bool usingController = false;
    bool usingKeyboard = false;
    bool usingMouse = false;
    bool useRawInput = false;
    bool mouseLookActive = false;

    // Methods
    static void ProcessEventButton(ButtonState& state, bool isPressed);
    static void EndFrameInputUpdate();
    static void ResetInputOnFocusLoss();
    static void ProcessEvents();

    // Access helpers (instead of my usual way)
    [[nodiscard]] bool IsKeyDown(Keyboard::Key k) const { return keyboard[k].pressed; }
    [[nodiscard]] bool IsKeyHeld(Keyboard::Key k) const { return keyboard[k].held; }
    [[nodiscard]] bool IsKeyReleased(Keyboard::Key k) const { return keyboard[k].released; }

    [[nodiscard]] bool IsMouseDown(Mouse::Button button) const { return mouseButtons[button].pressed; }
    [[nodiscard]] bool IsMouseHeld(Mouse::Button button) const { return mouseButtons[button].held; }
    [[nodiscard]] bool IsMouseReleased(Mouse::Button button) const { return mouseButtons[button].pressed; }

    [[nodiscard]] bool IsControllerDown(Gamepad::Button b, u32 idx = 0) const {
        return controllers[idx].buttons[b].pressed;
    }
    [[nodiscard]] bool IsControllerHeld(Gamepad::Button b, u32 idx = 0) const {
        return controllers[idx].buttons[b].held;
    }

    [[nodiscard]] f32 GetLeftStickX(u32 idx = 0) const { return controllers[idx].leftX; }
    [[nodiscard]] f32 GetLeftStickY(u32 idx = 0) const { return controllers[idx].leftY; }
    [[nodiscard]] f32 GetRightStickX(u32 idx = 0) const { return controllers[idx].rightX; }
    [[nodiscard]] f32 GetRightStickY(u32 idx = 0) const { return controllers[idx].rightY; }

    [[nodiscard]] f32 GetLeftTrigger(u32 idx = 0) const { return controllers[idx].leftTrigger; }
    [[nodiscard]] f64 GetRightTrigger(u32 idx = 0) const { return controllers[idx].rightTrigger; }

};

inline Input input;