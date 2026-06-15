//
// Created by Orgest on 11/21/2025.
//
#include <ranges>

#include "Platform.h"
#include "RendererTypes.h"
#include "fmt/format.h"
#include "Input/InputSystemSDL.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_messagebox.h"
#include "SDL3/SDL_timer.h"
#include "Tools/Arena.h"
#include "Tools/Logger.h"

#define SDL_HINT_MOUSE_RELATIVE_WARP_MOTION "SDL_MOUSE_RELATIVE_WARP_MOTION"

void Platform::Init(WindowContext* window, i32 width, i32 height, DisplayMode mode)
{
    window->platformName = "SDL3";
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD))
    {
        LOG(Error, "[SDL3] SDL_Init failed: {}", SDL_GetError());
        return;
    }

    SDL_SetHint(SDL_HINT_APP_NAME, ENGINE_NAME);

    // 1. Hardware Poll & Monitor Setup
    RefreshMonitorData(window);
    const auto& primary = window->displaySettings.monitors[window->activeMonitorIndex];

    // 2. Resolve Dimensions
    if (width == 0 || height == 0)
    {
        if (mode == DisplayMode::BorderlessFullscreen)
        {
            window->windowWidth = primary.desktopMode.width;
            window->windowHeight = primary.desktopMode.height;
        }
        else
        {
            // Sensible default for windowed development
            window->windowWidth = 1280;
            window->windowHeight = 720;
        }
    }
    else
    {
        window->windowWidth = width;
        window->windowHeight = height;
    }

    window->displayMode = DisplayMode::Windowed; // Force start as Windowed, upgrade later

    const std::string title = fmt::format("{} - {} - {} - {} - {} - {}",
                                          ENGINE_NAME,
                                          ENGINE_BUILD,
                                          window->platformName,
                                          ENGINE_VERSION,
                                          __DATE__,
                                          ENGINE_COMMIT_HASH);

    constexpr SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;

    SDL_Window* sdlWindow = SDL_CreateWindow(title.c_str(), window->windowWidth, window->windowHeight, windowFlags);
    if (!sdlWindow)
    {
        LOG(Error, "[SDL3] SDL_CreateWindow FAILED: {}", SDL_GetError());
        SDL_Quit();
        return;
    }

    window->handle = sdlWindow;

    window->perfCountFrequency = static_cast<i64>(SDL_GetPerformanceFrequency());
    window->lastFrameTime = static_cast<f64>(SDL_GetPerformanceCounter());

    SetWindowResolution(*window, window->windowWidth, window->windowHeight);

    if (mode == DisplayMode::BorderlessFullscreen)
    {
        SetDisplayMode(*window, DisplayMode::BorderlessFullscreen);
    }
}

bool Platform::GetWindowSize(const WindowHandle& handle, u32& width, u32& height)
{
    const auto win = static_cast<SDL_Window*>(handle);
    if (!win) return false;

    SDL_GetWindowSize(win, reinterpret_cast<i32*>(&width), reinterpret_cast<i32*>(&height));
    return true;
}

void Platform::SetDisplayMode(WindowContext& window, DisplayMode mode)
{
    if (mode == DisplayMode::Fullscreen) mode = DisplayMode::BorderlessFullscreen;

    auto* win = static_cast<SDL_Window*>(window.handle);
    if (!win) return;

    if (mode == DisplayMode::Windowed)
    {
        if (!window.displayState.isBorderless) return;

        SDL_SetWindowFullscreen(win, false);

        window.displayState.isBorderless = false;
        window.displayMode = DisplayMode::Windowed;
    }
    else if (mode == DisplayMode::BorderlessFullscreen)
    {
        if (window.displayState.isBorderless) return;

        SDL_SetWindowFullscreen(win, true);

        window.displayState.isBorderless = true;
        window.displayMode = DisplayMode::BorderlessFullscreen;
    }

    // Capture the final resulting client area
    i32 w, h;
    SDL_GetWindowSize(win, &w, &h);
    window.windowWidth = w;
    window.windowHeight = h;
}

void Platform::MaximizeWindow(WindowContext& window, bool maximize)
{
    if (window.displayMode != DisplayMode::Windowed) return;

    auto* win = static_cast<SDL_Window*>(window.handle);
    if (!win) return;

    if (maximize)
        SDL_MaximizeWindow(win);
    else
        SDL_RestoreWindow(win);

    window.displayState.isMaximized = maximize;
}

