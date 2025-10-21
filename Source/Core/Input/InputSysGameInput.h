//
// Created by Orgest on 10/1/2025.
//

#pragma once
#include <GameInput.h>

#include "Tools/Array.h"
struct Input;

namespace GI = GameInput::v2;

struct InputSysGameInput
{
	bool Init();
	void Shutdown();
	void Update();

	GI::IGameInput* gi = nullptr;
	GI::IGameInputDispatcher* dispatcher = nullptr;
	GI::GameInputCallbackToken giToken = {};
	Array<GI::IGameInputDevice*, MAX_GAMEPADS> gamepads;
	Array<bool, MAX_GAMEPADS> hasGamepad;
	u32 connectedCount = 0;

	static void CALLBACK DeviceCallback(
	   GI::GameInputCallbackToken token,
	   void*                      context,
	   GI::IGameInputDevice*      device,
	   uint64_t                   timestamp,
	   GI::GameInputDeviceStatus  currentStatus,
	   GI::GameInputDeviceStatus  previousStatus);
};

inline InputSysGameInput gameInput;