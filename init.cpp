#include "init.h"
#include "renderer.h"
#include "manage_screens.h"
#include "store.h"
#include "document.h"

#include <string.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

enum InitPhase
{
    INIT_OFF = 0,
    INIT_RUNNING,
    INIT_READY
};

static InitPhase g_phase = INIT_OFF;
static AppMode g_mode = APP_MODE_OFF;
static HINSTANCE g_instance = nullptr;
static ULONG_PTR g_gdiplusToken = 0;

static void StopGdiPlus()
{
    if (g_gdiplusToken != 0)
    {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

static bool StartGdiPlus()
{
    if (g_gdiplusToken != 0)
    {
        return true;
    }

    Gdiplus::GdiplusStartupInput input;
    return Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr) == Gdiplus::Ok;
}

bool App_PassedInitializer(const char* cmdLine)
{
    if (cmdLine == nullptr || strstr(cmdLine, APP_INIT_ARG) == nullptr)
    {
        return false;
    }

    HANDLE gate = OpenMutexA(SYNCHRONIZE, FALSE, APP_INIT_MUTEX_NAME);
    if (gate == nullptr)
    {
        return false;
    }

    CloseHandle(gate);
    return true;
}

AppMode App_RequestedMode(const char* cmdLine)
{
    if (cmdLine != nullptr && strstr(cmdLine, "--basic") != nullptr)
    {
        return APP_MODE_BASIC;
    }

    return APP_MODE_FULL;
}

bool App_Initialize(HINSTANCE instance)
{
    return App_InitializeRequested(instance, APP_MODE_FULL);
}

bool App_InitializeRequested(HINSTANCE instance, AppMode requested)
{
    if (g_phase == INIT_READY)
    {
        return true;
    }

    g_phase = INIT_RUNNING;
    g_instance = instance;
    g_mode = APP_MODE_BASIC;

    if (requested == APP_MODE_FULL && StartGdiPlus())
    {
        g_mode = APP_MODE_FULL;
    }

    if (!Renderer_Startup())
    {
        if (g_mode == APP_MODE_FULL)
        {
            Renderer_Shutdown();
            StopGdiPlus();
            g_mode = APP_MODE_BASIC;
            if (!Renderer_Startup())
            {
                g_mode = APP_MODE_OFF;
                g_phase = INIT_OFF;
                return false;
            }
        }
        else
        {
            g_mode = APP_MODE_OFF;
            g_phase = INIT_OFF;
            return false;
        }
    }

    g_phase = INIT_READY;
    return true;
}

void App_Shutdown()
{
    Document_Save();
    ManageScreens_Close();
    Renderer_Shutdown();
    Store_Close();
    StopGdiPlus();
    g_mode = APP_MODE_OFF;
    g_phase = INIT_OFF;
    g_instance = nullptr;
}

bool App_Starting()
{
    return g_phase == INIT_RUNNING;
}

bool App_Ready()
{
    return g_phase == INIT_READY && g_mode != APP_MODE_OFF;
}

AppMode App_Mode()
{
    return g_mode;
}

bool App_HasFullGraphics()
{
    return g_mode == APP_MODE_FULL && g_phase != INIT_OFF;
}

HINSTANCE App_Instance()
{
    return g_instance;
}