void Platform::MoveWindowToMonitor(WindowContext& window, i32 monitorIndex)
{
    if (monitorIndex < 0 || monitorIndex >= static_cast<i32>(window.displaySettings.monitors.size())) return;

    auto* win = static_cast<SDL_Window*>(window.handle);
    if (!win) return;

    const auto& target = window.displaySettings.monitors[monitorIndex];
    const SDL_DisplayID displayID = static_cast<SDL_DisplayID>(reinterpret_cast<uintptr_t>(target.nativeHandle));

    SDL_Rect bounds;
    if (SDL_GetDisplayBounds(displayID, &bounds))
    {
        SDL_SetWindowPosition(win, bounds.x, bounds.y);
    }
}

void Platform::SetWindowResolution(WindowContext& window, u32 width, u32 height)
{
    auto* win = static_cast<SDL_Window*>(window.handle);
    if (!win) return;

    const auto& targetMonitor = window.displaySettings.monitors[window.activeMonitorIndex];
    const SDL_DisplayID displayID = static_cast<SDL_DisplayID>(reinterpret_cast<uintptr_t>(targetMonitor.nativeHandle));

    SDL_Rect bounds;
    // Borderless ignores the taskbar, Windowed respects the work area
    if (window.displayMode == DisplayMode::BorderlessFullscreen)
    {
        SDL_GetDisplayBounds(displayID, &bounds);
    }
    else
    {
        SDL_GetDisplayUsableBounds(displayID, &bounds);
    }

    i32 finalWidth = std::min(static_cast<i32>(width), bounds.w);
    i32 finalHeight = std::min(static_cast<i32>(height), bounds.h);

    const i32 x = bounds.x + (bounds.w - finalWidth) / 2;
    const i32 y = bounds.y + (bounds.h - finalHeight) / 2;

    SDL_SetWindowSize(win, finalWidth, finalHeight);

    // Only move it manually if we are windowed (fullscreen enforces its own coordinates)
    if (window.displayMode == DisplayMode::Windowed)
    {
        SDL_SetWindowPosition(win, x, y);
    }

    i32 resultingW, resultingH;
    SDL_GetWindowSize(win, &resultingW, &resultingH);
    window.windowWidth = resultingW;
    window.windowHeight = resultingH;
}

void Platform::SetMonitorRefreshRate(WindowContext& window, u32 width, u32 height, u32 refreshRate)
{
    auto* win = static_cast<SDL_Window*>(window.handle);
    if (!win) return;

    const auto& targetMonitor = window.displaySettings.monitors[window.activeMonitorIndex];
    const SDL_DisplayID displayID = static_cast<SDL_DisplayID>(reinterpret_cast<uintptr_t>(targetMonitor.nativeHandle));

    int modeCount = 0;

    if (SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(displayID, &modeCount))
    {
        const SDL_DisplayMode* bestMode = nullptr;
        // Find the exact matching display mode
        for (int i = 0; i < modeCount; ++i)
        {
            if (modes[i]->w == static_cast<int>(width) &&
                modes[i]->h == static_cast<int>(height) &&
                static_cast<u32>(modes[i]->refresh_rate) == refreshRate)
            {
                bestMode = modes[i];
                break;
            }
        }

        if (bestMode)
        {
            SDL_SetWindowFullscreenMode(win, bestMode);
        }
        SDL_free(modes);
    }
}

void Platform::RefreshMonitorData(WindowContext* window)
{
    window->displaySettings.monitors.clear();

    int displayCount = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&displayCount);
    window->displaySettings.monitors.reserve(displayCount);

    if (!displays || displayCount == 0)
    {
        MonitorInfo fallback = {};
        fallback.name = "Fallback Display";
        fallback.nativeHandle = nullptr;
        fallback.isPrimary = true;
        fallback.desktopMode = {1920, 1080, 60};
        window->displaySettings.monitors.push_back(std::move(fallback));
    }
    else
    {
        const SDL_DisplayID primaryID = SDL_GetPrimaryDisplay();

        for (int i = 0; i < displayCount; ++i)
        {
            const SDL_DisplayID displayID = displays[i];
            MonitorInfo m = {};
            m.name = SDL_GetDisplayName(displayID);
            m.nativeHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(displayID));
            m.isPrimary = (displayID == primaryID);

            if (const SDL_DisplayMode* desktopMode = SDL_GetDesktopDisplayMode(displayID))
            {
                m.desktopMode = {
                    static_cast<u32>(desktopMode->w),
                    static_cast<u32>(desktopMode->h),
                    static_cast<u32>(desktopMode->refresh_rate)
                };
            }

            int modeCount = 0;
            if (SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(displayID, &modeCount))
            {
                for (int j = 0; j < modeCount; ++j)
                {
                    VideoMode mode = {
                        static_cast<u32>(modes[j]->w),
                        static_cast<u32>(modes[j]->h),
                        static_cast<u32>(modes[j]->refresh_rate)
                    };
                    m.supportedModes[GetAspectRatio(mode.width, mode.height)].push_back(mode);
                }
                SDL_free(modes);
            }

            for (auto& supportedList : m.supportedModes | std::views::values)
            {
                std::ranges::sort(supportedList);
                auto [first, last] = std::ranges::unique(supportedList);
                supportedList.erase(first, last);
            }

            window->displaySettings.monitors.push_back(std::move(m));
        }
        SDL_free(displays);
    }

    auto it = std::ranges::find_if(window->displaySettings.monitors, [](const MonitorInfo& m) { return m.isPrimary; });
    window->activeMonitorIndex = (it != window->displaySettings.monitors.end())
                                     ? static_cast<i32>(std::distance(window->displaySettings.monitors.begin(), it))
                                     : 0;
}

