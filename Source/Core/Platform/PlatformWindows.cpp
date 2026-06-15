//
// Created by Orgest on 6/8/2025.
//
#ifdef ENGINE_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "RendererTypes.h"
#include "Input/InputSys.h"
#include "Input/InputSysGameInput.h"

#ifdef EDITORUI
#include <imgui_impl_win32.h>
#endif


#include <dwmapi.h>
#include <ranges>

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
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK GlobalWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static bool IsSystemDarkModeEnabled()
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

static void ApplyTheme(HWND hwnd)
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

    RefreshMonitorData(window); // Populate!!
    const auto& primary = window->displaySettings.monitors[window->activeMonitorIndex];

    if (width == 0 || height == 0)
    {
        if (mode == DisplayMode::BorderlessFullscreen)
        {
            window->windowWidth = primary.desktopMode.width;
            window->windowHeight = primary.desktopMode.height;
        }
        else
        {
            // Screen size
            window->windowWidth = static_cast<u32>(GetSystemMetrics(SM_CXSCREEN));
            window->windowHeight = static_cast<u32>(GetSystemMetrics(SM_CYSCREEN));
        }
    }
    else
    {
        window->windowWidth = width;
        window->windowHeight = height;
    }

    window->platformName = "Win32";

    // Init performance counter
    LARGE_INTEGER perfFrequency, perfCounter;
    ::QueryPerformanceFrequency(&perfFrequency);
    ::QueryPerformanceCounter(&perfCounter);

    window->perfCountFrequency = perfFrequency.QuadPart;
    window->lastFrameTime = static_cast<f64>(perfCounter.QuadPart);

    // Title (will be changed in the future)!
    const std::string title = fmt::format("{} - {} - {} - {} - {} - {}", ENGINE_NAME, ENGINE_BUILD,
                                          window->platformName,
                                          ENGINE_VERSION, __DATE__, ENGINE_COMMIT_HASH);
    const std::wstring widePlatformName = ConvertToWideString(title);

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
        0,
        0,
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


    ApplyTheme(static_cast<HWND>(window->handle));

    SetWindowResolution(*window, window->windowWidth, window->windowHeight);

    if (mode == DisplayMode::BorderlessFullscreen)
    {
        SetDisplayMode(*window, DisplayMode::BorderlessFullscreen);
    }

    // Dpi!!
    window->dpiScale = static_cast<f32>(GetDpiForWindow(window->GetHandle<HWND>())) / USER_DEFAULT_SCREEN_DPI;

    // GameInput!!
    gameInput.Init();
}

void Platform::SetTitleBarText(const WindowContext& window, const std::string_view text)
{
    static std::string lastText;
    if (text == lastText) return;
    auto* hwnd = static_cast<HWND>(window.handle);
    lastText = text;

    const std::wstring wideTitleName = ConvertToWideString(text);

    ::SetWindowText(hwnd, wideTitleName.c_str());
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
    if (mode == DisplayMode::Fullscreen) mode = DisplayMode::BorderlessFullscreen;

    auto* hwnd = static_cast<HWND>(window.handle);
    if (!hwnd) return;

    // The Win32 Master Struct for tracking window restoration states natively
    static WINDOWPLACEMENT savedPlacement = {sizeof(WINDOWPLACEMENT)};

    if (mode == DisplayMode::Windowed)
    {
        if (!window.displayState.isBorderless) return;

        // Restore original borders and placement (natively handles SW_MAXIMIZE if it was maximized before!)
        ::SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        ::SetWindowPlacement(hwnd, &savedPlacement);
        ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        window.displayState.isBorderless = false;
        window.displayMode = DisplayMode::Windowed;
    }
    else if (mode == DisplayMode::BorderlessFullscreen)
    {
        if (window.displayState.isBorderless) return;

        // Save current placement (catches if it's currently maximized or floating)
        ::GetWindowPlacement(hwnd, &savedPlacement);

        ::SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);

        HMONITOR hMonitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {sizeof(MONITORINFO)};
        if (::GetMonitorInfo(hMonitor, &monitorInfo))
        {
            ::SetWindowPos(hwnd, HWND_TOP,
                           monitorInfo.rcMonitor.left,
                           monitorInfo.rcMonitor.top,
                           monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                           monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                           SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }

        window.displayState.isBorderless = true;
        window.displayMode = DisplayMode::BorderlessFullscreen;
    }

    RECT clientRect;
    if (::GetClientRect(hwnd, &clientRect))
    {
        window.windowWidth = clientRect.right - clientRect.left;
        window.windowHeight = clientRect.bottom - clientRect.top;
    }
}

