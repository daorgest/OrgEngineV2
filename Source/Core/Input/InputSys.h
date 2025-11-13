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
    enum Button : u8
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
    enum Button : u16
    {
        A, B, X, Y,
        Start, Select,
        L1, R1,
        L3, R3,
        DpadUp, DpadDown, DpadLeft, DpadRight,
        LeftShoulder, RightShoulder,
        LeftThumb, RightThumb,
        ButtonCount
    };
} // namespace Gamepad

namespace Keyboard
{
    enum Key : u16
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

struct Input
{
    Array<ButtonState, Keyboard::Key::ButtonCount> keyboard;
    Array<ButtonState, Mouse::Button::ButtonCount> mouseButtons;
    Array<ControllerState, CONTROLLER_COUNT> controllers;

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
};

inline Input input;