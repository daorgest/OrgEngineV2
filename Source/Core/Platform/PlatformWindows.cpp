//
// Created by Orgest on 6/8/2025.
//
#define WIN32_LEAN_AND_MEAN
#include <format>
#include <Windows.h>

#include "Platform.h"

// Ensure we extract signed coordinate data (multi-monitor support) (Dont want to import the whole of <windowsx.h>)
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

// Force GPU To be on Dedicated for laptops!
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
	__declspec(dllexport) void NoHotPatch() {} // Disable Nahimic code injection.
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void Platform::Init(WindowContext* window, i32 width, i32 height, DisplayMode mode)
{
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

	// Title!
	const std::string title = std::format("{} - {} - {} - {}", ENGINE_NAME, ENGINE_BUILD, window->platformName, ENGINE_VERSION);
	std::wstring widePlatformName = ConvertToWideString(title);


	// Init window class!
	WNDCLASSEX wc = {
		.cbSize = sizeof(wc),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WndProc,
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
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
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

	std::wstring wideMsg   = ConvertToWideString(message);
	std::wstring wideTitle = ConvertToWideString(title);

	int result = MessageBox(nullptr, wideMsg.c_str(), wideTitle.c_str(), flags);

	switch (result)
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

void Platform::ShowWindow(const WindowContext& window)
{
	::ShowWindow(static_cast<HWND>(window.handle), SW_NORMAL);
	::SetForegroundWindow(static_cast<HWND>(window.handle));
}

Platform::BatteryState Platform::GetBatteryState()
{
	BatteryState state;

	SYSTEM_POWER_STATUS status;
	if(GetSystemPowerStatus(&status))
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

bool Platform::ProcessMessages(WindowContext*)
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

void* Platform::Allocate(size_t size)
{
	return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void Platform::Free(void* ptr)
{
	if (ptr)
	{
		VirtualFree(ptr, 0, MEM_RELEASE);
	}
}

#ifdef EDITOR
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

// Utility Functions
std::wstring Platform::ConvertToWideString(const std::string_view& str)
{
	std::wstring wideStr;
	wideStr.resize(MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0));
	MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wideStr.data(), static_cast<int>(wideStr.size()));
	return wideStr;
}

std::string Platform::ConvertToString(const std::wstring_view& wstr)
{
	std::string multiByteStr;
	multiByteStr.resize(WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, FALSE));
	WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), multiByteStr.data(), static_cast<int>(multiByteStr.size()), nullptr, FALSE);
	return multiByteStr;
}
