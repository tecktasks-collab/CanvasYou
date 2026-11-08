#include "templates.h"
#include "document.h"
#include "renderer.h"
#include "store.h"

#include <string.h>
#include <stdlib.h>

static bool g_active = false;
static char g_id[64] = {};
static char g_name[128] = {};
static char g_path[MAX_PATH] = {};
static StoreTemplate g_catalog[STORE_TEMPLATE_MAX] = {};
static int g_catalogCount = 0;
static void (*g_changedHook)() = nullptr;
static char g_promptName[128] = {};
#define ID_TPL_NAME_EDIT 202

static void NotifyChanged()
{
    if (g_changedHook != nullptr)
    {
        g_changedHook();
    }
}

static void JoinPath(char* out, int outChars, const char* folder, const char* name)
{
    lstrcpynA(out, folder, outChars);
    const int len = lstrlenA(out);
    if (len > 0 && out[len - 1] != '\\' && len + 1 < outChars)
    {
        out[len] = '\\';
        out[len + 1] = '\0';
    }
    lstrcatA(out, name);
}

static char* ReadFileText(const char* path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return nullptr;
    }

    const DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 2 * 1024 * 1024)
    {
        CloseHandle(file);
        return nullptr;
    }

    char* text = static_cast<char*>(malloc(static_cast<size_t>(size) + 1));
    if (text == nullptr)
    {
        CloseHandle(file);
        return nullptr;
    }

    DWORD read = 0;
    const BOOL ok = ReadFile(file, text, size, &read, nullptr);
    CloseHandle(file);
    if (!ok)
    {
        free(text);
        return nullptr;
    }
    text[read] = '\0';
    return text;
}

static void MakeId(char* out, int outChars)
{
    FILETIME ft = {};
    GetSystemTimeAsFileTime(&ft);
    char id[64] = {};
    wsprintfA(id, "tpl%08X%08X", ft.dwHighDateTime, ft.dwLowDateTime);
    lstrcpynA(out, id, outChars);
}

static void RememberLast()
{
    if (Store_Ready() && g_id[0] != '\0')
    {
        Store_SetString("last.template", g_id);
    }
}

static bool WriteCurrent()
{
    if (g_id[0] == '\0' || g_name[0] == '\0' || g_path[0] == '\0')
    {
        return false;
    }

    Document_EnsureFolders();
    char folder[MAX_PATH] = {};
    Document_GetTemplatesDir(folder, MAX_PATH);
    if (folder[0] != '\0')
    {
        CreateDirectoryA(folder, nullptr);
    }

    if (!Renderer_SaveTemplate(g_path, g_id, g_name))
    {
        return false;
    }

    char* layout = ReadFileText(g_path);
    const bool ok = Store_UpsertTemplate(g_id, g_name, g_path, layout);
    free(layout);
    if (ok)
    {
        RememberLast();
        Template_RefreshCatalog();
        NotifyChanged();
    }
    return ok;
}

bool Template_IsActive()
{
    return g_active;
}

void Template_SetActive(bool active)
{
    g_active = active;
}

void Template_RefreshCatalog()
{
    g_catalogCount = 0;
    if (Store_Ready())
    {
        g_catalogCount = Store_ListTemplates(g_catalog, STORE_TEMPLATE_MAX);
    }
}

int Template_CatalogCount()
{
    return g_catalogCount;
}

bool Template_CatalogGet(int index, StoreTemplate* out)
{
    if (out == nullptr || index < 0 || index >= g_catalogCount)
    {
        return false;
    }
    *out = g_catalog[index];
    return true;
}

void Template_CurrentId(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    lstrcpynA(out, g_id, outChars);
}

void Template_CurrentName(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    lstrcpynA(out, g_name, outChars);
}

void Template_CurrentPath(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    lstrcpynA(out, g_path, outChars);
}

bool Template_New(const char* name)
{
    if (!Renderer_NewDocument())
    {
        return false;
    }

    MakeId(g_id, sizeof(g_id));
    lstrcpynA(g_name, (name != nullptr && name[0] != '\0') ? name : "Untitled", sizeof(g_name));

    char folder[MAX_PATH] = {};
    Document_GetTemplatesDir(folder, MAX_PATH);
    if (folder[0] == '\0')
    {
        Store_TemplatesDir(folder, MAX_PATH);
    }
    if (folder[0] == '\0')
    {
        g_id[0] = '\0';
        g_name[0] = '\0';
        g_path[0] = '\0';
        return false;
    }
    CreateDirectoryA(folder, nullptr);

    char fileName[80] = {};
    wsprintfA(fileName, "%s.json", g_id);
    JoinPath(g_path, MAX_PATH, folder, fileName);
    return WriteCurrent();
}

bool Template_Open(const char* id)
{
    if (id == nullptr || id[0] == '\0')
    {
        return false;
    }

    StoreTemplate item = {};
    if (!Store_GetTemplate(id, &item) || item.path[0] == '\0')
    {
        return false;
    }

    char loadedName[128] = {};
    if (!Renderer_LoadTemplate(item.path, loadedName, sizeof(loadedName)))
    {
        return false;
    }

    lstrcpynA(g_id, item.id, sizeof(g_id));
    lstrcpynA(g_name, loadedName[0] != '\0' ? loadedName : item.name, sizeof(g_name));
    lstrcpynA(g_path, item.path, sizeof(g_path));
    RememberLast();
    NotifyChanged();
    return true;
}

