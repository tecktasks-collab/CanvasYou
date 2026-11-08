#pragma once

#include <windows.h>
#include "boot.h"

#define SCREEN_LAYOUT_SINGLE 0
#define SCREEN_LAYOUT_SPAN 1
#define SCREEN_LAYOUT_MIRROR 2

void ManageScreens_Show(HWND owner, HINSTANCE instance);
void ManageScreens_Present();
void ManageScreens_Close();
int ManageScreens_CollectHwnds(HWND* out, int max);
void ManageScreens_LoadPending(const BootTicket* ticket);
void ManageScreens_LoadFromStore();
void ManageScreens_Restore(HWND owner, HINSTANCE instance);
void ManageScreens_CreateIn(HWND parent, HINSTANCE instance);
void ManageScreens_Layout(int x, int y, int width, int height);
void ManageScreens_RefreshUi();
int ManageScreens_OutputCount();
bool ManageScreens_GetOutputLabel(int index, char* out, int outChars);
int ManageScreens_ScreenCount();
int ManageScreens_ActiveScreen();
int ManageScreens_AddVirtualScreen();
int ManageScreens_AddVirtualDisplay(int modeIndex);
int ManageScreens_VirtualCount();
int ManageScreens_GetVirtualId(int index);
bool ManageScreens_GetVirtualLabel(int index, char* out, int outChars);
HWND ManageScreens_GetVirtualHwnd(int id);
void ManageScreens_SetVirtualHwnd(int id, HWND hwnd);
void ManageScreens_NoteVirtualClosed(int id);
bool ManageScreens_ActivateScreen(int index);
bool ManageScreens_GetScreenLabel(int index, char* out, int outChars);
int ManageScreens_GetScreenLayout(int index);
bool ManageScreens_SetScreenLayout(int index, int layout);
int ManageScreens_GetScreenOutputCount(int index);
bool ManageScreens_GetScreenOutputLabel(int index, int slot, char* out, int outChars);
bool ManageScreens_GetScreenOutputSlice(int index, int slot, int* left, int* right);
bool ManageScreens_GetScreenCanvas(int index, int* width, int* height, int* refreshHz);
bool ManageScreens_SetScreenCanvas(int index, int width, int height, int refreshHz);
void ManageScreens_SetOutputHwnd(int index, HWND hwnd);
void ManageScreens_NoteViewerClosed(int index);
HWND ManageScreens_GetOutputHwnd(int index);
HWND ManageScreens_GetScreenOutputHwnd(int screen, int slot);
bool ManageScreens_OutputsEnabled();
void ManageScreens_SetOutputsEnabled(bool enabled);
