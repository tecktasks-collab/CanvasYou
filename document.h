#pragma once

#include <windows.h>

void Document_GetFilesDir(char* out, int outChars);
void Document_GetAssetsDir(char* out, int outChars);
void Document_GetScreensDir(char* out, int outChars);
void Document_GetTemplatesDir(char* out, int outChars);
void Document_SetFilesDir(const char* path);
void Document_SetAssetsDir(const char* path);
void Document_SetScreensDir(const char* path);
void Document_SetTemplatesDir(const char* path);

bool Document_HasProject();
void Document_Name(char* out, int outChars);
void Document_Path(char* out, int outChars);
bool Document_NewProject(const char* name);
bool Document_AddSlide();
bool Document_Open(const char* path);
bool Document_Save();
bool Document_SaveOrWarn(HWND owner);
bool Document_ImportAsset(const wchar_t* source, wchar_t* outName, int outChars);
int Document_PlanImport(const wchar_t* source, wchar_t* outName, int nameChars, wchar_t* outDest, int destChars);
bool Document_ResolveAsset(const wchar_t* stored, wchar_t* outPath, int outChars);
void Document_EnsureFolders();
