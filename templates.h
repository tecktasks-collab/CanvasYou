#pragma once

#include <windows.h>
#include "store.h"

bool Template_IsActive();
void Template_SetActive(bool active);

void Template_RefreshCatalog();
int Template_CatalogCount();
bool Template_CatalogGet(int index, StoreTemplate* out);

void Template_CurrentId(char* out, int outChars);
void Template_CurrentName(char* out, int outChars);
void Template_CurrentPath(char* out, int outChars);

bool Template_New(const char* name);
bool Template_Open(const char* id);
bool Template_SaveCurrent();
bool Template_SaveAs(const char* name);
bool Template_Rename(const char* name);
bool Template_PromptName(HWND owner, const char* title, const char* seed, char* out, int outChars);
void Template_SetChangedHook(void (*fn)());
bool Template_DeleteCurrent();
bool Template_Delete(const char* id);
