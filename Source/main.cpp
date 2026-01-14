#pragma once
#include "Application.h"
#include "Tools/Logger.h"

#if ENGINE_PLATFORM_SDL
    #include <SDL3/SDL_main.h>
    #define ENGINE_MAIN int SDL_main(int argc, char *argv[])
#elif ENGINE_PLATFORM_WIN32
    #ifdef NDEBUG
        #include <windows.h>
        #define ENGINE_MAIN i32 WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
    #else
        #define ENGINE_MAIN int main()
    #endif
#else
    #define ENGINE_MAIN int main()
#endif

ENGINE_MAIN
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