static BOOL CALLBACK MonitorEnumCallback(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    auto* ctx = reinterpret_cast<Platform::WindowContext*>(dwData);

    MONITORINFOEXW info = {};
    info.cbSize = sizeof(MONITORINFOEXW);
    if (!GetMonitorInfoW(hMonitor, &info)) return TRUE; // Keep enumerating even if one fails

    Platform::MonitorInfo m = {};
    m.name = Platform::ConvertToString(info.szDevice);
    m.nativeHandle = hMonitor;
    m.isPrimary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

    // Get the current desktop mode
    DEVMODEW dm = {.dmSize = sizeof(dm)};
    if (EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &dm))
    {
        m.desktopMode = {(u32)dm.dmPelsWidth, (u32)dm.dmPelsHeight, (u32)dm.dmDisplayFrequency};
    }

    // Enumerate ALL supported modes using your C++23/26 style
    for (u32 i = 0; EnumDisplaySettingsW(info.szDevice, i, &dm); ++i)
    {
        Platform::VideoMode mode = {(u32)dm.dmPelsWidth, (u32)dm.dmPelsHeight, (u32)dm.dmDisplayFrequency};
        m.supportedModes[Platform::GetAspectRatio(mode.width, mode.height)].push_back(mode);
    }

    for (auto& modes : m.supportedModes | std::views::values)
    {
        std::ranges::sort(modes);
        auto [first, last] = std::ranges::unique(modes);
        modes.resize(static_cast<vecSizeType>(first - modes.begin()));
    }
    ctx->displaySettings.monitors.push_back(std::move(m));
    return TRUE;
}

void Platform::RefreshMonitorData(WindowContext* window)
{
    // Clear old data before re-polling hardware
    window->displaySettings.monitors.clear();

    // Pass the function pointer and the window context as the user data
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumCallback, reinterpret_cast<LPARAM>(window));

    // Fallback: If for some reason we have no monitors, fake one using system metrics
    if (window->displaySettings.monitors.empty())
    {
        MonitorInfo fallback = {};
        fallback.name = "Fallback Display";
        fallback.nativeHandle = nullptr;
        fallback.isPrimary = true;
        fallback.desktopMode = {
            static_cast<u32>(GetSystemMetrics(SM_CXSCREEN)),
            static_cast<u32>(GetSystemMetrics(SM_CYSCREEN)),
            60
        };
        window->displaySettings.monitors.push_back(std::move(fallback));
    }

    // Set the active index to the primary monitor using modern ranges
    auto it = std::ranges::find_if(window->displaySettings.monitors,
                                   [](const MonitorInfo& m) { return m.isPrimary; });

    if (it != window->displaySettings.monitors.end())
    {
        window->activeMonitorIndex = static_cast<i32>(std::distance(window->displaySettings.monitors.begin(), it));
    }
    else
    {
        window->activeMonitorIndex = 0; // Safe default if Windows reported no 'primary' flag
    }
}

