#include "history.h"
#include "document.h"
#include "renderer.h"

#include <stdlib.h>
#include <string.h>

#define HISTORY_MAX 32

struct HistoryItem
{
    char* data;
    int bytes;
};

static HistoryItem g_undo[HISTORY_MAX] = {};
static int g_undoCount = 0;
static HistoryItem g_redo[HISTORY_MAX] = {};
static int g_redoCount = 0;
static bool g_busy = false;

static void FreeItem(HistoryItem* item)
{
    free(item->data);
    item->data = nullptr;
    item->bytes = 0;
}

static void ClearStack(HistoryItem* stack, int* count)
{
    for (int i = 0; i < *count; ++i)
    {
        FreeItem(&stack[i]);
    }
    *count = 0;
}

static void PushStack(HistoryItem* stack, int* count, char* data, int bytes)
{
    if (*count >= HISTORY_MAX)
    {
        FreeItem(&stack[0]);
        for (int i = 1; i < HISTORY_MAX; ++i)
        {
            stack[i - 1] = stack[i];
        }
        *count = HISTORY_MAX - 1;
    }
    stack[*count].data = data;
    stack[*count].bytes = bytes;
    *count += 1;
}

static bool PopStack(HistoryItem* stack, int* count, HistoryItem* out)
{
    if (*count <= 0)
    {
        return false;
    }
    *count -= 1;
    *out = stack[*count];
    stack[*count].data = nullptr;
    stack[*count].bytes = 0;
    return true;
}

static char* ReadAll(const char* path, int* outBytes)
{
    *outBytes = 0;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return nullptr;
    }
    const DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 8 * 1024 * 1024)
    {
        CloseHandle(file);
        return nullptr;
    }
    char* data = static_cast<char*>(malloc(static_cast<size_t>(size) + 1));
    if (data == nullptr)
    {
        CloseHandle(file);
        return nullptr;
    }
    DWORD read = 0;
    const BOOL ok = ReadFile(file, data, size, &read, nullptr);
    CloseHandle(file);
    if (!ok)
    {
        free(data);
        return nullptr;
    }
    data[read] = '\0';
    *outBytes = static_cast<int>(read);
    return data;
}

static bool WriteAll(const char* path, const char* data, int bytes)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, data, static_cast<DWORD>(bytes), &written, nullptr);
    CloseHandle(file);
    return ok == TRUE;
}

static bool SnapshotCurrent(char** outData, int* outBytes)
{
    *outData = nullptr;
    *outBytes = 0;
    if (!Document_HasProject())
    {
        return false;
    }
    if (!Document_Save())
    {
        return false;
    }
    char path[MAX_PATH] = {};
    Document_Path(path, MAX_PATH);
    if (path[0] == '\0')
    {
        return false;
    }
    *outData = ReadAll(path, outBytes);
    return *outData != nullptr;
}

static bool RestoreItem(const HistoryItem* item)
{
    if (item == nullptr || item->data == nullptr || item->bytes <= 0)
    {
        return false;
    }
    char path[MAX_PATH] = {};
    Document_Path(path, MAX_PATH);
    if (path[0] == '\0')
    {
        return false;
    }
    if (!WriteAll(path, item->data, item->bytes))
    {
        return false;
    }
    g_busy = true;
    const bool ok = Document_Open(path);
    g_busy = false;
    return ok;
}

void History_Clear()
{
    ClearStack(g_undo, &g_undoCount);
    ClearStack(g_redo, &g_redoCount);
}

void History_Remember()
{
    if (g_busy || !Document_HasProject())
    {
        return;
    }
    char* data = nullptr;
    int bytes = 0;
    if (!SnapshotCurrent(&data, &bytes))
    {
        return;
    }
    if (g_undoCount > 0 && g_undo[g_undoCount - 1].bytes == bytes &&
        memcmp(g_undo[g_undoCount - 1].data, data, static_cast<size_t>(bytes)) == 0)
    {
        free(data);
        return;
    }
    PushStack(g_undo, &g_undoCount, data, bytes);
    ClearStack(g_redo, &g_redoCount);
}

bool History_CanUndo()
{
    return g_undoCount > 0 && Document_HasProject();
}

bool History_CanRedo()
{
    return g_redoCount > 0 && Document_HasProject();
}

bool History_Undo()
{
    if (!History_CanUndo())
    {
        return false;
    }
    char* current = nullptr;
    int currentBytes = 0;
    SnapshotCurrent(&current, &currentBytes);
    HistoryItem item = {};
    if (!PopStack(g_undo, &g_undoCount, &item))
    {
        free(current);
        return false;
    }
    const bool ok = RestoreItem(&item);
    FreeItem(&item);
    if (ok && current != nullptr)
    {
        PushStack(g_redo, &g_redoCount, current, currentBytes);
    }
    else
    {
        free(current);
    }
    return ok;
}

bool History_Redo()
{
    if (!History_CanRedo())
    {
        return false;
    }
    char* current = nullptr;
    int currentBytes = 0;
    SnapshotCurrent(&current, &currentBytes);
    HistoryItem item = {};
    if (!PopStack(g_redo, &g_redoCount, &item))
    {
        free(current);
        return false;
    }
    const bool ok = RestoreItem(&item);
    FreeItem(&item);
    if (ok && current != nullptr)
    {
        PushStack(g_undo, &g_undoCount, current, currentBytes);
    }
    else
    {
        free(current);
    }
    return ok;
}
