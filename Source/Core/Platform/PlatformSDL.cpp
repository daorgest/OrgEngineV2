//
// Created by Orgest on 11/21/2025.
//
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

    const SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* currentMode = SDL_GetCurrentDisplayMode(displayID);
    window->monitorWidth = currentMode->w;
    window->monitorHeight = currentMode->h;

    window->windowWidth = (width == 0) ? window->monitorWidth : width;
    window->windowHeight = (height == 0) ? window->monitorHeight : height;

    const std::string title = fmt::format("{} - {} - {} - {} - {} - {}",
                                          ENGINE_NAME,
                                          ENGINE_BUILD,
                                          window->platformName,
                                          ENGINE_VERSION,
                                          __DATE__,
                                          ENGINE_COMMIT_HASH
    );


    SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;

    if (mode == DisplayMode::BorderlessFullscreen)
    {
        // WS_POPUP | WS_VISIBLE equivalent
        windowFlags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_MAXIMIZED;
    }

    SDL_Window* sdlWindow = SDL_CreateWindow(title.c_str(), window->monitorWidth, window->monitorHeight, windowFlags);
    if (!sdlWindow)
    {
        LOG(Error, "[SDL3] SDL_CreateWindow FAILED: {}", SDL_GetError());
        SDL_Quit();
        return;
    }

    if (mode == DisplayMode::Windowed)
    {
        SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    window->handle = sdlWindow;
    window->displayMode = mode;

    window->perfCountFrequency = static_cast<i64>(SDL_GetPerformanceFrequency());
    window->lastFrameTime = static_cast<f64>(SDL_GetPerformanceCounter());

    SDL_MaximizeWindow(sdlWindow);
    SDL_ShowWindow(sdlWindow);
}

bool Platform::GetWindowSize(const WindowHandle& handle, u32& width, u32& height)
{
    const auto win = static_cast<SDL_Window*>(handle);
    if (!win)
        return false;

    SDL_GetWindowSize(win, reinterpret_cast<i32*>(&width), reinterpret_cast<i32*>(&height));

    return true;
}

void Platform::SetDisplayMode(WindowContext& window, const DisplayMode mode)
{
    const auto win = static_cast<SDL_Window*>(window.handle);
    if (!win)
        return;

    if (window.displayMode == DisplayMode::Windowed && !window.displayState.isMaximized)
    {
        SDL_GetWindowSize(win, &window.lastWindowWidth, &window.lastWindowHeight);
    }

    switch (mode)
    {
    case DisplayMode::Windowed:
        {
            SDL_SetWindowFullscreen(win, false);
            SDL_SetWindowBordered(win, true);
            SDL_SetWindowResizable(win, true);

            if (window.displayState.isMaximized)
            {
                SDL_MaximizeWindow(win);
                SDL_RestoreWindow(win);
            }
            else
            {
                SDL_SetWindowSize(win, window.lastWindowWidth, window.lastWindowHeight);
                SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                SDL_RestoreWindow(win);
            }

            window.displayState.isBorderless = 0;
            window.displayState.isExclusiveFullscreen = 0;
            break;
        }
    case DisplayMode::BorderlessFullscreen:
        {
            SDL_SetWindowFullscreenMode(win, nullptr);
            SDL_SetWindowFullscreen(win, true);

            window.displayState.isBorderless = 1;
            window.displayState.isExclusiveFullscreen = 0;
            break;
        }
    default:
        break;
    }

    window.displayMode = mode;

    // i32 w, h;
    // SDL_GetWindowSize(win, &w, &h);
    // window.windowWidth = w;
    // window.windowHeight = h;
}

void Platform::UpdateScreenDimensions(WindowContext& window)
{
    auto* win = static_cast<SDL_Window*>(window.handle);
    if (!win)
        return;

    i32 w, h;
    SDL_GetWindowSize(win, &w, &h);
    window.windowWidth = w;
    window.windowHeight = h;

    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (!SDL_GetCurrentDisplayMode(display))
    {
        constexpr SDL_DisplayMode dm = {};
        window.monitorWidth = dm.w;
        window.monitorHeight = dm.h;
    }
}

std::wstring Platform::ConvertToWideString(const std::string_view& str)
{
    std::wstring out;
    out.reserve(str.size());
    for (const unsigned char c : str)
        out.push_back(c);
    return out;
}

std::string Platform::ConvertToString(const std::wstring_view& wstr)
{
    std::string out;
    out.reserve(wstr.size());
    for (const wchar_t c : wstr)
        out.push_back(static_cast<char>(c & 0xFF));
    return out;
}

void* Platform::Allocate(size_t size)
{
    return std::malloc(size);
}

