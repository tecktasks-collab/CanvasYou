#include <windows.h>
#include <string.h>

#include "init.h"
#include "store.h"
#include "document.h"
#include "edit_view.h"
#include "templates.h"
#include "renderer.h"

#define ID_TPL_NEW 101
#define ID_TPL_OPEN 102
#define ID_TPL_SAVE 103
#define ID_TPL_SAVEAS 104
#define ID_TPL_DELETE 105
#define ID_TPL_EXIT 106
#define ID_OPEN_LIST 201
#define ID_NAME_EDIT 202

static HWND g_window = nullptr;
static HINSTANCE g_instance = nullptr;
static char g_promptName[128] = {};
static char g_openId[64] = {};

void ManageScreens_Close()
{
}

void UpdateDocumentMenus()
{
}

void RefreshSlideLists()
{
    EditView_Refresh();
}

static void UpdateTitle()
{
    if (g_window == nullptr)
    {
        return;
    }

    char name[128] = {};
    Template_CurrentName(name, sizeof(name));
    char title[160] = {};
    if (name[0] != '\0')
    {
        wsprintfA(title, "Template Maker - %s", name);
    }
    else
    {
        lstrcpyA(title, "Template Maker");
    }
    SetWindowTextA(g_window, title);
}

static bool PromptName(HWND owner, const char* title, const char* seed, char* out, int outChars)
{
    lstrcpynA(g_promptName, seed != nullptr ? seed : "Untitled", sizeof(g_promptName));

    HWND dlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "TemplatePrompt",
        title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        360,
        140,
        owner,
        nullptr,
        g_instance,
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
        (HMENU)ID_NAME_EDIT,
        g_instance,
        nullptr
    );
    CreateWindowExA(0, "BUTTON", "OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 168, 56, 76, 26, dlg, (HMENU)IDOK, g_instance, nullptr);
    CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 252, 56, 76, 26, dlg, (HMENU)IDCANCEL, g_instance, nullptr);

    EnableWindow(owner, FALSE);
    ShowWindow(dlg, SW_SHOW);
    SetFocus(GetDlgItem(dlg, ID_NAME_EDIT));

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

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (ok)
    {
        lstrcpynA(out, g_promptName, outChars);
    }
    return ok;
}

static LRESULT CALLBACK PromptProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
                GetWindowTextA(GetDlgItem(hwnd, ID_NAME_EDIT), g_promptName, sizeof(g_promptName));
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

