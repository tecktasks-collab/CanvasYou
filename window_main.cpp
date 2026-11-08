#include <windows.h>
#include "app.h"
#include "init.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR cmdLine, int show)
{
    if (!App_PassedInitializer(cmdLine))
    {
        MessageBoxA(
            nullptr,
            "Window.exe does not start on its own.\n\nRun Initializer.exe.\nOr run Basic.exe for the program with no init.",
            "Initializer",
            MB_OK | MB_ICONINFORMATION
        );
        return 0;
    }

    if (!App_InitializeRequested(instance, App_RequestedMode(cmdLine)))
    {
        MessageBoxA(nullptr, "Startup failed.", "Initializer", MB_OK | MB_ICONERROR);
        return 0;
    }

    return RunApp(instance, show);
}
