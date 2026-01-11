//
// Created by Orgest on 11/21/2025.
//
#include "Platform.h"
#include "fmt/format.h"
#include "Input/InputSystemSDL.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_messagebox.h"
#include "SDL3/SDL_timer.h"
#include "Tools/Arena.h"
#include "Tools/Logger.h"


void Platform::Init(WindowContext* window, i32 width, i32 height, DisplayMode mode)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        LOG(Error, "[SDL3] SDL_Init failed: {}", SDL_GetError());
        return;
    }

    SDL_SetHint(SDL_HINT_APP_NAME, ENGINE_NAME);
    window->platformName = "SDL3";


    const SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* currentMode = SDL_GetCurrentDisplayMode(displayID);

    // Default Fallback
    i32 monitorW = 1280;
    i32 monitorH = 720;

    if (currentMode)
    {
        monitorW = currentMode->w;
        monitorH = currentMode->h;
    }

    window->monitorWidth = monitorW;
    window->monitorHeight = monitorH;

    // 2. Set default to Half Resolution if no dimensions provided
    window->windowWidth = (width == 0) ? (monitorW / 2) : width;
    window->windowHeight = (height == 0) ? (monitorH / 2) : height;

    const std::string title = fmt::format("{} - {} - {} - {} - {} - {}",
                                          ENGINE_NAME,
                                          ENGINE_BUILD,
                                          window->platformName,
                                          ENGINE_VERSION,
                                          __DATE__,
                                          ENGINE_COMMIT_HASH
    );


    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, window->windowWidth);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, window->windowHeight);

    // Position
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);

    // Base Flags
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);

    switch (mode)
    {
    case DisplayMode::Windowed:
        // Standard windowed - no extra properties needed
        break;

    case DisplayMode::BorderlessFullscreen:
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
        break;

    case DisplayMode::Fullscreen:
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
        break;
    }


    SDL_Window* win = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    if (!win)
    {
        LOG(Error, "[SDL3] SDL_CreateWindow FAILED: {}", SDL_GetError());
        std::abort();
    }

    window->handle = win;
    window->displayMode = mode;

    window->perfCountFrequency = static_cast<i64>(SDL_GetPerformanceFrequency());
    window->lastFrameTime = static_cast<f64>(SDL_GetPerformanceCounter());
}

bool Platform::GetWindowSize(const WindowHandle& handle, u32& width, u32& height)
{
    const auto win = static_cast<SDL_Window*>(handle);
    if (!win)
        return false;

    i32 w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);

    width = static_cast<u32>(w);
    height = static_cast<u32>(h);
    return true;
}

void Platform::SetDisplayMode(WindowContext& window, DisplayMode mode)
{
    const auto win = static_cast<SDL_Window*>(window.handle);
    if (!win)
        return;

    window.displayMode = mode;

    switch (mode)
    {
    case DisplayMode::Windowed:
        SDL_SetWindowFullscreenMode(win, nullptr);
        SDL_SetWindowBordered(win, true);
        window.displayState.isBorderless = 0;
        window.displayState.isExclusiveFullscreen = 0;
        break;

    case DisplayMode::BorderlessFullscreen:
        {
            SDL_DisplayID display = SDL_GetPrimaryDisplay();
            const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(display);

            SDL_SetWindowFullscreenMode(win, dm);
            SDL_SetWindowBordered(win, false);

            window.displayState.isBorderless = 1;
            window.displayState.isExclusiveFullscreen = 0;
            break;
        }

    case DisplayMode::Fullscreen:
        {
            SDL_DisplayID display = SDL_GetPrimaryDisplay();
            const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(display);

            SDL_SetWindowFullscreenMode(win, dm);

            window.displayState.isExclusiveFullscreen = 1;
            window.displayState.isBorderless = 0;
            break;
        }
    }

    // Update cached values
    i32 w, h;
    SDL_GetWindowSize(win, &w, &h);
    window.windowWidth = w;
    window.windowHeight = h;
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
    const Uint64 now = SDL_GetPerformanceCounter();

    const f64 deltaTicks = static_cast<f64>(now) - window.lastFrameTime;
    window.deltaTime = deltaTicks / static_cast<f64>(window.perfCountFrequency);
    window.elapsedTime += window.deltaTime;

    window.frameTime = window.deltaTime * 1000.0;
    window.fps = (window.deltaTime > 0.0) ? 1.0 / window.deltaTime : 0.0f;

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
                window->lastFrameTime = (double)SDL_GetPerformanceCounter();
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
        case SDL_EVENT_WINDOW_MAXIMIZED:
            if (window)
            {
                window->displayState.isMinimized = 0;
                window->displayState.isResized = 1;
                window->lastFrameTime = static_cast<double>(SDL_GetPerformanceCounter());
            }
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            if (window)
            {
                window->windowWidth = e.window.data1;
                window->windowHeight = e.window.data2;
                window->displayState.isResized = 1;
            }
            break;
        }


        InputSystemSDL::ProcessEvent(e);
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

void Platform::LockCursor(const WindowContext& window, bool enable)
{
    auto* win = static_cast<SDL_Window*>(window.handle);
    if (!win) return;

    if (enable)
    {
        SDL_SetWindowRelativeMouseMode(win, true);
        SDL_CaptureMouse(true);
    }
    else
    {
        SDL_SetWindowRelativeMouseMode(win, false);
        SDL_CaptureMouse(false);
    }
}

bool Platform::WrapCursorToOppositeEdge(const WindowContext* window, i32 margin)
{
    const auto win = static_cast<SDL_Window*>(window->handle);
    if (!win) return false;

    // Get current cursor pos relative to window
    f32 fx, fy;
    if (SDL_GetMouseState(&fx, &fy) <= 0)
        return false;

    const i32 x = static_cast<i32>(fx);
    const i32 y = static_cast<i32>(fy);

    i32 w, h;
    SDL_GetWindowSize(win, &w, &h);

    i32 newX = x;
    i32 newY = y;
    bool wrap = false;

    if (x < margin)
    {
        newX = w - margin - 1;
        wrap = true;
    }
    else if (x > w - margin)
    {
        newX = margin + 1;
        wrap = true;
    }

    if (y < margin)
    {
        newY = h - margin - 1;
        wrap = true;
    }
    else if (y > h - margin)
    {
        newY = margin + 1;
        wrap = true;
    }

    if (wrap)
    {
        SDL_WarpMouseInWindow(win, newX, newY);
        return true;
    }

    return false;
}