static bool PromptOpen(HWND owner)
{
    Template_RefreshCatalog();
    const int count = Template_CatalogCount();
    if (count <= 0)
    {
        MessageBoxA(owner, "No templates in the program Templates folder yet.", "Open", MB_OK | MB_ICONINFORMATION);
        return false;
    }

    g_openId[0] = '\0';
    HWND dlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "TemplateOpenPrompt",
        "Open Template",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        400,
        320,
        owner,
        nullptr,
        g_instance,
        nullptr
    );
    if (dlg == nullptr)
    {
        return false;
    }

    HWND list = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "LISTBOX",
        "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY,
        16,
        16,
        352,
        220,
        dlg,
        (HMENU)ID_OPEN_LIST,
        g_instance,
        nullptr
    );
    for (int i = 0; i < count; ++i)
    {
        StoreTemplate item = {};
        if (Template_CatalogGet(i, &item))
        {
            const int row = static_cast<int>(SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.name)));
            SendMessageA(list, LB_SETITEMDATA, row, i);
        }
    }
    SendMessageA(list, LB_SETCURSEL, 0, 0);
    CreateWindowExA(0, "BUTTON", "Open", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 208, 248, 76, 26, dlg, (HMENU)IDOK, g_instance, nullptr);
    CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 292, 248, 76, 26, dlg, (HMENU)IDCANCEL, g_instance, nullptr);

    EnableWindow(owner, FALSE);
    ShowWindow(dlg, SW_SHOW);
    SetFocus(list);

    MSG msg = {};
    bool done = false;
    while (!done && GetMessageA(&msg, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageA(dlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!IsWindow(dlg))
        {
            done = true;
        }
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return g_openId[0] != '\0';
}

static void FinishOpenDialog(HWND hwnd, bool accept)
{
    if (accept)
    {
        const int row = static_cast<int>(SendMessageA(GetDlgItem(hwnd, ID_OPEN_LIST), LB_GETCURSEL, 0, 0));
        const int index = static_cast<int>(SendMessageA(GetDlgItem(hwnd, ID_OPEN_LIST), LB_GETITEMDATA, row, 0));
        StoreTemplate item = {};
        if (row >= 0 && Template_CatalogGet(index, &item))
        {
            lstrcpynA(g_openId, item.id, sizeof(g_openId));
        }
    }
    else
    {
        g_openId[0] = '\0';
    }
    DestroyWindow(hwnd);
}

static LRESULT CALLBACK OpenPromptProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || (LOWORD(wParam) == ID_OPEN_LIST && HIWORD(wParam) == LBN_DBLCLK))
            {
                FinishOpenDialog(hwnd, true);
                return 0;
            }
            if (LOWORD(wParam) == IDCANCEL)
            {
                FinishOpenDialog(hwnd, false);
                return 0;
            }
            break;
        case WM_CLOSE:
            FinishOpenDialog(hwnd, false);
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void DoNew()
{
    char name[128] = "Untitled";
    if (!Template_PromptName(g_window, "New Template", "Untitled", name, sizeof(name)))
    {
        return;
    }
    if (!Template_New(name))
    {
        MessageBoxA(g_window, "Could not create that template.", "Template Maker", MB_OK | MB_ICONWARNING);
        return;
    }
    EditView_OpenSlide(0);
    UpdateTitle();
}

static void DoOpen()
{
    if (!PromptOpen(g_window))
    {
        return;
    }
    Template_SaveCurrent();
    if (!Template_Open(g_openId))
    {
        MessageBoxA(g_window, "Could not open that template.", "Template Maker", MB_OK | MB_ICONWARNING);
        return;
    }
    EditView_OpenSlide(0);
    UpdateTitle();
}

static void DoSave()
{
    if (!Template_SaveCurrent())
    {
        MessageBoxA(g_window, "Could not save the template.", "Template Maker", MB_OK | MB_ICONWARNING);
        return;
    }
    UpdateTitle();
}

static void DoSaveAs()
{
    char seed[128] = {};
    Template_CurrentName(seed, sizeof(seed));
    char name[128] = {};
    if (!Template_PromptName(g_window, "Save Template As", seed[0] != '\0' ? seed : "Untitled", name, sizeof(name)))
    {
        return;
    }
    if (!Template_SaveAs(name))
    {
        MessageBoxA(g_window, "Could not save that template.", "Template Maker", MB_OK | MB_ICONWARNING);
        return;
    }
    UpdateTitle();
}