void* Platform::AllocateFromArena(void* arena, size_t size)
{
    auto* a = static_cast<ArenaAllocator*>(arena);
    return a->Alloc(size);
}

void Platform::Free(void* ptr)
{
    std::free(ptr);
}

Platform::WindowHandle Platform::GetNativeWindowHandle(const WindowContext& window)
{
    return window.handle;
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

            // Re-sample time after sleeping
            now = SDL_GetPerformanceCounter();
        }
    }

    const f64 deltaTicks = static_cast<f64>(now) - window.lastFrameTime;
    window.deltaTime = deltaTicks / perfFreq;

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
    const f32 averageFrameTimeMs = totalTime / 60.0f;
    window.fps = (averageFrameTimeMs > 0.0f) ? (1000.0f / averageFrameTimeMs) : 0.0f;

    window.lastFrameTime = static_cast<f64>(now);
}

void Platform::ShowWindow(const WindowContext& window)
{
    const auto win = static_cast<SDL_Window*>(window.handle);
    if (!win) return;

    SDL_ShowWindow(win);
    SDL_RaiseWindow(win);
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
                window->displayState.isFocused = 1;
                window->lastFrameTime = static_cast<f64>(SDL_GetPerformanceCounter());
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (window)
            {
                window->displayState.isFocused = 0;
            }
            Input::ResetInputOnFocusLoss();
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            if (window)
            {
                window->displayState.isMinimized = 1;
                window->displayState.isResized = 0;
            }
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            if (window)
            {
                if (window->displayMode == DisplayMode::Windowed) {
                    window->displayState.isMaximized = 0;
                }
                window->displayState.isMinimized = 0;
            }
            break;
        case SDL_EVENT_WINDOW_MAXIMIZED:
            if (window)
            {
                window->displayState.isMaximized = 1;
                window->displayState.isMinimized = 0;
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            if (window)
            {
                if (e.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    window->displayState.isMinimized = true;
                    window->displayState.isResized = false;
                }
                else
                {
                    window->displayState.isMinimized = false;
                    window->displayState.isResized = true;
                }

                window->windowWidth = e.window.data1;
                window->windowHeight = e.window.data2;
            }
            break;

        case SDL_EVENT_KEY_DOWN: // System keys
            {
                if (e.key.scancode == SDL_SCANCODE_ESCAPE)
                {
                    return false;
                }

                if (e.key.scancode == SDL_SCANCODE_F11)
                {
                    if (window->displayMode == DisplayMode::Windowed)
                    {
                        SetDisplayMode(*window, DisplayMode::BorderlessFullscreen);
                    }
                    else
                    {
                        SetDisplayMode(*window, DisplayMode::Windowed);
                    }
                }
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
            SDL_MessageBoxButtonData buttons[2] =
            {
                {0, 0, "No"},
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes"}
            };

            SDL_MessageBoxData data{};
            data.flags = flags;
            data.title = title.data();
            data.message = message.data();
            data.numbuttons = 2;
            data.buttons = buttons;

            i32 button = 0;
            if (SDL_ShowMessageBox(&data, &button) == 0)
                return button == 1;
            return false;
        }
    }

    return SDL_ShowSimpleMessageBox(flags, title.data(), message.data(), nullptr) == 0;
}

void Platform::CenterMouse(const WindowContext* window)
{
    const auto win = static_cast<SDL_Window*>(window->handle);
    if (!win) return;

    i32 w, h;
    SDL_GetWindowSize(win, &w, &h);

    SDL_WarpMouseInWindow(win, w / 2, h / 2);
}

void Platform::SetCursorVisible(bool show)
{
    if (show)
    {
        SDL_ShowCursor();
    }
    else
    {
        SDL_HideCursor();
    }
}

void Platform::SetCursorLocked(const WindowContext* window, bool enable)
{
    auto* win = static_cast<SDL_Window*>(window->handle);
    if (!win) return;

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

    const f32 fMargin = static_cast<f32>(margin);
    const f32 fW = static_cast<f32>(w);
    const f32 fH = static_cast<f32>(h);

    if (x < fMargin)
    {
        nx = fW - fMargin - 1.0f;
        wrapped = true;
    }
    else if (x > fW - fMargin)
    {
        nx = fMargin + 1.0f;
        wrapped = true;
    }

    if (y < fMargin)
    {
        ny = fH - fMargin - 1.0f;
        wrapped = true;
    }
    else if (y > fH - fMargin)
    {
        ny = fMargin + 1.0f;
        wrapped = true;
    }

    if (wrapped)
    {
        SDL_WarpMouseInWindow(win, nx, ny);
    }
    return wrapped;
}