//
// Created by Orgest on 6/8/2025.
//
#define WIN32_LEAN_AND_MEAN
#ifdef ENGINE_PLATFORM_WIN32
#include <Windows.h>
// Xbox stuff

#include "RendererTypes.h"
#include "Input/InputSys.h"
#include "Input/InputSysGameInput.h"

#include <fmt/core.h>

#ifdef EDITORUI
#include <imgui_impl_win32.h>
#endif

#include <dwmapi.h>

#include "Platform.h"
#include "../Tools/Arena.h"
#include "Tools/Logger.h"
#include "tracy/Tracy.hpp"

// Ensure we extract signed coordinate data (multi-monitor support) (Dont want to import the whole of <windowsx.h>)
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

// Force GPU To be on Dedicated for laptops!
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

__declspec(dllexport) void NoHotPatch()
{
} // Disable Nahimic code injection.
}

// Forward Declare up here for init
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK GlobalWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

Keyboard::Key MapKeys(WPARAM wParam, LPARAM lParam)
{
    const u8 scancode = (lParam >> 16) & 0xFF;
    const bool isExtended = (lParam >> 24) & 1;

    // Handle extended and special keys
    switch (wParam)
    {
    case VK_ESCAPE: return Keyboard::Key::Escape;
    case VK_TAB: return Keyboard::Key::Tab;
    case VK_RETURN: return Keyboard::Key::Enter;
    case VK_BACK: return Keyboard::Key::Backspace;
    case VK_SPACE: return Keyboard::Key::Space;

    case VK_INSERT: return Keyboard::Key::Insert;
    case VK_DELETE: return Keyboard::Key::Delete;
    case VK_HOME: return Keyboard::Key::Home;
    case VK_END: return Keyboard::Key::End;
    case VK_LEFT: return Keyboard::Key::Left;
    case VK_RIGHT: return Keyboard::Key::Right;
    case VK_UP: return Keyboard::Key::Up;
    case VK_DOWN: return Keyboard::Key::Down;


    case VK_SHIFT:
        return (scancode == 0x36) ? Keyboard::Key::Shift : Keyboard::Key::Shift;
    case VK_CONTROL:
        return isExtended ? Keyboard::Key::Ctrl : Keyboard::Key::Ctrl;
    case VK_MENU:
        return isExtended ? Keyboard::Key::Alt : Keyboard::Key::Alt;

    case VK_F1: return Keyboard::Key::F1;
    case VK_F2: return Keyboard::Key::F2;
    case VK_F3: return Keyboard::Key::F3;
    case VK_F4: return Keyboard::Key::F4;
    case VK_F5: return Keyboard::Key::F5;
    case VK_F6: return Keyboard::Key::F6;
    case VK_F7: return Keyboard::Key::F7;
    case VK_F8: return Keyboard::Key::F8;
    case VK_F9: return Keyboard::Key::F9;
    case VK_F10: return Keyboard::Key::F10;
    case VK_F11: return Keyboard::Key::F11;
    case VK_F12: return Keyboard::Key::F12;

    case VK_CAPITAL: return Keyboard::Key::CapsLock;
    case VK_NUMLOCK: return Keyboard::Key::NumLock;
    case VK_SCROLL: return Keyboard::Key::ScrollLock;
    case VK_PRINT: return Keyboard::Key::PrintScreen;
    case VK_PAUSE: return Keyboard::Key::Pause;
    case VK_APPS: return Keyboard::Key::Menu;

    case VK_OEM_1: return Keyboard::Key::SemiColon; // ; :
    case VK_OEM_2: return Keyboard::Key::Question_BackSlash; // / ?
    case VK_OEM_3: return Keyboard::Key::Tilde; // ` ~
    case VK_OEM_4: return Keyboard::Key::SquareBracketsOpen; // [
    case VK_OEM_5: return Keyboard::Key::Backslash; // \ |
    case VK_OEM_6: return Keyboard::Key::SquareBracketsClose; // ]
    case VK_OEM_7: return Keyboard::Key::Quotes; // ' "
    case VK_OEM_COMMA: return Keyboard::Key::Comma_LeftArrow; // ,
    case VK_OEM_PERIOD: return Keyboard::Key::Period_RightArrow; // .
    case VK_OEM_PLUS: return Keyboard::Key::Plus_Equal; // =
    case VK_OEM_MINUS: return Keyboard::Key::Minus_Underscore; // -

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

bool IsSystemDarkModeEnabled()
{
    HKEY hKey;
    LPCWSTR subkey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD value = 1;
        DWORD size = sizeof(value);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&value, &size) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return value == 0;
        }
        RegCloseKey(hKey);
    }
    return false;
}

