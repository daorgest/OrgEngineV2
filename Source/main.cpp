#pragma once
#include "Application.h"
#include "Tools/Logger.h"

#if ENGINE_PLATFORM_WIN32

#ifdef NDEBUG
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif

#elif ENGINE_PLATFORM_SDL
int main()

#else
int main()
#endif
{
    Logger::Init();
    Application app;

    if (!app.Init())
        return -1;

    app.Run();
    app.Cleanup();
    Logger::Shutdown();
    return 0;
}