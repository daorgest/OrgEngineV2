//
// Created by Orgest on 6/8/2025.
//
#define WIN32_LEAN_AND_MEAN
// #undef _WIN32_WINNT
// #define _WIN32_WINNT 0x0A00


#include <Windows.h>

// Xbox stuff
#include <algorithm>
#include <xinput.h>

#include "Input/InputSys.h"
static HMODULE xinputLib = nullptr;
static DWORD (WINAPI* XInputGetStateFn)(DWORD, XINPUT_STATE*) = nullptr;
static DWORD (WINAPI* XInputSetStateFn)(DWORD, XINPUT_VIBRATION*) = nullptr;
// Vibration handling
constexpr u16 MAX_VIBRATION = UINT16_MAX;

#include <fmt/core.h>

#ifdef EDITORUI
	#include <imgui_impl_win32.h>
#endif

#include "Arena.h"
#include "Platform.h"
#include "tracy/Tracy.hpp"

// Ensure we extract signed coordinate data (multi-monitor support) (Dont want to import the whole of <windowsx.h>)
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

// Force GPU To be on Dedicated for laptops!
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
	__declspec(dllexport) void NoHotPatch() {} // Disable Nahimic code injection.
}

// Forward Declare up here for init
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK GlobalWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

// Input handling Win32
static void LoadXInput()
{
	if (xinputLib != nullptr) return;

	xinputLib = LoadLibraryA("xinput1_4.dll");
	if (!xinputLib) xinputLib = LoadLibraryA("xinput9_1_0.dll"); // Fallback,

	if (xinputLib != nullptr)
	{
		XInputGetStateFn = reinterpret_cast<decltype(XInputGetStateFn)>(GetProcAddress(xinputLib, "XInputGetState"));
		XInputSetStateFn = reinterpret_cast<decltype(XInputSetStateFn)>(GetProcAddress(xinputLib, "XInputSetState"));
		fprintf(stderr, "[Win32] XInput loaded from library.\n");
	} else
	{
		fprintf(stderr, "[Win32] Failed to load XInput library.\n");
	}
}