static void DoDelete()
{
    char name[128] = {};
    Template_CurrentName(name, sizeof(name));
    char text[200] = {};
    wsprintfA(text, "Delete \"%s\" from the Templates folder?", name[0] != '\0' ? name : "this template");
    if (MessageBoxA(g_window, text, "Template Maker", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK)
    {
        return;
    }
    if (!Template_DeleteCurrent() || !Template_New("Untitled"))
    {
        MessageBoxA(g_window, "Could not delete that template.", "Template Maker", MB_OK | MB_ICONWARNING);
        return;
    }
    EditView_OpenSlide(0);
    UpdateTitle();
}

static HMENU CreateMakerMenu()
{
    HMENU file = CreateMenu();
    AppendMenuA(file, MF_STRING, ID_TPL_NEW, "&New\tCtrl+N");
    AppendMenuA(file, MF_STRING, ID_TPL_OPEN, "&Open...\tCtrl+O");
    AppendMenuA(file, MF_STRING, ID_TPL_SAVE, "&Save\tCtrl+S");
    AppendMenuA(file, MF_STRING, ID_TPL_SAVEAS, "Save &As...");
    AppendMenuA(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(file, MF_STRING, ID_TPL_DELETE, "&Delete...");
    AppendMenuA(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(file, MF_STRING, ID_TPL_EXIT, "E&xit");

    HMENU bar = CreateMenu();
    AppendMenuA(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), "&File");
    return bar;
}

static LRESULT CALLBACK MakerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case ID_TPL_NEW:
                    DoNew();
                    return 0;
                case ID_TPL_OPEN:
                    DoOpen();
                    return 0;
                case ID_TPL_SAVE:
                    DoSave();
                    return 0;
                case ID_TPL_SAVEAS:
                    DoSaveAs();
                    return 0;
                case ID_TPL_DELETE:
                    DoDelete();
                    return 0;
                case ID_TPL_EXIT:
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;
        case WM_SIZE:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            EditView_Layout(0, 0, client.right, client.bottom);
            return 0;
        }
        case WM_CLOSE:
            Template_SaveCurrent();
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            EditView_Destroy();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show)
{
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
    {
        SetProcessDPIAware();
    }

    g_instance = instance;
    if (!App_InitializeRequested(instance, APP_MODE_FULL))
    {
        MessageBoxA(nullptr, "Could not start the template maker.", "Template Maker", MB_OK | MB_ICONERROR);
        return 0;
    }

    if (!Store_Open())
    {
        MessageBoxA(nullptr, "Could not open the program database.", "Template Maker", MB_OK | MB_ICONERROR);
        return 0;
    }

    Template_SetActive(true);
    Template_SetChangedHook(UpdateTitle);
    Document_EnsureFolders();
    Template_RefreshCatalog();

    WNDCLASSA maker = {};
    maker.lpfnWndProc = MakerProc;
    maker.hInstance = instance;
    maker.lpszClassName = "TemplateMaker";
    maker.hbrBackground = CreateSolidBrush(RGB(0x3F, 0x2C, 0x43));
    maker.hCursor = LoadCursor(nullptr, IDC_ARROW);
    maker.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    maker.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&maker);

    WNDCLASSA prompt = {};
    prompt.lpfnWndProc = PromptProc;
    prompt.hInstance = instance;
    prompt.lpszClassName = "TemplatePrompt";
    prompt.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    prompt.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&prompt);

    WNDCLASSA openPrompt = {};
    openPrompt.lpfnWndProc = OpenPromptProc;
    openPrompt.hInstance = instance;
    openPrompt.lpszClassName = "TemplateOpenPrompt";
    openPrompt.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    openPrompt.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&openPrompt);
    EditView_Register(instance);

    ACCEL accels[] = {
        { FVIRTKEY | FCONTROL, 'N', ID_TPL_NEW },
        { FVIRTKEY | FCONTROL, 'O', ID_TPL_OPEN },
        { FVIRTKEY | FCONTROL, 'S', ID_TPL_SAVE }
    };
    HACCEL accel = CreateAcceleratorTableA(accels, 3);

    g_window = CreateWindowExA(
        WS_EX_APPWINDOW,
        "TemplateMaker",
        "Template Maker",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        820,
        nullptr,
        CreateMakerMenu(),
        instance,
        nullptr
    );
    if (g_window == nullptr)
    {
        return 0;
    }

    EditView_Create(g_window, instance);
    EditView_Show(true);

    char lastId[64] = {};
    Store_GetString("last.template", lastId, sizeof(lastId), "");
    if (lastId[0] == '\0' || !Template_Open(lastId))
    {
        if (!Template_New("Untitled"))
        {
            MessageBoxA(g_window, "Could not create a starting layout.", "Template Maker", MB_OK | MB_ICONWARNING);
        }
    }
    EditView_OpenSlide(0);
    UpdateTitle();

    ShowWindow(g_window, show);
    UpdateWindow(g_window);

    MSG msg = {};
    while (GetMessageA(&msg, nullptr, 0, 0) > 0)
    {
        if (accel != nullptr && TranslateAcceleratorA(g_window, accel, &msg))
        {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (accel != nullptr)
    {
        DestroyAcceleratorTable(accel);
    }
    Template_SetActive(false);
    Store_Close();
    Renderer_Shutdown();
    return 0;
}
