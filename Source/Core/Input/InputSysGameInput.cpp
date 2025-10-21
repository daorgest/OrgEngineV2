//
// Created by Orgest on 10/1/2025.
//

#include "InputSysGameInput.h"

#include "InputSys.h"

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

	// modifiers (GameInput may give L/R or generic VKs — map both to your single key)
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
	(void)k.codePoint; // useful for text input; not needed for action mapping
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
		gi->RegisterDeviceCallback(nullptr, GI::GameInputKindGamepad, GI::GameInputDeviceConnected, GI::GameInputAsyncEnumeration,
		                           this,
		                           DeviceCallback,
		                           &giToken);
	}

	return true;
}


// Matches typedef: void CALLBACK(...)
void CALLBACK InputSysGameInput::DeviceCallback(GI::GameInputCallbackToken /*token*/, void* context, GI::IGameInputDevice* device,
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
				self->hasGamepad[i] = true;
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
				self->hasGamepad[i] = false;
			}
		}
	}

	// recompute
	self->connectedCount = 0;
	for (u32 i = 0; i < MAX_GAMEPADS; ++i)
		if (self->hasGamepad[i]) ++self->connectedCount;
}

void InputSysGameInput::Update()
{
	input.usingKeyboard = false;
	input.usingMouse = false;
	input.usingController = false;

	{
		// Keyboard
		if (GI::IGameInputReading* r = {};
			!HR_FAIL(gi->GetCurrentReading(GI::GameInputKindKeyboard, nullptr, &r)) && (r != nullptr))
		{
			for (int k = 0; k < Keyboard::ButtonCount; ++k)
				input.keyboard[k].seenThisFrame = false;

			GI::GameInputKeyState keys[32];
			const u32 n = r->GetKeyState(std::size(keys), keys);

			for (u32 i = 0; i < n; ++i)
			{
				const auto key = MapKey(keys[i]);
				if (key == Keyboard::Unknown) continue;

				input.keyboard[key].seenThisFrame = true;
				Input::ProcessEventButton(input.keyboard[key], true);
			}
			// mark releases for keys not seen this frame
			for (int k = 0; k < Keyboard::ButtonCount; ++k)
			{
				if (input.keyboard[k].held && !input.keyboard[k].seenThisFrame)
				{
					Input::ProcessEventButton(input.keyboard[k], false);
				}
			}

			input.usingKeyboard = (n > 0);
			r->Release();
		}
	}

	// Controllers
	for (u32 i = 0; i < MAX_GAMEPADS; ++i)
	{
		if (!hasGamepad[i]) continue;

		if (GI::IGameInputReading* r = nullptr;
			!HR_FAIL(gi->GetCurrentReading(GI::GameInputKindGamepad, gamepads[i], &r)) && r)
		{
			GI::GameInputGamepadState gs{};
			if (!HR_FAIL(r->GetGamepadState(&gs)))
			{
				const u32 mask = MapButtons(gs);
				for (u32 b = 0; b < Gamepad::Button::ButtonCount; ++b)
					Input::ProcessEventButton(input.gamepadButtons[b], (mask & (1 << b)) != 0);

				const bool moved =
					(gs.leftTrigger > 0.001f) || (gs.rightTrigger > 0.001f) ||
					(fabsf(gs.leftThumbstickX) > 0.001f) || (fabsf(gs.leftThumbstickY) > 0.001f) ||
					(fabsf(gs.rightThumbstickX) > 0.001f) || (fabsf(gs.rightThumbstickY) > 0.001f);

				if (moved || mask) input.usingController = true;
			}
			r->Release();
		}
	}

	// reset deltas
	input.xrel = input.yrel = 0.0f;
	input.scrollX = input.scrollY = 0;

	// Mouse
	if (GI::IGameInputReading* r = nullptr; !HR_FAIL(gi->GetCurrentReading(GI::GameInputKindMouse, nullptr, &r)) && r)
	{
		GI::GameInputMouseState ms{};
		if (r->GetMouseState(&ms))
		{
			// mouse delta update
			const f64 currentX = static_cast<f64>(ms.positionX);
			const f64 currentY = static_cast<f64>(ms.positionY);
			input.xrel = currentX - input.lastX;
			input.yrel = currentY - input.lastY;
			input.lastX = currentX;
			input.lastY = currentY;

			// wheel delta update
			const int64_t dWheelX = ms.wheelX - input.prevWheelX;
			const int64_t dWheelY = ms.wheelY - input.prevWheelY;
			input.prevWheelX = ms.wheelX;
			input.prevWheelY = ms.wheelY;
			input.scrollX = dWheelX;
			input.scrollY = dWheelY;

			auto setBtn = [&](int idx, bool down)
			{
				Input::ProcessEventButton(input.mouseButtons[idx], down);
			};
			setBtn(Mouse::Left, (ms.buttons & GI::GameInputMouseLeftButton) != 0);
			setBtn(Mouse::Right, (ms.buttons & GI::GameInputMouseRightButton) != 0);
			setBtn(Mouse::Middle, (ms.buttons & GI::GameInputMouseMiddleButton) != 0);
			setBtn(Mouse::Button4, (ms.buttons & GI::GameInputMouseButton4) != 0);
			setBtn(Mouse::Button5, (ms.buttons & GI::GameInputMouseButton5) != 0);

			const bool anyMotion = (input.xrel != 0.0f) || (input.yrel != 0.0f) || (input.scrollX != 0) || (input.scrollY != 0);
			const bool anyButton = (ms.buttons != GI::GameInputMouseNone);
			if (anyMotion || anyButton) input.usingMouse = true;
		}
		r->Release();
	}
}