void Platform::UpdateGamepads()
{
    if (XInputGetStateFn == nullptr) return;

    for (u32 ctrlIdx = 0; ctrlIdx < CONTROLLER_COUNT; ctrlIdx++)
    {
        XINPUT_STATE state{};

        if (XInputGetStateFn(ctrlIdx, &state) == ERROR_SUCCESS)
        {
            if (state.dwPacketNumber == input.lastPacketNumbers[ctrlIdx])
                continue;

            input.lastPacketNumbers[ctrlIdx] = state.dwPacketNumber;
            const XINPUT_GAMEPAD& gamepad = state.Gamepad;

            using namespace Gamepad;
            Input::ProcessEventButton(input.gamepadButtons[Button::A], gamepad.wButtons & XINPUT_GAMEPAD_A);
            Input::ProcessEventButton(input.gamepadButtons[Button::B], gamepad.wButtons & XINPUT_GAMEPAD_B);
            Input::ProcessEventButton(input.gamepadButtons[Button::X], gamepad.wButtons & XINPUT_GAMEPAD_X);
            Input::ProcessEventButton(input.gamepadButtons[Button::Y], gamepad.wButtons & XINPUT_GAMEPAD_Y);
            Input::ProcessEventButton(input.gamepadButtons[Button::Start], gamepad.wButtons & XINPUT_GAMEPAD_START);
            Input::ProcessEventButton(input.gamepadButtons[Button::Select], gamepad.wButtons & XINPUT_GAMEPAD_BACK);
            Input::ProcessEventButton(input.gamepadButtons[Button::LeftShoulder], gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
            Input::ProcessEventButton(input.gamepadButtons[Button::RightShoulder], gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
            Input::ProcessEventButton(input.gamepadButtons[Button::DpadUp], gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
            Input::ProcessEventButton(input.gamepadButtons[Button::DpadDown], gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            Input::ProcessEventButton(input.gamepadButtons[Button::DpadLeft], gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
            Input::ProcessEventButton(input.gamepadButtons[Button::DpadRight], gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

            input.thumbLeftX  = gamepad.sThumbLX;
            input.thumbLeftY  = gamepad.sThumbLY;
            input.thumbRightX = gamepad.sThumbRX;
            input.thumbRightY = gamepad.sThumbRY;
            input.leftTrigger  = gamepad.bLeftTrigger;
            input.rightTrigger = gamepad.bRightTrigger;

        	// Deadzone stuff
        	auto applyDeadzone = [](i16& x, i16& y, const f32 deadzone)
        	{
        		const f32 dx = x;
        		const f32 dy = y;
        		const f32 magnitudeSq = (dx * dx) + (dy * dy);
        		const f32 deadzoneSq = deadzone * deadzone;

        		if (magnitudeSq < deadzoneSq)
        		{
        			x = 0;
        			y = 0;
        		}
        		else
        		{
        			const f32 magnitude = std::sqrtf(magnitudeSq);
        			f32 scale = (magnitude - deadzone) / (MAX_THUMB_VALUE - deadzone); // 32767 is max
        			scale = std::clamp(scale, 0.0f, 1.0f);
        			x = static_cast<i16>(dx * scale);
        			y = static_cast<i16>(dy * scale);
        		}
        	};

        	applyDeadzone(input.thumbLeftX,  input.thumbLeftY,  XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        	applyDeadzone(input.thumbRightX, input.thumbRightY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

        	// Vibration
            if ((XInputSetStateFn != nullptr) && (input.leftMotorVibration > 0.0f || input.rightMotorVibration > 0.0f))
            {
                XINPUT_VIBRATION vib = {
                    .wLeftMotorSpeed  = static_cast<WORD>(input.leftMotorVibration * MAX_VIBRATION),
                    .wRightMotorSpeed = static_cast<WORD>(input.rightMotorVibration * MAX_VIBRATION)
                };
                XInputSetStateFn(ctrlIdx, &vib);
            }
            break;
        }

        input.usingController = false;
        input.gamepadButtons.reset();
        input.lastPacketNumbers[ctrlIdx] = 0;
        input.thumbLeftX = input.thumbLeftY = 0;
        input.thumbRightX = input.thumbRightY = 0;
        input.leftTrigger = input.rightTrigger = 0;

    }
}

Keyboard::Key MapKeys(WPARAM wParam, LPARAM lParam)
{
	const u8 scancode = (lParam >> 16) & 0xFF;
	const bool isExtended = (lParam >> 24) & 1;

	// Handle extended and special keys
	switch (wParam)
	{
		case VK_ESCAPE: return Keyboard::Key::Escape;
		case VK_TAB:    return Keyboard::Key::Tab;
		case VK_RETURN: return Keyboard::Key::Enter;
		case VK_BACK:   return Keyboard::Key::Backspace;
		case VK_SPACE:  return Keyboard::Key::Space;

		case VK_INSERT: return Keyboard::Key::Insert;
		case VK_DELETE: return Keyboard::Key::Delete;
		case VK_HOME:   return Keyboard::Key::Home;
		case VK_END:    return Keyboard::Key::End;
		case VK_LEFT:   return Keyboard::Key::Left;
		case VK_RIGHT:  return Keyboard::Key::Right;
		case VK_UP:     return Keyboard::Key::Up;
		case VK_DOWN:   return Keyboard::Key::Down;

		case VK_SHIFT:
			return (scancode == 0x36) ? Keyboard::Key::Shift : Keyboard::Key::Shift;
		case VK_CONTROL:
			return isExtended ? Keyboard::Key::Ctrl : Keyboard::Key::Ctrl;
		case VK_MENU:
			return isExtended ? Keyboard::Key::Alt : Keyboard::Key::Alt;

		case VK_F1:  return Keyboard::Key::F1;
		case VK_F2:  return Keyboard::Key::F2;
		case VK_F3:  return Keyboard::Key::F3;
		case VK_F4:  return Keyboard::Key::F4;
		case VK_F5:  return Keyboard::Key::F5;
		case VK_F6:  return Keyboard::Key::F6;
		case VK_F7:  return Keyboard::Key::F7;
		case VK_F8:  return Keyboard::Key::F8;
		case VK_F9:  return Keyboard::Key::F9;
		case VK_F10: return Keyboard::Key::F10;
		case VK_F11: return Keyboard::Key::F11;
		case VK_F12: return Keyboard::Key::F12;

		case VK_CAPITAL: return Keyboard::Key::CapsLock;
		case VK_NUMLOCK: return Keyboard::Key::NumLock;
		case VK_SCROLL:  return Keyboard::Key::ScrollLock;
		case VK_PRINT:   return Keyboard::Key::PrintScreen;
		case VK_PAUSE:   return Keyboard::Key::Pause;
		case VK_APPS:    return Keyboard::Key::Menu;

		case VK_OEM_1: return Keyboard::Key::SemiColon;             // ; :
		case VK_OEM_2: return Keyboard::Key::Question_BackSlash;    // / ?
		case VK_OEM_3: return Keyboard::Key::Tilde;                 // ` ~
		case VK_OEM_4: return Keyboard::Key::SquareBracketsOpen;   // [
		case VK_OEM_5: return Keyboard::Key::Backslash;            // \ |
		case VK_OEM_6: return Keyboard::Key::SquareBracketsClose;  // ]
		case VK_OEM_7: return Keyboard::Key::Quotes;               // ' "
		case VK_OEM_COMMA:  return Keyboard::Key::Comma_LeftArrow;    // ,
		case VK_OEM_PERIOD: return Keyboard::Key::Period_RightArrow;  // .
		case VK_OEM_PLUS:   return Keyboard::Key::Plus_Equal;         // =
		case VK_OEM_MINUS:  return Keyboard::Key::Minus_Underscore;   // -

		default: break;
	}

	// Handle A–Z
	if (wParam >= 'A' && wParam <= 'Z')
		return static_cast<Keyboard::Key>(Keyboard::Key::A + (wParam - 'A'));

	// Handle 0–9
	if (wParam >= '0' && wParam <= '9')
		return static_cast<Keyboard::Key>(Keyboard::Key::Num0 + (wParam - '0'));

	return Keyboard::Key::Unknown;
}


void Platform::Init(WindowContext* window, i32 width, i32 height, DisplayMode mode)
{
	ZoneScopedN("Init Win32 Platform");
	window->windowWidth   = width;
	window->windowHeight  = height;
	window->monitorWidth  = GetSystemMetrics(SM_CXSCREEN);
	window->monitorHeight = GetSystemMetrics(SM_CYSCREEN);
	window->platformName  = "Win32";
	window->positionX     = CW_USEDEFAULT;
	window->positionY     = CW_USEDEFAULT;
	// TODO ORGEST: this is fine for now, but will have to test for multiple screens/devices
	width == 0 ? window->windowWidth = window->monitorWidth : window->windowWidth = width;
	height == 0 ? window->windowHeight = window->monitorHeight : window->windowHeight = height;

	// Init timer frequency
	LARGE_INTEGER perfFrequency;
	QueryPerformanceFrequency(&perfFrequency);
	window->perfCountFrequency = perfFrequency.QuadPart;

	// Title (will be changed in the future)!
	const std::string title = fmt::format("{} - {} - {} - {} - {} - {}", ENGINE_NAME, ENGINE_BUILD, window->platformName,
	                                      ENGINE_VERSION, __DATE__, ENGINE_COMMIT_HASH);
	std::wstring widePlatformName = ConvertToWideString(title);

	// Init window class!
	WNDCLASSEX wc = {
		.cbSize = sizeof(wc),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = GlobalWndProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = GetModuleHandle(nullptr),
		.hIcon = nullptr,
		.hCursor = LoadCursor(nullptr, IDC_ARROW),
		.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
		.lpszMenuName = nullptr,
		.lpszClassName =  widePlatformName.c_str(),
		.hIconSm = nullptr
	};

	if (!RegisterClassEx(&wc))
	{
		ShowMessageBox("Failed to register window class.", "Error", MessageBoxType::Error);
	}

	const DWORD windowStyle = (mode == DisplayMode::BorderlessFullscreen) ? WS_POPUP | WS_VISIBLE : WS_OVERLAPPEDWINDOW;

	// Creating the handle!
	window->handle = CreateWindowEx(
		0,
		widePlatformName.c_str(),
		widePlatformName.c_str(),
		windowStyle,
		window->positionX,
		window->positionY,
		window->windowWidth,
		window->windowHeight,
		nullptr,
		nullptr,
		GetModuleHandle(nullptr),
		window
	);

	if (!window->handle)
	{
		ShowMessageBox("Failed to create window.", "Error", MessageBoxType::Error);
	}

	// for laptops/double checking
	HMONITOR hMonitor = MonitorFromWindow(static_cast<HWND>(window->handle), MONITOR_DEFAULTTONEAREST);
	MONITORINFOEX monitorInfo = {};
	monitorInfo.cbSize = sizeof(MONITORINFOEX);
	if (GetMonitorInfo(hMonitor, &monitorInfo))
	{
		window->monitorWidth  = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
		window->monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
	}
	else
	{
		window->monitorWidth  = GetSystemMetrics(SM_CXSCREEN);
		window->monitorHeight = GetSystemMetrics(SM_CYSCREEN);
	}

	// Dpi!!
	window->dpiScale = static_cast<float>(GetDpiForWindow(static_cast<HWND>(window->handle))) / USER_DEFAULT_SCREEN_DPI;

	// Xinput!!
	LoadXInput();
}

bool Platform::GetWindowSize(const WindowHandle& handle, u32& width, u32& height)
{
	RECT rect;
	GetClientRect(static_cast<HWND>(handle), &rect);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;
	return true;
}

void Platform::SetDisplayMode(WindowContext& window, DisplayMode mode)
{
	static RECT previousWindowRect{};
    static DWORD previousWindowStyle = 0;
    static DWORD previousWindowExStyle = 0;

    HWND hwnd = static_cast<HWND>(window.handle);

    if (mode == DisplayMode::Fullscreen)
    {
        window.displayState.isExclusiveFullscreen = true;
        window.displayMode = DisplayMode::Fullscreen;

        ShowWindow(hwnd, SW_MINIMIZE); // optional: minimize first
        return;
    }

	if (mode == DisplayMode::Windowed && window.displayState.isBorderless)
	{
		SetWindowLong(hwnd, GWL_STYLE, previousWindowStyle);
		SetWindowLong(hwnd, GWL_EXSTYLE, previousWindowExStyle);
		SetWindowPos(hwnd, nullptr,
		             previousWindowRect.left,
		             previousWindowRect.top,
		             previousWindowRect.right - previousWindowRect.left,
		             previousWindowRect.bottom - previousWindowRect.top,
		             SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

		window.displayState.isBorderless = false;
		window.displayMode               = DisplayMode::Windowed;
	}
	else if (mode == DisplayMode::BorderlessFullscreen)
	{
		if (previousWindowRect.right == 0 && previousWindowRect.bottom == 0)
		{
			GetWindowRect(hwnd, &previousWindowRect);
			previousWindowStyle   = GetWindowLong(hwnd, GWL_STYLE);
			previousWindowExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
		}

		SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW);
		window.displayState.isBorderless = true;

		MONITORINFO monitorInfo = {sizeof(MONITORINFO)};
		if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
		{
			SetWindowPos(hwnd, nullptr,
			             monitorInfo.rcMonitor.left,
			             monitorInfo.rcMonitor.top,
			             monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			             monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			             SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}

        window.displayState.isBorderless = true;
        window.displayMode = DisplayMode::BorderlessFullscreen;
    }
}



LRESULT CALLBACK GlobalWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (msg == WM_NCCREATE)
	{
		const auto cs = reinterpret_cast<CREATESTRUCT*>(lp);
		auto window = static_cast<Platform::WindowContext*>(cs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	}
	// Forward the message to our primary window procedure.
	return WndProc(hwnd, msg, wp, lp);
}

#ifdef EDITORUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
#if EDITORUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
#endif

	auto* const window = reinterpret_cast<Platform::WindowContext*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_DPICHANGED:
		if (window)
			window->dpiScale = static_cast<float>(GetDpiForWindow(static_cast<HWND>(window->handle))) / USER_DEFAULT_SCREEN_DPI;
		break;
	case WM_SETFOCUS:
		if (window) { window->displayState.isFocused = true; }
		break;
	case WM_KILLFOCUS:
		if (window) { window->displayState.isFocused = false; }
		Input::ResetInputOnFocusLoss();
		break;
	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONDBLCLK:
		{
			i32 button = 0;
			if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) button = 0;
			if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) button = 1;
			if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) button = 2;
			if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
				                                                               ? Mouse::Button::Button4
				                                                               : Mouse::Button::Button5;

			if (button >= 0 && button < Mouse::Button::ButtonCount)
				Input::ProcessEventButton(input.mouseButtons[button], true);
			return 0;
		}
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
		{
			int button = -1;

			switch (msg)
			{
			case WM_LBUTTONUP:  button = Mouse::Button::Left;   break;
			case WM_RBUTTONUP:  button = Mouse::Button::Right;  break;
			case WM_MBUTTONUP:  button = Mouse::Button::Middle; break;
			case WM_XBUTTONUP:
				{
					WORD xButton = GET_XBUTTON_WPARAM(wParam);
					if (xButton == XBUTTON1)      button = Mouse::Button::Button4;
					else if (xButton == XBUTTON2) button = Mouse::Button::Button5;
					break;
				}
			}

			if (button >= 0 && button < Mouse::Button::ButtonCount)
				Input::ProcessEventButton(input.mouseButtons[button], false);

			return 0;
		}
	case WM_MOUSEWHEEL:
		input.scrollDelta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
		break;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		{
			const auto key = MapKeys(wParam, lParam);
			if (key != Keyboard::Key::ButtonCount)
				Input::ProcessEventButton(input.keyboard[key], true);

			if (wParam == VK_ESCAPE)
			{
				DestroyWindow(hwnd);
			}
			else if (wParam == VK_F11)
			{
				if (window->displayMode == Platform::DisplayMode::Windowed)
					Platform::SetDisplayMode(*window, Platform::DisplayMode::BorderlessFullscreen);
				else
					Platform::SetDisplayMode(*window, Platform::DisplayMode::Windowed);
			}
			return 0;
		}
	case WM_SYSKEYUP:
	case WM_KEYUP:
		{
			const auto key = MapKeys(wParam, lParam);
			if (key != Keyboard::Key::ButtonCount)
				Input::ProcessEventButton(input.keyboard[key], false);
			return 0;
		}
	case WM_MOUSEMOVE:
	case WM_NCMOUSEMOVE:
		{
			if (!input.useRawInput)
			{
				const f32 currentX = static_cast<f32>(GET_X_LPARAM(lParam));
				const f32 currentY = static_cast<f32>(GET_Y_LPARAM(lParam));

				// Calculate relative movement (delta)
				input.xrel = currentX - input.lastX;
				input.yrel = currentY - input.lastY;

				// Update the last cursor positions
				input.lastX = currentX;
				input.lastY = currentY;

				// Update the current cursor positions
				input.cursorX = currentX;
				input.cursorY = currentY;
			}
			break;
		}
	case WM_SIZE:
		{
			if (window == nullptr) break;

			if (wParam == SIZE_MINIMIZED)
			{
				window->displayState.isMinimized = true;
				window->displayState.isResized = false;
			}
			else
			{
				window->displayState.isMinimized = false;
				window->displayState.isResized = true;
			}

			window->windowWidth = LOWORD(lParam);
			window->windowHeight = HIWORD(lParam);
			return 0;
		}

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

bool Platform::ShowMessageBox(std::string_view message, std::string_view title, MessageBoxType type)
{
	UINT flags = 0;

	switch (type)
	{
	case MessageBoxType::Info:    flags |= MB_ICONINFORMATION | MB_OK;	     break;
	case MessageBoxType::Warning: flags |= MB_ICONWARNING     | MB_OKCANCEL; break;
	case MessageBoxType::Error:   flags |= MB_ICONERROR       | MB_OKCANCEL; break;
	case MessageBoxType::YesNo:   flags |= MB_ICONQUESTION    | MB_YESNO;    break;
	default:					  flags = MB_OK;							 break;
	}

	const std::wstring wideMsg   = ConvertToWideString(message);
	const std::wstring wideTitle = ConvertToWideString(title);

	switch (MessageBox(nullptr, wideMsg.c_str(), wideTitle.c_str(), flags))
	{
	case IDCANCEL:
		{
#ifndef NDEBUG
			int flag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
			flag ^= _CRTDBG_LEAK_CHECK_DF;
			_CrtSetDbgFlag(flag);
#endif
			_exit(EXIT_FAILURE);
			break;
		}
	case IDYES: return true;
	case IDNO: return false;
	case IDOK: return true;
	default: return false;
	}
}

void Platform::StartFrame(WindowContext& window)
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	const f64 currentTicks = static_cast<f64>(currentTime.QuadPart);

	if (window.lastFrameTime == 0.0) {
		window.lastFrameTime = currentTicks;
	}

	const f64 deltaTicks = currentTicks - window.lastFrameTime;
	window.deltaTime     = deltaTicks / static_cast<f64>(window.perfCountFrequency);
	window.elapsedTime  += window.deltaTime;
	window.fps           = (window.deltaTime > 0.0) ? static_cast<f32>(1.0 / window.deltaTime) : 0.0f;

	window.lastFrameTime = currentTicks;
}

void Platform::ShowWindow(const WindowContext& window)
{
	::ShowWindow(static_cast<HWND>(window.handle), SW_NORMAL);
	::SetForegroundWindow(static_cast<HWND>(window.handle));
}

Platform::BatteryState Platform::GetBatteryState()
{
	BatteryState state;

	SYSTEM_POWER_STATUS status;
	if(GetSystemPowerStatus(&status) != 0)
	{
		state.batteryPercentage = (status.BatteryLifePercent != 255) ? status.BatteryLifePercent : -1;
		state.ACConnected        = (status.ACLineStatus == 1);
		// BatteryFlag: bit 8 indicates charging; bit 128 indicates no battery.
		state.Charging           = (status.BatteryFlag & 8) != 0;
		state.HasBattery         = (status.BatteryFlag & 128) == 0;
	}
	else
	{
		// In case of failure, mark battery level unknown.
		state.batteryPercentage = -1;
		state.ACConnected        = false;
		state.Charging           = false;
		state.HasBattery         = false;
	}
	return state;
}

bool Platform::ProcessMessages(WindowContext* /*unused*/)
{
	MSG msg = {};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT) return false;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}

// Memory
void* Platform::Allocate(size_t size)
{
	return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void* Platform::AllocateFromArena(void* arena, size_t size)
{
	auto* myArena = static_cast<ArenaAllocator*>(arena);
	return myArena->Alloc(size);
}

void Platform::Free(void* ptr)
{
	if (ptr != nullptr)
	{
		VirtualFree(ptr, 0, MEM_RELEASE);
	}
}

// Utility Functions
std::wstring Platform::ConvertToWideString(const std::string_view& str)
{
	std::wstring wideStr(MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0), 0);
	MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wideStr.data(), static_cast<int>(wideStr.size()));
	return wideStr;
}

std::string Platform::ConvertToString(const std::wstring_view& wstr)
{
	std::string multiByteStr(WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, FALSE), 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), multiByteStr.data(), static_cast<int>(multiByteStr.size()), nullptr, FALSE);
	return multiByteStr;
}
