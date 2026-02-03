//
// Created by Orgest on 10/1/2025.
//

#pragma once
#ifdef ENGINE_PLATFORM_WIN32
#include <GameInput.h>
#ifndef GAMEINPUT_API_VERSION
#define GAMEINPUT_API_VERSION 0
#endif

#include "Platform.h"
#include "Tools/Array.h"

#if GAMEINPUT_API_VERSION == 1
namespace GI = GameInput::v1;
#elif GAMEINPUT_API_VERSION == 2
namespace GI = GameInput::v2;
#elif GAMEINPUT_API_VERSION == 3
namespace GI = GameInput::v3;
#endif

struct InputSysGameInput
{
    bool Init();
    void Shutdown();
    void Update(const Platform::WindowContext& windowContext);

    GI::IGameInput* gi = nullptr;
    GI::IGameInputDispatcher* dispatcher = nullptr;
    GI::GameInputCallbackToken giToken = {};

    Array<GI::IGameInputDevice*, MAX_GAMEPADS> gamepads;
    i32 connectedCount = 0;


private:
    GI::IGameInputReading* prevReading = nullptr; // Unified bookmark for the buffer
    GI::GameInputMouseState lastState = {};
    bool hasBaseline = false;

    static void CALLBACK DeviceCallback(GI::GameInputCallbackToken token, void* context, GI::IGameInputDevice* device,
                                        u64 timestamp,
                                        GI::GameInputDeviceStatus currentStatus,
                                        GI::GameInputDeviceStatus previousStatus);
    void HandleMouse(GI::IGameInputReading* reading);
    void HandleController(GI::IGameInputReading* reading);
};

inline InputSysGameInput gameInput;
#endif