void Platform::UpdateScreenDimensions(WindowContext& window)
{
    auto* win = static_cast<SDL_Window*>(window.handle);
    if (!win) return;

    i32 w, h;
    SDL_GetWindowSize(win, &w, &h);
    window.windowWidth = w;
    window.windowHeight = h;
}

std::wstring Platform::ConvertToWideString(std::string_view str)
{
    std::wstring out;
    out.reserve(str.size());
    for (const unsigned char c : str) out.push_back(c);
    return out;
}

std::string Platform::ConvertToString(std::wstring_view wstr)
{
    std::string out;
    out.reserve(wstr.size());
    for (const wchar_t c : wstr) out.push_back(static_cast<char>(c & 0xFF));
    return out;
}

void* Platform::Allocate(size_t size)
{
    return std::malloc(size);
}

void* Platform::AllocateFromArena(void* arena, size_t size)
{
    return static_cast<ArenaAllocator*>(arena)->Alloc(size);
}

void Platform::Free(void* ptr)
{
    std::free(ptr);
}

void Platform::StartFrame(WindowContext& window)
{
    Uint64 now = SDL_GetPerformanceCounter();
    const f64 perfFreq = static_cast<f64>(SDL_GetPerformanceFrequency());

    if (!window.displayState.isFocused)
    {
        const f64 elapsedMs = ((static_cast<f64>(now) - window.lastFrameTime) / perfFreq) * 1000.0;

        if (elapsedMs < static_cast<f64>(BACKGROUND_FRAME_TIME))
        {
            const f64 sleepMs = static_cast<f64>(BACKGROUND_FRAME_TIME) - elapsedMs;
            SDL_Delay(static_cast<Uint32>(sleepMs));
            now = SDL_GetPerformanceCounter();
        }
    }

    const f64 deltaTicks = static_cast<f64>(now) - window.lastFrameTime;
    window.deltaTime = deltaTicks / perfFreq;

    if (window.deltaTime > 0.1)
        window.deltaTime = 0.1;

    window.elapsedTime += window.deltaTime;
    window.frameTime = static_cast<f32>(window.deltaTime * 1000.0);

    window.frameTimeSum -= window.frameTimeBuffer[window.frameBufferIndex];
    window.frameTimeBuffer[window.frameBufferIndex] = window.frameTime;
    window.frameTimeSum += window.frameTime;

    window.frameBufferIndex = (window.frameBufferIndex + 1) % 60;
    window.totalFrames++;
    const f32 divisor = (window.totalFrames < 60) ? static_cast<f32>(window.totalFrames) : 60.0f;
    const f32 averageFrameTime = window.frameTimeSum / divisor;

    window.fps = (averageFrameTime > 0.0f) ? (1000.0f / averageFrameTime) : 0.0f;
    window.lastFrameTime = static_cast<f64>(now);
}

void Platform::ShowWindow(const WindowContext& window)
{
    const auto win = static_cast<SDL_Window*>(window.handle);
    if (!win) return;

    SDL_ShowWindow(win);
    SDL_RaiseWindow(win);
}

Platform::BatteryState Platform::GetBatteryState()
{
    BatteryState state = {};
    int seconds, percent;
    const SDL_PowerState power = SDL_GetPowerInfo(&seconds, &percent);

    state.batteryPercentage = percent;
    state.HasBattery = (power != SDL_POWERSTATE_NO_BATTERY && power != SDL_POWERSTATE_UNKNOWN);
    state.ACConnected = (power == SDL_POWERSTATE_CHARGING || power == SDL_POWERSTATE_CHARGED);
    state.Charging = (power == SDL_POWERSTATE_CHARGING);

    return state;
}

