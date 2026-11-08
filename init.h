#pragma once

#include <windows.h>

#define APP_INIT_MUTEX_NAME "Local\\PresentationSoftwareInitProcess"
#define APP_INIT_ARG "--from-init"

enum AppMode
{
    APP_MODE_OFF = 0,
    APP_MODE_BASIC,
    APP_MODE_FULL
};

bool App_PassedInitializer(const char* cmdLine);
AppMode App_RequestedMode(const char* cmdLine);

bool App_Initialize(HINSTANCE instance);
bool App_InitializeRequested(HINSTANCE instance, AppMode requested);
void App_Shutdown();

bool App_Starting();
bool App_Ready();
AppMode App_Mode();
bool App_HasFullGraphics();
HINSTANCE App_Instance();
