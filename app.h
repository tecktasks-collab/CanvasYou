#pragma once

#include <windows.h>

int RunApp(HINSTANCE instance, int show);
void UpdateDocumentMenus();
void PresentCurrent();
void ClearLiveOutputs();
void LiveOutput_Push();
void LiveOutput_PaintHwnd(HWND hwnd);
bool App_IsLive();
bool App_LiveHold();
bool App_OutputCleared();
void RefreshSlideLists();
void RefreshProgramScreenMirror();
void RefreshScreensPanel();
void App_RebuildScreenMenus();
void App_RefreshScreenViewerTitles();
void App_OpenScreenViewer(int index);
void App_OpenVirtualDisplay(int id);
void App_OnStart();