bool Platform::ProcessMessages(WindowContext* window)
{
    SDL_Event e;

    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (window)
            {
                window->displayState.isFocused = true;
                window->lastFrameTime = static_cast<f64>(SDL_GetPerformanceCounter());
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (window) window->displayState.isFocused = false;
            Input::ResetInputOnFocusLoss();
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            if (window)
            {
                window->displayState.isMinimized = true;
                window->displayState.isResized = false;
            }
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            if (window)
            {
                window->displayState.isMaximized = false;
                window->displayState.isMinimized = false;
            }
            break;
        case SDL_EVENT_WINDOW_MAXIMIZED:
            if (window)
            {
                window->displayState.isMaximized = true;
                window->displayState.isMinimized = false;
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            if (window)
            {
                window->displayState.isMinimized = false;
                window->displayState.isResized = true;
                window->windowWidth = e.window.data1;
                window->windowHeight = e.window.data2;
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.scancode == SDL_SCANCODE_ESCAPE) return false;
            if (e.key.scancode == SDL_SCANCODE_F11 && window)
            {
                if (window->displayMode == DisplayMode::Windowed)
                    SetDisplayMode(*window, DisplayMode::BorderlessFullscreen);
                else
                    SetDisplayMode(*window, DisplayMode::Windowed);
            }
            break;
        default:
            break;
        }

        InputSystemSDL::ProcessEvents(e);
    }
    return true;
}

bool Platform::ShowMessageBox(std::string_view message, std::string_view title, MessageBoxType type)
{
    SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_INFORMATION;
    switch (type)
    {
    case MessageBoxType::Info: flags = SDL_MESSAGEBOX_INFORMATION;
        break;
    case MessageBoxType::Warning: flags = SDL_MESSAGEBOX_WARNING;
        break;
    case MessageBoxType::Error: flags = SDL_MESSAGEBOX_ERROR;
        break;
    case MessageBoxType::YesNo:
        {
            SDL_MessageBoxButtonData buttons[2] = {
                {0, 0, "No"},
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes"}
            };
            SDL_MessageBoxData data = {flags, nullptr, title.data(), message.data(), 2, buttons, nullptr};
            i32 button = 0;
            if (SDL_ShowMessageBox(&data, &button) == 0) return button == 1;
            return false;
        }
    }
    return SDL_ShowSimpleMessageBox(flags, title.data(), message.data(), nullptr) == 0;
}

void Platform::CenterMouse(const WindowContext* window)
{
    const auto win = static_cast<SDL_Window*>(window->handle);
    if (!win) return;

    if (SDL_GetWindowRelativeMouseMode(win)) return;

    i32 w, h;
    SDL_GetWindowSize(win, &w, &h);
    SDL_WarpMouseInWindow(win, w / 2, h / 2);
}

void Platform::SetCursorVisible(const WindowContext* window, bool show)
{
    show ? SDL_ShowCursor() : SDL_HideCursor();
}

void Platform::SetCursorLocked(const WindowContext* window, bool enable)
{
    if (auto* win = static_cast<SDL_Window*>(window->handle))
        SDL_SetWindowRelativeMouseMode(win, enable);
}

bool Platform::WrapCursorToOppositeEdge(const WindowContext* window, i32 margin)
{
    if (!window || !window->handle) return false;
    const auto win = static_cast<SDL_Window*>(window->handle);

    f32 x, y;
    i32 w, h;
    SDL_GetMouseState(&x, &y);
    SDL_GetWindowSize(win, &w, &h);

    f32 nx = x, ny = y;
    bool wrapped = false;
    const f32 fM = static_cast<f32>(margin);

    if (x < fM)
    {
        nx = static_cast<f32>(w) - fM - 1.0f;
        wrapped = true;
    }
    else if (x > static_cast<f32>(w) - fM)
    {
        nx = fM + 1.0f;
        wrapped = true;
    }

    if (y < fM)
    {
        ny = static_cast<f32>(h) - fM - 1.0f;
        wrapped = true;
    }
    else if (y > static_cast<f32>(h) - fM)
    {
        ny = fM + 1.0f;
        wrapped = true;
    }

    if (wrapped) SDL_WarpMouseInWindow(win, nx, ny);
    return wrapped;
}
