#pragma once
#include "Application.h"
#include "Tools/Logger.h"

#if ENGINE_PLATFORM_SDL
    #include <SDL3/SDL_main.h>
    #define ENGINE_MAIN int SDL_main(int argc, char *argv[])
#else
    #define ENGINE_MAIN int main(int argc, char** argv)
#endif

ENGINE_MAIN
{
    Logger::Init();
    {
        Application app;
        if (!app.Init())
            return -1;
        app.Run();
    }
    Logger::Shutdown();
    return 0;
}