void ApplyModernTheme(HWND hwnd)
{
    BOOL isDark = IsSystemDarkModeEnabled();

    // Dark Mode (Win10 1809+ and Win11)
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &isDark, sizeof(isDark));

    // Rounded Corners (Win11 only)
    DWORD cornerPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));

    // Custom Title Bar Color (Win11 only)
    if (isDark)
    {
        COLORREF captionColor = RGB(20, 20, 20);
        DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
    }

    // Force redraw for frame change
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void Platform::Init(WindowContext* window, i32 width, i32 height, DisplayMode mode)
{
    ZoneScopedN("Init Win32 Platform");
    ImGui_ImplWin32_EnableDpiAwareness(); // why here.....trust me it works
    window->windowWidth = width;
    window->windowHeight = height;
    window->monitorWidth = GetSystemMetrics(SM_CXSCREEN);
    window->monitorHeight = GetSystemMetrics(SM_CYSCREEN);
    window->platformName = "Win32";
    window->positionX = CW_USEDEFAULT;
    window->positionY = CW_USEDEFAULT;
    // TODO(Orgest): this is fine for now, but will have to test for multiple screens/devices
    width == 0 ? window->windowWidth = window->monitorWidth : window->windowWidth = width;
    height == 0 ? window->windowHeight = window->monitorHeight : window->windowHeight = height;

    // Check for min Windows version Win 10 22H2
    // if (IsWindowsVersionOrGreater(10, 0, 22621))
    // {
    // 	ShowMessageBox("Windows 10 22H2 is the min spec", "Error", MessageBoxType::Error);
    // }


    // Init performance counter
    LARGE_INTEGER perfFrequency, perfCounter;
    ::QueryPerformanceFrequency(&perfFrequency);
    ::QueryPerformanceCounter(&perfCounter);

    window->perfCountFrequency = perfFrequency.QuadPart;
    window->lastFrameTime = static_cast<f64>(perfCounter.QuadPart);
    // Store OS version
    // InitOSVersion(window);

    // Title (will be changed in the future)!
    const std::string title = fmt::format("{} - {} - {} - {} - {} - {}", ENGINE_NAME, ENGINE_BUILD,
                                          window->platformName,
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
        .lpszClassName = widePlatformName.c_str(),
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


    ApplyModernTheme(static_cast<HWND>(window->handle));

    HMONITOR hMonitor = MonitorFromWindow(window->GetHandle<HWND>(), MONITOR_DEFAULTTONEAREST);
    MONITORINFOEX monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    if (GetMonitorInfo(hMonitor, &monitorInfo))
    {
        window->monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        window->monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
    }
    else
    {
        window->monitorWidth = GetSystemMetrics(SM_CXSCREEN);
        window->monitorHeight = GetSystemMetrics(SM_CYSCREEN);
    }

    // Dpi!!
    window->dpiScale = static_cast<f32>(GetDpiForWindow(window->GetHandle<HWND>())) / USER_DEFAULT_SCREEN_DPI;

    // GameInput!!
    gameInput.Init();
}

bool Platform::GetWindowSize(const WindowHandle& window, u32& width, u32& height)
{
    RECT rect;
    GetClientRect(static_cast<HWND>(window), &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    return true;
}

void Platform::SetDisplayMode(WindowContext& window, DisplayMode mode)
{
    static RECT previousWindowRect{};
    static DWORD previousWindowStyle = 0;
    static DWORD previousWindowExStyle = 0;

    auto* hwnd = window.GetHandle<HWND>();

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
        window.displayMode = DisplayMode::Windowed;
    }
    else if (mode == DisplayMode::BorderlessFullscreen)
    {
        if (previousWindowRect.right == 0 && previousWindowRect.bottom == 0)
        {
            GetWindowRect(hwnd, &previousWindowRect);
            previousWindowStyle = GetWindowLong(hwnd, GWL_STYLE);
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
    case WM_SETTINGCHANGE:
        // Check if the system theme color set changed
        if (lParam && wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0)
        {
            ApplyModernTheme(hwnd);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (window)
        {
            window->dpiScale = static_cast<f32>(GetDpiForWindow(hwnd)) / USER_DEFAULT_SCREEN_DPI;

            const auto* suggestedRect = reinterpret_cast<RECT*>(lParam);

            SetWindowPos(hwnd,
                         nullptr,
                         suggestedRect->left,
                         suggestedRect->top,
                         suggestedRect->right - suggestedRect->left,
                         suggestedRect->bottom - suggestedRect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    case WM_SETFOCUS:
        if (window)
        {
            window->displayState.isFocused = true;

            // Reset lastFrameTime to prevent huge deltaTime spike when regaining focus
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            window->lastFrameTime = static_cast<f64>(currentTime.QuadPart);
        }
        break;

    case WM_KILLFOCUS:
        if (window)
        {
            window->displayState.isFocused = false;
            Input::ResetInputOnFocusLoss();
        }
        break;
    case WM_ENTERSIZEMOVE:
        if (window)
        {
            window->displayState.isResizing = true;
        }
        break;

    case WM_EXITSIZEMOVE:
        if (window)
        {
            window->displayState.isResizing = false;
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
        {
            // i32 button = 0;
            // if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) button = 0;
            // if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) button = 1;
            // if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) button = 2;
            // if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
            // 	                                                               ? Mouse::Button::Button4
            // 	                                                               : Mouse::Button::Button5;
            //
            // if (button >= 0 && button < Mouse::Button::ButtonCount)
            // 	Input::ProcessEventButton(input.mouseButtons[button], true);
            return 0;
        }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
        {
            // int button = -1;
            //
            // switch (msg)
            // {
            // case WM_LBUTTONUP:  button = Mouse::Button::Left;   break;
            // case WM_RBUTTONUP:  button = Mouse::Button::Right;  break;
            // case WM_MBUTTONUP:  button = Mouse::Button::Middle; break;
            // case WM_XBUTTONUP:
            // 	{
            // 		WORD xButton = GET_XBUTTON_WPARAM(wParam);
            // 		if (xButton == XBUTTON1)      button = Mouse::Button::Button4;
            // 		else if (xButton == XBUTTON2) button = Mouse::Button::Button5;
            // 		break;
            // 	}
            // }
            //
            // if (button >= 0 && button < Mouse::Button::ButtonCount)
            // 	Input::ProcessEventButton(input.mouseButtons[button], false);

            return 0;
        }
    case WM_MOUSEWHEEL:
        // input.scrollDelta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        {
            // const auto key = MapKeys(wParam, lParam);
            // if (key != Keyboard::Key::ButtonCount)
            // 	Input::ProcessEventButton(input.keyboard[key], true);
            //
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
            // const auto key = MapKeys(wParam, lParam);
            // if (key != Keyboard::Key::ButtonCount)
            // 	Input::ProcessEventButton(input.keyboard[key], false);
            return 0;
        }
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
        {
            // if (!input.useRawInput)
            // {
            // 	const f32 currentX = static_cast<f32>(GET_X_LPARAM(lParam));
            // 	const f32 currentY = static_cast<f32>(GET_Y_LPARAM(lParam));
            //
            // 	// Calculate relative movement (delta)
            // 	input.xrel = currentX - input.lastX;
            // 	input.yrel = currentY - input.lastY;
            //
            // 	// Update the last cursor positions
            // 	input.lastX = currentX;
            // 	input.lastY = currentY;
            //
            // 	// Update the current cursor positions
            // 	input.cursorX = currentX;
            // 	input.cursorY = currentY;
            // }
            break;
        }
    case WM_SIZE:
        {
            if (window == nullptr) break;
            const u32 w = LOWORD(lParam);
            const u32 h = HIWORD(lParam);

            if (wParam == SIZE_MINIMIZED || w == 0 || h == 0)
            {
                window->displayState.isMinimized = true;
                window->displayState.isResized = false;
            }
            else
            {
                window->displayState.isMinimized = false;
                // Only flag resize if actual dimensions changed
                if (window->windowWidth != w || window->windowHeight != h)
                {
                    window->displayState.isResized = true;
                    window->windowWidth = w;
                    window->windowHeight = h;
                }
            }
            return 0;
        }
    case WM_GETMINMAXINFO:
        {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 320;
            info->ptMinTrackSize.y = 240;
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
    case MessageBoxType::Info: flags |= MB_ICONINFORMATION | MB_OK;
        break;
    case MessageBoxType::Warning: flags |= MB_ICONWARNING | MB_OKCANCEL;
        break;
    case MessageBoxType::Error: flags |= MB_ICONERROR | MB_OKCANCEL;
        break;
    case MessageBoxType::YesNo: flags |= MB_ICONQUESTION | MB_YESNO;
        break;
    default: flags = MB_OK;
        break;
    }

    const std::wstring wideMsg = ConvertToWideString(message);
    const std::wstring wideTitle = ConvertToWideString(title);

    // Standard Win32 MessageBox call
    const i32 result = MessageBox(nullptr, wideMsg.c_str(), wideTitle.c_str(), flags);

    if (type == MessageBoxType::Error && result == IDCANCEL)
    {
#ifndef NDEBUG
        // Cleanup debug flags before exiting
        int flag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        flag ^= _CRTDBG_LEAK_CHECK_DF;
        _CrtSetDbgFlag(flag);
#endif
        _exit(EXIT_FAILURE);
    }

    return (result == IDYES || result == IDOK);
}

void Platform::StartFrame(WindowContext& window)
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    f64 currentTicks = static_cast<f64>(currentTime.QuadPart);

    if (!window.displayState.isFocused)
    {
        const f64 elapsedMs = ((currentTicks - window.lastFrameTime) / static_cast<f64>(window.perfCountFrequency)) *
            1000.0;

        if (elapsedMs < static_cast<f64>(BACKGROUND_FRAME_TIME))
        {
            const f64 sleepMs = static_cast<f64>(BACKGROUND_FRAME_TIME) - elapsedMs;
            Sleep(static_cast<DWORD>(sleepMs));

            QueryPerformanceCounter(&currentTime);
            currentTicks = static_cast<f64>(currentTime.QuadPart);
        }
    }

    const f64 deltaTicks = currentTicks - window.lastFrameTime;
    window.deltaTime = deltaTicks / static_cast<f64>(window.perfCountFrequency);

    window.elapsedTime += window.deltaTime;

    window.frameTime = static_cast<f32>(window.deltaTime * 1000.0);

    // Store current frame time in the circular buffer
    window.frameTimeBuffer[window.frameBufferIndex] = window.frameTime;
    window.frameBufferIndex = (window.frameBufferIndex + 1) % 60;

    // Calculate Average
    f32 totalTime = 0.0f;
    for (i32 i = 0; i < 60; i++)
    {
        totalTime += window.frameTimeBuffer[i];
    }
    const f32 averageFrameTime = totalTime / 60.0f;
    window.fps = (averageFrameTime > 0.0f) ? (1000.0f / averageFrameTime) : 0.0f;

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
    if (GetSystemPowerStatus(&status) != 0)
    {
        state.batteryPercentage = (status.BatteryLifePercent != 255) ? status.BatteryLifePercent : -1;
        state.ACConnected = (status.ACLineStatus == 1);
        // BatteryFlag: bit 8 indicates charging; bit 128 indicates no battery.
        state.Charging = (status.BatteryFlag & 8) != 0;
        state.HasBattery = (status.BatteryFlag & 128) == 0;
    }
    else
    {
        // In case of failure, mark battery level unknown.
        state.batteryPercentage = -1;
        state.ACConnected = false;
        state.Charging = false;
        state.HasBattery = false;
    }
    return state;
}

void Platform::CenterMouse(const WindowContext* window)
{
    HWND hwnd = static_cast<HWND>(window->handle);

    RECT rc{};
    GetClientRect(hwnd, &rc);

    POINT pt{};
    pt.x = (rc.left + rc.right) / 2;
    pt.y = (rc.top + rc.bottom) / 2;

    // Convert client center -> screen coords, then warp
    ::ClientToScreen(hwnd, &pt);
    ::SetCursorPos(pt.x, pt.y);
}

bool Platform::GetCursorClientPos(const WindowContext* window, i32& outX, i32& outY)
{
    HWND hwnd = static_cast<HWND>(window->handle);

    POINT pt{};
    if (!GetCursorPos(&pt))
        return false;

    if (!ScreenToClient(hwnd, &pt))
        return false;

    outX = pt.x;
    outY = pt.y;
    return true;
}

void Platform::SetCursorClientPos(const WindowContext* window, i32 x, i32 y)
{
    HWND hwnd = static_cast<HWND>(window->handle);

    POINT pt{};
    pt.x = x;
    pt.y = y;

    // Convert client coords to screen coords, then warp
    ClientToScreen(hwnd, &pt);
    SetCursorPos(pt.x, pt.y);
}

bool Platform::WrapCursorToOppositeEdge(const WindowContext* window, i32 margin)
{
    i32 cursorX, cursorY;
    if (!GetCursorClientPos(window, cursorX, cursorY))
        return false;

    const i32 width = window->windowWidth;
    const i32 height = window->windowHeight;

    i32 newX = cursorX;
    i32 newY = cursorY;
    bool needsWrap = false;

    // Wrap horizontally to opposite edge
    if (cursorX < margin)
    {
        newX = width - margin - 1;
        needsWrap = true;
    }
    else if (cursorX > width - margin)
    {
        newX = margin + 1;
        needsWrap = true;
    }

    // Wrap vertically to opposite edge
    if (cursorY < margin)
    {
        newY = height - margin - 1;
        needsWrap = true;
    }
    else if (cursorY > height - margin)
    {
        newY = margin + 1;
        needsWrap = true;
    }

    if (needsWrap)
    {
        SetCursorClientPos(window, newX, newY);
        return true;
    }

    return false;
}

void Platform::SetCursorVisible(bool show)
{
    CURSORINFO ci = {sizeof(CURSORINFO)};
    if (GetCursorInfo(&ci))
    {
        bool isCurrentlyVisible = (ci.flags & CURSOR_SHOWING) != 0;
        if (show != isCurrentlyVisible)
        {
            // ShowCursor returns the new visibility level.
            // We loop to ensure we override any previous nested calls.
            if (show) while (ShowCursor(TRUE) < 0)
            {
            }
            else while (ShowCursor(FALSE) >= 0);
        }
    }
}

bool GetClientRectOnScreen(HWND hwnd, RECT& rect)
{
    // Get the client area dimensions in client coordinates.
    if (!GetClientRect(hwnd, &rect))
    {
        return false;
    }

    // Convert the client coordinates to screen coordinates.
    POINT upperLeft = {rect.left, rect.top};
    POINT lowerRight = {rect.right, rect.bottom};

    if (!MapWindowPoints(hwnd, nullptr, &upperLeft, 1))
    {
        return false;
    }
    if (!MapWindowPoints(hwnd, nullptr, &lowerRight, 1))
    {
        return false;
    }

    // Update the RECT with the new screen coordinates.
    rect.left = upperLeft.x;
    rect.top = upperLeft.y;
    rect.right = lowerRight.x;
    rect.bottom = lowerRight.y;

    return true;
}

void Platform::SetCursorLocked(const WindowContext* wc, bool locked)
{
    const auto hwnd = static_cast<HWND>(wc->handle);
    if (locked)
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
        ClipCursor(&rect);
        SetCapture(hwnd);
    }
    else
    {
        // Release everything.
        ClipCursor(nullptr);
        ReleaseCapture();
    }
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
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wideStr.data(),
                        static_cast<int>(wideStr.size()));
    return wideStr;
}

std::string Platform::ConvertToString(const std::wstring_view& wstr)
{
    std::string multiByteStr(WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0,
                                                 nullptr, FALSE), 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), multiByteStr.data(),
                        static_cast<int>(multiByteStr.size()), nullptr, FALSE);
    return multiByteStr;
}
#endif
