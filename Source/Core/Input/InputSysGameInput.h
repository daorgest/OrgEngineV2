//
// Created by Orgest on 10/1/2025.
//

#pragma once
#include <GameInput.h>

#include "Platform.h"
#include "Tools/Array.h"
struct Input;

namespace GI = GameInput::v2;

struct InputSysGameInput
{
    bool Init();
    void Shutdown();
    void Update(const Platform::WindowContext& windowContext);

    GI::IGameInput* gi = nullptr;
    GI::IGameInputDispatcher* dispatcher = nullptr;
    GI::GameInputCallbackToken giToken = {};
    Array<GI::IGameInputDevice*, MAX_GAMEPADS> gamepads;

private:
    GI::IGameInputReading* lastMouseReading_ = nullptr;
    GI::GameInputMouseState lastMouseState_{}; // baseline for deltas
    bool haveMouseBaseline_ = false;
    u32 connectedCount = 0;
    GI::IGameInputReading* lastKeyboardReading_ = nullptr;

    static void CALLBACK DeviceCallback(GI::GameInputCallbackToken token, void* context, GI::IGameInputDevice* device,
                                        u64 timestamp,
                                        GI::GameInputDeviceStatus currentStatus,
                                        GI::GameInputDeviceStatus previousStatus);
    void HandleKeyboard(GI::IGameInputReading* reading);
    void InitialMouseReading(GI::IGameInputReading* reading);
    void HandleMouse(GI::IGameInputReading* reading);
    void HandleController(GI::IGameInputReading* reading);
};

inline InputSysGameInput gameInput;