bool Template_SaveCurrent()
{
    if (!g_active)
    {
        return false;
    }
    if (g_id[0] == '\0')
    {
        return Template_New("Untitled");
    }
    return WriteCurrent();
}

bool Template_SaveAs(const char* name)
{
    if (name == nullptr || name[0] == '\0')
    {
        return false;
    }

    MakeId(g_id, sizeof(g_id));
    lstrcpynA(g_name, name, sizeof(g_name));

    char folder[MAX_PATH] = {};
    Document_GetTemplatesDir(folder, MAX_PATH);
    if (folder[0] == '\0')
    {
        Store_TemplatesDir(folder, MAX_PATH);
    }
    if (folder[0] == '\0')
    {
        return false;
    }
    CreateDirectoryA(folder, nullptr);

    char fileName[80] = {};
    wsprintfA(fileName, "%s.json", g_id);
    JoinPath(g_path, MAX_PATH, folder, fileName);
    return WriteCurrent();
}

bool Template_Rename(const char* name)
{
    if (name == nullptr || name[0] == '\0')
    {
        return false;
    }
    if (g_id[0] == '\0')
    {
        return Template_New(name);
    }
    lstrcpynA(g_name, name, sizeof(g_name));
    return WriteCurrent();
}

void Template_SetChangedHook(void (*fn)())
{
    g_changedHook = fn;
}

static LRESULT CALLBACK TemplateNamePromptProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
                GetWindowTextA(GetDlgItem(hwnd, ID_TPL_NAME_EDIT), g_promptName, sizeof(g_promptName));
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDCANCEL)
            {
                g_promptName[0] = '\0';
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            g_promptName[0] = '\0';
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void EnsureNamePromptClass(HINSTANCE instance)
{
    static bool registered = false;
    if (registered)
    {
        return;
    }
    WNDCLASSA prompt = {};
    prompt.lpfnWndProc = TemplateNamePromptProc;
    prompt.hInstance = instance;
    prompt.lpszClassName = "TemplateNamePrompt";
    prompt.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    prompt.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&prompt);
    registered = true;
}

bool Template_PromptName(HWND owner, const char* title, const char* seed, char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return false;
    }

    HINSTANCE instance = owner != nullptr
        ? reinterpret_cast<HINSTANCE>(GetWindowLongPtrA(owner, GWLP_HINSTANCE))
        : GetModuleHandleA(nullptr);
    EnsureNamePromptClass(instance);
    lstrcpynA(g_promptName, seed != nullptr && seed[0] != '\0' ? seed : "Untitled", sizeof(g_promptName));

    HWND dlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "TemplateNamePrompt",
        title != nullptr ? title : "Rename",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        360,
        140,
        owner,
        nullptr,
        instance,
        nullptr
    );
    if (dlg == nullptr)
    {
        return false;
    }

    CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        g_promptName,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        16,
        16,
        312,
        24,
        dlg,
        (HMENU)ID_TPL_NAME_EDIT,
        instance,
        nullptr
    );
    CreateWindowExA(0, "BUTTON", "OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 168, 56, 76, 26, dlg, (HMENU)IDOK, instance, nullptr);
    CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 252, 56, 76, 26, dlg, (HMENU)IDCANCEL, instance, nullptr);

    if (owner != nullptr)
    {
        EnableWindow(owner, FALSE);
    }
    ShowWindow(dlg, SW_SHOW);
    SetFocus(GetDlgItem(dlg, ID_TPL_NAME_EDIT));

    MSG msg = {};
    bool done = false;
    bool ok = false;
    while (!done && GetMessageA(&msg, nullptr, 0, 0) > 0)
    {
        if (msg.hwnd == dlg || IsChild(dlg, msg.hwnd))
        {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN)
            {
                SendMessageA(dlg, WM_COMMAND, IDOK, 0);
                continue;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
            {
                SendMessageA(dlg, WM_COMMAND, IDCANCEL, 0);
                continue;
            }
        }
        if (!IsDialogMessageA(dlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!IsWindow(dlg))
        {
            done = true;
            ok = (g_promptName[0] != '\0');
        }
    }

    if (owner != nullptr)
    {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    if (!ok)
    {
        return false;
    }
    lstrcpynA(out, g_promptName, outChars);
    return out[0] != '\0';
}

bool Template_Delete(const char* id)
{
    if (id == nullptr || id[0] == '\0')
    {
        return false;
    }

    StoreTemplate item = {};
    if (Store_GetTemplate(id, &item) && item.path[0] != '\0')
    {
        DeleteFileA(item.path);
    }
    const bool ok = Store_DeleteTemplate(id);
    if (lstrcmpA(g_id, id) == 0)
    {
        g_id[0] = '\0';
        g_name[0] = '\0';
        g_path[0] = '\0';
    }
    Template_RefreshCatalog();
    return ok;
}

bool Template_DeleteCurrent()
{
    if (g_id[0] == '\0')
    {
        return false;
    }
    return Template_Delete(g_id);
}