void Platform::SetWindowResolution(WindowContext& window, u32 width, u32 height)
{
    const auto hwnd = static_cast<HWND>(window.handle);

    RECT wr = {0, 0, (LONG)width, (LONG)height};
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    AdjustWindowRect(&wr, style, FALSE);

    i32 finalWidth = wr.right - wr.left;
    i32 finalHeight = wr.bottom - wr.top;

    // Grab the target monitor to calculate the exact center
    const auto& targetMonitor = window.displaySettings.monitors[window.activeMonitorIndex];
    MONITORINFO mi = {sizeof(MONITORINFO)};

    if (GetMonitorInfo((HMONITOR)targetMonitor.nativeHandle, &mi))
    {
        // Borderless ignores the taskbar, Windowed respects it.
        const RECT& bounds = (window.displayMode == DisplayMode::BorderlessFullscreen) ? mi.rcMonitor : mi.rcWork;

        const i32 boundWidth = bounds.right - bounds.left;
        const i32 boundHeight = bounds.bottom - bounds.top;

        // Clamp the total window size to never exceed the bounds
        finalWidth = std::min(finalWidth, boundWidth);
        finalHeight = std::min(finalHeight, boundHeight);

        // Calculate absolute center within the active bounds
        const i32 x = bounds.left + (boundWidth - finalWidth) / 2;
        const i32 y = bounds.top + (boundHeight - finalHeight) / 2;

        SetWindowPos(hwnd, nullptr, x, y, finalWidth, finalHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    else
    {
        // Fallback if monitor query fails
        SetWindowPos(hwnd, nullptr, 0, 0, finalWidth, finalHeight,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    // Because we might have clamped the size, query the actual resulting client area.
    RECT clientRect;
    if (::GetClientRect(hwnd, &clientRect))
    {
        window.windowWidth = clientRect.right - clientRect.left;
        window.windowHeight = clientRect.bottom - clientRect.top;
    }
    else
    {
        window.windowWidth = width;
        window.windowHeight = height;
    }
}

void Platform::SetMonitorRefreshRate(WindowContext& window, u32 refreshRate)
{
    const auto& targetMonitor = window.displaySettings.monitors[window.activeMonitorIndex];
    const std::wstring deviceName = ConvertToWideString(targetMonitor.name);

    DEVMODEW dm = {.dmSize = sizeof(dm)};
    if (EnumDisplaySettingsW(deviceName.c_str(), ENUM_CURRENT_SETTINGS, &dm))
    {
        // Only trigger an OS display change if the user actually requested a different frequency
        if (dm.dmDisplayFrequency != refreshRate)
        {
            dm.dmDisplayFrequency = refreshRate;
            dm.dmFields = DM_DISPLAYFREQUENCY;

            // Pass 0 instead of CDS_FULLSCREEN for standard Windowed/Borderless behavior
            LONG result = ChangeDisplaySettingsExW(deviceName.c_str(), &dm, nullptr, 0, nullptr);

            if (result != DISP_CHANGE_SUCCESSFUL)
            {
                LOG(Warning, "Failed to change OS display refresh rate. Error Code: {}", result);
            }
        }
    }
}

void Platform::MoveWindowToMonitor(WindowContext& window, i32 monitorIndex)
{
    const auto& target = window.displaySettings.monitors[monitorIndex];
    const auto hwnd = static_cast<HWND>(window.handle);

    // Get the monitor's workspace coordinates
    MONITORINFO mi = {sizeof(MONITORINFO)};
    if (GetMonitorInfo((HMONITOR)target.nativeHandle, &mi))
    {
        // Move window to the top-left of the target monitor
        SetWindowPos(hwnd, nullptr,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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
            ApplyTheme(hwnd);
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
            Platform::SetCursorLocked(window, false);
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
            return 0;
        }
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
        {
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
                window->displayState.isMaximized = (wParam == SIZE_MAXIMIZED);

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
    case WM_SYSCOMMAND:
        {
            // Prevent the Alt key from pausing the application loop to activate the window menu
            if ((wParam & 0xfff0) == SC_KEYMENU)
            {
                return 0;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
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
    const f64 realDeltaTime = deltaTicks / static_cast<f64>(window.perfCountFrequency);

    window.frameTime = static_cast<f32>(realDeltaTime * 1000.0);

    window.frameTimeSum -= window.frameTimeBuffer[window.frameBufferIndex]; // Remove oldest
    window.frameTimeBuffer[window.frameBufferIndex] = window.frameTime;       // Insert newest
    window.frameTimeSum += window.frameTime; // Add newest

    window.frameBufferIndex = (window.frameBufferIndex + 1) % 60;
    window.totalFrames++;
    const f32 divisor = (window.totalFrames < 60) ? static_cast<f32>(window.totalFrames) : 60.0f;
    const f32 averageFrameTime = window.frameTimeSum / divisor;

    // Weird delta jumps begone?
    window.deltaTime = std::clamp(realDeltaTime, 0.0, 0.1);

    window.elapsedTime += window.deltaTime;
    window.fps = (averageFrameTime > 0.0f) ? (1000.0f / averageFrameTime) : 0.0f;
    window.lastFrameTime = currentTicks;
}

void Platform::ShowWindow(const WindowContext& window)
{
    auto* hwnd = static_cast<HWND>(window.handle);

    ::ShowWindow(hwnd, SW_NORMAL);
    ::SetForegroundWindow(hwnd);
}

void Platform::MaximizeWindow(WindowContext& window, const bool maximize)
{
    if (window.displayMode != DisplayMode::Windowed) return;

    auto* hwnd = static_cast<HWND>(window.handle);
    if (!hwnd) return;

    ::ShowWindow(hwnd, maximize ? SW_MAXIMIZE : SW_RESTORE);
    window.displayState.isMaximized = maximize;
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

std::string Platform::GetCPUName()
{
    int cpuInfo[4] = {-1};
    char cpuBrandString[0x40] = {};

    // Check if the CPU supports the brand string query
    __cpuid(cpuInfo, 0x80000000);

    if (const unsigned int nExIds = cpuInfo[0]; nExIds >= 0x80000004)
    {
        __cpuid(cpuInfo, 0x80000002);
        memcpy(cpuBrandString, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000003);
        memcpy(cpuBrandString + 16, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000004);
        memcpy(cpuBrandString + 32, cpuInfo, sizeof(cpuInfo));

        return std::string(cpuBrandString);
    }

    return "Unknown CPU";
}

void Platform::CenterMouse(const WindowContext* window)
{
    const auto hwnd = static_cast<HWND>(window->handle);

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
    const auto hwnd = static_cast<HWND>(window->handle);

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
    const auto hwnd = static_cast<HWND>(window->handle);

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

void Platform::SetCursorVisible(const WindowContext* window, bool show)
{
    if (!window || !window->handle) return;

    CURSORINFO ci = {sizeof(CURSORINFO)};
    if (::GetCursorInfo(&ci))
    {
        if (const bool isCurrentlyVisible = (ci.flags & CURSOR_SHOWING) != 0; show != isCurrentlyVisible)
        {
            ::ShowCursor(show ? TRUE : FALSE);
        }
    }
}

void Platform::SetCursorLocked(const WindowContext* window, bool locked)
{
    if (!window || !window->handle) return;
    const auto hwnd = static_cast<HWND>(window->handle);

    // Safeguard: Never lock if we aren't the active foreground window
    if (locked && GetForegroundWindow() != hwnd) return;

    static bool isCurrentlyLocked = false;

    // If it's already locked, we STILL re-apply ClipCursor to prevent Windows from
    // accidentally dropping the bounds (e.g. from popups or notifications),
    // but we skip re-calling SetCapture to avoid messing with ImGui state.
    if (locked && isCurrentlyLocked)
    {
        RECT rect;
        if (::GetClientRect(hwnd, &rect))
        {
            ::MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
            ClipCursor(&rect);
        }
        return;
    }

    if (locked == isCurrentlyLocked) return;

    if (locked)
    {
        RECT rect;
        if (::GetClientRect(hwnd, &rect))
        {
            ::MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
            ClipCursor(&rect);
            SetCapture(hwnd);
            isCurrentlyLocked = true;
        }
    }
    else
    {
        ClipCursor(nullptr);
        ReleaseCapture();
        isCurrentlyLocked = false;
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
std::wstring Platform::ConvertToWideString(std::string_view str)
{
    std::wstring wideStr(MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0), 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wideStr.data(),
                        static_cast<int>(wideStr.size()));
    return wideStr;
}

std::string Platform::ConvertToString(std::wstring_view wstr)
{
    std::string multiByteStr(WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0,
                                                 nullptr, FALSE), 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), multiByteStr.data(),
                        static_cast<int>(multiByteStr.size()), nullptr, FALSE);
    return multiByteStr;
}
#endif
