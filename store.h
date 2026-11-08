#pragma once

#include <windows.h>

bool Store_ResolveRoot(char* out, int outChars);
bool Store_Open();
bool Store_OpenAt(const char* root);
void Store_Close();
bool Store_Ready();
void Store_Root(char* out, int outChars);
struct sqlite3;
sqlite3* Store_Db();


int Store_GetInt(const char* key, int fallback);
void Store_SetInt(const char* key, int value);
void Store_GetString(const char* key, char* out, int outChars, const char* fallback);
void Store_SetString(const char* key, const char* value);

#define STORE_PROJECT_MAX 128

struct StoreProject
{
    char name[128];
    char path[MAX_PATH];
};

bool Store_RememberProject(const char* name, const char* path);
int Store_ListProjects(StoreProject* out, int max);
bool Store_SetProjectOrder(const StoreProject* items, int count);

#define STORE_SCREEN_MAX 8
#define STORE_SCREEN_OUTPUT_MAX 8
#define STORE_VIRTUAL_MAX 32

struct StoreScreen
{
    char name[64];
    int layoutMode;
    int outputCount;
    int canvasW;
    int canvasH;
    int refreshHz;
    char devices[STORE_SCREEN_OUTPUT_MAX][32];
};

int Store_LoadScreens(StoreScreen* out, int max, int* activeOut);
bool Store_SaveScreens(const StoreScreen* items, int count, int active);

struct StoreVirtual
{
    int id;
    char mode[16];
};

int Store_LoadVirtuals(StoreVirtual* out, int max);
bool Store_SaveVirtuals(const StoreVirtual* items, int count);

#define STORE_TEMPLATE_MAX 256

struct StoreTemplate
{
    char id[64];
    char name[128];
    char path[MAX_PATH];
};

void Store_TemplatesDir(char* out, int outChars);
int Store_ListTemplates(StoreTemplate* out, int max);
bool Store_UpsertTemplate(const char* id, const char* name, const char* path, const char* layout);
bool Store_DeleteTemplate(const char* id);
bool Store_GetTemplate(const char* id, StoreTemplate* out);
