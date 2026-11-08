#include "manage_screens.h"
#include "renderer.h"
#include "init.h"
#include "gpu_client.h"
#include "app.h"
#include "store.h"

#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")

#define MAX_MANAGED_SCREENS BOOT_MAX_SCREENS
#define MAX_SCREEN_SCENES 8
#define ID_COUNT_EDIT 1
#define ID_SCENE_ADD 20
#define ID_LAYOUT_MODE 21
#define ID_MONITOR_COMBO_BASE 100
#define ID_CHOOSER_UNASSIGN 1
#define ID_CHOOSER_MONITOR_BASE 10
#define ID_CHOOSER_VIRTUAL_BASE 100
#define ID_CHOOSER_NEW_MODE_BASE 200
#define VIRTUAL_MODE_COUNT 9
#define CHOOSER_H 32
#define TILE_GAP 12
#define MONITOR_RADIUS 6
#define CHOOSER_RADIUS 6
#define CARD_RADIUS 10
#define CARD_PAD 10
#define LAYOUT_COMBO_W 168
#define LAYOUT_COMBO_H 28
#define MAP_HEADER_H 96
#define MAP_FOOTER_H 62
#define MAP_LEGEND_H 26
#define TILE_TOP MAP_HEADER_H
#define SCENE_RAIL_W 220
#define SCENE_ROW_H 40
#define SCENE_HEADER_H 48
#define SCENE_BTN 26
#define CHROME_PAD 8
#define COLOR_BASE RGB(0x26, 0x26, 0x26)
#define COLOR_PANEL RGB(0x2E, 0x2E, 0x2E)
#define COLOR_RAIL RGB(0x2E, 0x2E, 0x2E)
#define COLOR_RAISED RGB(0x38, 0x38, 0x38)
#define COLOR_RAISED_HI RGB(0x42, 0x42, 0x42)
#define COLOR_SUNKEN RGB(0x1E, 0x1E, 0x1E)
#define COLOR_MONITOR RGB(0x15, 0x15, 0x15)
#define COLOR_CHOOSER RGB(0x38, 0x38, 0x38)
#define COLOR_EDGE RGB(0x45, 0x45, 0x45)
#define COLOR_INK RGB(0xD0, 0xD0, 0xD0)
#define COLOR_INK_BRIGHT RGB(0xF2, 0xF2, 0xF2)
#define COLOR_MUTED RGB(0x8C, 0x8C, 0x8C)
#define COLOR_FAINT RGB(0x5E, 0x5E, 0x5E)
#define COLOR_SELECT RGB(0x38, 0x38, 0x38)
#define COLOR_GREEN RGB(0x7F, 0xD0, 0x7F)
#define COLOR_ACCENT RGB(0xE8, 0xE8, 0xE8)

struct VirtualMode
{
    const char* label;
    int width;
    int height;
    int refreshHz;
};

#define VIRTUAL_DEVICE_PREFIX "virtual:"
#define MAX_VIRTUAL_DISPLAYS STORE_VIRTUAL_MAX

static const VirtualMode kVirtualModes[VIRTUAL_MODE_COUNT] = {
    { "720p24", 1280, 720, 24 },
    { "720p30", 1280, 720, 30 },
    { "720p50", 1280, 720, 50 },
    { "720p60", 1280, 720, 60 },
    { "1080p30", 1920, 1080, 30 },
    { "1080p60", 1920, 1080, 60 },
    { "2160p25", 3840, 2160, 25 },
    { "2160p30", 3840, 2160, 30 },
    { "2160p60", 3840, 2160, 60 },
};

static bool IsVirtualDevice(const char* device)
{
    return device != nullptr && strncmp(device, VIRTUAL_DEVICE_PREFIX, 8) == 0;
}

static void VirtualModeToken(const char* device, char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    out[0] = '\0';
    if (!IsVirtualDevice(device))
    {
        return;
    }
    const char* label = device + 8;
    const char* hash = strchr(label, '#');
    int n = hash != nullptr ? (int)(hash - label) : lstrlenA(label);
    if (n >= outChars)
    {
        n = outChars - 1;
    }
    if (n < 0)
    {
        n = 0;
    }
    lstrcpynA(out, label, n + 1);
}

static int VirtualIdFromDevice(const char* device)
{
    if (!IsVirtualDevice(device))
    {
        return 0;
    }
    const char* hash = strchr(device, '#');
    if (hash == nullptr || hash[1] == '\0')
    {
        return 0;
    }
    return atoi(hash + 1);
}

static const VirtualMode* FindVirtualMode(const char* device)
{
    char token[16] = {};
    VirtualModeToken(device, token, sizeof(token));
    if (token[0] == '\0')
    {
        return nullptr;
    }
    for (int i = 0; i < VIRTUAL_MODE_COUNT; ++i)
    {
        if (lstrcmpiA(kVirtualModes[i].label, token) == 0)
        {
            return &kVirtualModes[i];
        }
    }
    return nullptr;
}

static void FormatVirtualDevice(char* out, int outChars, const VirtualMode* mode, int id)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    const char* label = mode != nullptr ? mode->label : "1080p60";
    if (id > 0)
    {
        wsprintfA(out, "virtual:%s#%d", label, id);
    }
    else
    {
        wsprintfA(out, "virtual:%s", label);
    }
    out[outChars - 1] = '\0';
}

static bool IsViewerHwnd(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return false;
    }
    char className[32] = {};
    GetClassNameA(hwnd, className, sizeof(className));
    return lstrcmpiA(className, "BlankScreenWindow") == 0;
}

struct DisplayInfo
{
    RECT rect;
    bool primary;
    char label[128];
    char title[160];
    char device[32];
};

struct OutputSlot
{
    HWND hwnd;
    HWND combo;
    int monitorIndex;
    char device[32];
    RECT tile;
    RECT preview;
    RECT chooserRc;
};

struct ScreenScene
{
    char name[64];
    int count;
    int layoutMode;
    int canvasW;
    int canvasH;
    int refreshHz;
    char devices[MAX_MANAGED_SCREENS][32];
    HWND viewers[MAX_MANAGED_SCREENS];
};

struct MonitorClaim
{
    char device[32];
    HWND hwnd;
    int monitorIndex;
};

struct VirtualGlass
{
    int id;
    char mode[16];
    char device[32];
    HWND hwnd;
};

static HWND g_owner = nullptr;
static HINSTANCE g_instance = nullptr;
static HWND g_manageWindow = nullptr;
static HWND g_hostPanel = nullptr;
static HWND g_sceneRail = nullptr;
static HWND g_outputMap = nullptr;
static HWND g_countEdit = nullptr;
static HWND g_countUpDown = nullptr;
static HWND g_countLabel = nullptr;
static HWND g_detectedLabel = nullptr;
static HWND g_layoutCombo = nullptr;
static HFONT g_uiFont = nullptr;
static HFONT g_titleFont = nullptr;
static HFONT g_smallFont = nullptr;
static RECT g_segOuter = {};
static RECT g_segRc[3] = {};
static RECT g_stepBox = {};
static RECT g_stepMinus = {};
static RECT g_stepPlus = {};
static OutputSlot g_outputs[MAX_MANAGED_SCREENS] = {};
static int g_outputCount = 0;
static DisplayInfo g_displays[MAX_MANAGED_SCREENS] = {};
static int g_displayCount = 0;
static ScreenScene g_scenes[MAX_SCREEN_SCENES] = {};
static int g_sceneCount = 0;
static int g_activeScene = 0;
static bool g_applying = false;
static bool g_fillingCombo = false;
static bool g_classReady = false;
static bool g_pendingReady = false;
static bool g_restored = false;
static int g_pendingCount = 0;
static char g_pendingDevices[MAX_MANAGED_SCREENS][64] = {};
static MonitorClaim g_claims[MAX_MANAGED_SCREENS] = {};
static int g_claimCount = 0;
static bool g_outputsEnabled = true;
static bool g_outputsEnabledLoaded = false;
static VirtualGlass g_virtuals[MAX_VIRTUAL_DISPLAYS] = {};
static int g_virtualCount = 0;
static void SaveVirtualsOnly();
static void EnsureVirtualPool();
static VirtualGlass* FindVirtualById(int id);
static void BindVirtualHwnd(int id, HWND hwnd);

static void ApplyScreenCount(int count, bool persist);
static void SetCountText(int count);
static void LayoutOutputTiles();
static HWND CreateFullscreenOnMonitor(const RECT* monitor);
static void AdoptOwner(HWND owner);
static void PlaceOutputOnMonitor(int slot);
static void ClearOutput(int slot);
static void EnsureClaimsForAllScenes();
static void SaveScreens();
static void RebuildSlotCombos();
static void FillSlotCombo(int slot);
static void LayoutSlotCombos();
static int FindMonitorByDevice(const char* device);
static void EnsureUiFont();
static void EnsureClasses(HINSTANCE instance);
static void BuildChrome(HWND parent, HINSTANCE instance);
static void LayoutChrome(int width, int height);
static void CaptureActiveScene();
static void SaveScenes();
static void ApplyScene(int index, bool placeOutputs);
static void EnsureDefaultScene();
static void AddScreenScene();
static void RemoveActiveScreenScene();
static void LoadScenesFromStore();
static void DrawRoundFill(HDC hdc, const RECT* rc, int radius, COLORREF fill, COLORREF border);
static void GetRailHeaderButtons(const RECT* client, RECT* plusRc, RECT* minusRc);
static LRESULT CALLBACK SceneRailProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static int ClampLayoutMode(int mode);
static void EnsureLayoutCombo();
static void LayoutLayoutCombo();
static void SyncLayoutCombo();
static void OnLayoutComboChanged();
static void SetLayoutMode(int mode);
static void NudgeScreenCount(int delta);
static void LayoutMapChrome(const RECT* client);
static int ActiveSceneMaxSlots();

static void EnsureUiFont()
{
    if (g_uiFont != nullptr)
    {
        return;
    }
    g_uiFont = CreateFontA(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
    );
    g_titleFont = CreateFontA(
        -22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
    );
    g_smallFont = CreateFontA(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
    );
}

static const char* LayoutChipText(int mode)
{
    switch (ClampLayoutMode(mode))
    {
        case SCREEN_LAYOUT_SPAN: return "Span";
        case SCREEN_LAYOUT_MIRROR: return "Mirror";
        default: return "Single";
    }
}

static void ApplyDarkCaption(HWND hwnd)
{
    if (hwnd == nullptr)
    {
        return;
    }
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
    {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
    COLORREF caption = COLOR_BASE;
    DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
    COLORREF text = COLOR_INK;
    DwmSetWindowAttribute(hwnd, 36, &text, sizeof(text));
}

static void RoundChild(HWND hwnd, int radius)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return;
    }
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    if (rc.right < 4 || rc.bottom < 4)
    {
        return;
    }
    HRGN region = CreateRoundRectRgn(0, 0, rc.right + 1, rc.bottom + 1, radius * 2, radius * 2);
    SetWindowRgn(hwnd, region, TRUE);
}

static BOOL CALLBACK CollectDisplays(HMONITOR monitor, HDC, LPRECT, LPARAM)
{
    if (g_displayCount >= MAX_MANAGED_SCREENS)
    {
        return FALSE;
    }

    MONITORINFOEXA info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoA(monitor, reinterpret_cast<MONITORINFO*>(&info)))
    {
        return TRUE;
    }

    DisplayInfo* display = &g_displays[g_displayCount];
    display->rect = info.rcMonitor;
    display->primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    lstrcpynA(display->device, info.szDevice, 32);

    DISPLAY_DEVICEA device = {};
    device.cb = sizeof(device);
    const char* name = info.szDevice;
    if (EnumDisplayDevicesA(info.szDevice, 0, &device, 0) && device.DeviceString[0] != '\0')
    {
        name = device.DeviceString;
    }
    lstrcpynA(display->label, name, sizeof(display->label));

    const int width = info.rcMonitor.right - info.rcMonitor.left;
    const int height = info.rcMonitor.bottom - info.rcMonitor.top;
    if (display->primary)
    {
        wsprintfA(display->title, "%s  %dx%d  (Primary)", name, width, height);
    }
    else
    {
        wsprintfA(display->title, "%s  %dx%d", name, width, height);
    }

    g_displayCount += 1;
    return TRUE;
}

static void RefreshDisplays()
{
    g_displayCount = 0;
    EnumDisplayMonitors(nullptr, nullptr, CollectDisplays, 0);

    if (g_displayCount == 0)
    {
        g_displays[0].rect.left = 0;
        g_displays[0].rect.top = 0;
        g_displays[0].rect.right = GetSystemMetrics(SM_CXSCREEN);
        g_displays[0].rect.bottom = GetSystemMetrics(SM_CYSCREEN);
        g_displays[0].primary = true;
        lstrcpyA(g_displays[0].title, "Primary display");
        lstrcpyA(g_displays[0].label, "Primary display");
        lstrcpyA(g_displays[0].device, "DISPLAY1");
        g_displayCount = 1;
    }
}

static bool IsFullscreenHwnd(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return false;
    }
    char className[40] = {};
    GetClassNameA(hwnd, className, sizeof(className));
    return lstrcmpiA(className, "ManagedFullscreenWindow") == 0;
}

static int FindClaimIndex(const char* device)
{
    if (device == nullptr || device[0] == '\0' || IsVirtualDevice(device))
    {
        return -1;
    }
    for (int i = 0; i < g_claimCount; ++i)
    {
        if (lstrcmpiA(g_claims[i].device, device) == 0)
        {
            return i;
        }
    }
    return -1;
}

static HWND ClaimHwnd(const char* device)
{
    const int index = FindClaimIndex(device);
    if (index < 0)
    {
        return nullptr;
    }
    return g_claims[index].hwnd;
}

static void ForgetClaimHwnd(HWND hwnd)
{
    if (hwnd == nullptr)
    {
        return;
    }
    for (int i = 0; i < g_claimCount; ++i)
    {
        if (g_claims[i].hwnd == hwnd)
        {
            g_claims[i].hwnd = nullptr;
        }
    }
    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        if (g_outputs[i].hwnd == hwnd)
        {
            g_outputs[i].hwnd = nullptr;
        }
    }
}

static void RemoveClaimAt(int index)
{
    if (index < 0 || index >= g_claimCount)
    {
        return;
    }
    for (int i = index; i + 1 < g_claimCount; ++i)
    {
        g_claims[i] = g_claims[i + 1];
    }
    ZeroMemory(&g_claims[g_claimCount - 1], sizeof(g_claims[0]));
    g_claimCount -= 1;
}

static bool DeviceAssignedOnAnyScene(const char* device)
{
    if (device == nullptr || device[0] == '\0' || IsVirtualDevice(device))
    {
        return false;
    }
    for (int s = 0; s < g_sceneCount; ++s)
    {
        const int slots = g_scenes[s].count < MAX_MANAGED_SCREENS ? g_scenes[s].count : MAX_MANAGED_SCREENS;
        for (int i = 0; i < slots; ++i)
        {
            if (lstrcmpiA(g_scenes[s].devices[i], device) == 0)
            {
                return true;
            }
        }
    }
    for (int i = 0; i < g_outputCount; ++i)
    {
        if (lstrcmpiA(g_outputs[i].device, device) == 0)
        {
            return true;
        }
    }
    return false;
}

static void ReleasePhysicalDevice(const char* device)
{
    const int index = FindClaimIndex(device);
    if (index < 0)
    {
        return;
    }
    HWND hwnd = g_claims[index].hwnd;
    g_claims[index].hwnd = nullptr;
    if (hwnd != nullptr && IsWindow(hwnd))
    {
        DestroyWindow(hwnd);
    }
    RemoveClaimAt(index);
}

static void LoadOutputsEnabled()
{
    if (g_outputsEnabledLoaded || !Store_Ready())
    {
        return;
    }
    g_outputsEnabled = Store_GetInt("outputs.enabled", 1) != 0;
    g_outputsEnabledLoaded = true;
}

static MonitorClaim* GetOrCreateClaim(const char* device, int monitorIndex)
{
    if (device == nullptr || device[0] == '\0' || IsVirtualDevice(device))
    {
        return nullptr;
    }
    int index = FindClaimIndex(device);
    if (index >= 0)
    {
        g_claims[index].monitorIndex = monitorIndex;
        return &g_claims[index];
    }
    if (g_claimCount >= MAX_MANAGED_SCREENS || monitorIndex < 0 || monitorIndex >= g_displayCount)
    {
        return nullptr;
    }
    MonitorClaim* claim = &g_claims[g_claimCount];
    ZeroMemory(claim, sizeof(*claim));
    lstrcpynA(claim->device, device, 32);
    claim->monitorIndex = monitorIndex;
    claim->hwnd = CreateFullscreenOnMonitor(&g_displays[monitorIndex].rect);
    if (claim->hwnd == nullptr)
    {
        return nullptr;
    }
    LoadOutputsEnabled();
    if (g_outputsEnabled)
    {
        ShowWindow(claim->hwnd, SW_SHOW);
        SetWindowPos(claim->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
    g_claimCount += 1;
    return claim;
}

static void EnsureClaimsForAllScenes()
{
    RefreshDisplays();
    for (int s = 0; s < g_sceneCount; ++s)
    {
        const int slots = g_scenes[s].count < MAX_MANAGED_SCREENS ? g_scenes[s].count : MAX_MANAGED_SCREENS;
        for (int i = 0; i < slots; ++i)
        {
            const char* device = g_scenes[s].devices[i];
            const int monitor = FindMonitorByDevice(device);
            if (monitor >= 0)
            {
                GetOrCreateClaim(device, monitor);
            }
        }
    }
}

static int FindMonitorByDevice(const char* device)
{
    if (device == nullptr || device[0] == '\0' || IsVirtualDevice(device))
    {
        return -1;
    }
    for (int i = 0; i < g_displayCount; ++i)
    {
        if (lstrcmpiA(g_displays[i].device, device) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int ClampLayoutMode(int mode)
{
    if (mode < SCREEN_LAYOUT_SINGLE || mode > SCREEN_LAYOUT_MIRROR)
    {
        return SCREEN_LAYOUT_SINGLE;
    }
    return mode;
}

static void EnsureLayoutCombo()
{
    if (g_layoutCombo != nullptr && IsWindow(g_layoutCombo))
    {
        ShowWindow(g_layoutCombo, SW_HIDE);
    }
}

static void LayoutLayoutCombo()
{
    if (g_layoutCombo != nullptr && IsWindow(g_layoutCombo))
    {
        ShowWindow(g_layoutCombo, SW_HIDE);
    }
}

static void SyncLayoutCombo()
{
    EnsureDefaultScene();
    EnsureLayoutCombo();
    if (g_layoutCombo == nullptr)
    {
        return;
    }

    const int mode = ClampLayoutMode(g_scenes[g_activeScene].layoutMode);
    g_fillingCombo = true;
    SendMessage(g_layoutCombo, CB_SETCURSEL, mode, 0);
    g_fillingCombo = false;
}

static void SetLayoutMode(int mode)
{
    if (g_fillingCombo)
    {
        return;
    }

    EnsureDefaultScene();
    g_scenes[g_activeScene].layoutMode = ClampLayoutMode(mode);
    SaveScenes();
    if (g_outputCount > ActiveSceneMaxSlots())
    {
        ApplyScreenCount(ActiveSceneMaxSlots(), true);
    }
    if (g_outputMap != nullptr)
    {
        InvalidateRect(g_outputMap, nullptr, FALSE);
    }
    ManageScreens_RefreshUi();
    App_RebuildScreenMenus();
    RefreshProgramScreenMirror();
    PresentCurrent();
}

bool ManageScreens_SetScreenLayout(int index, int layout)
{
    EnsureDefaultScene();
    if (index < 0 || index >= g_sceneCount)
    {
        return false;
    }
    if (index != g_activeScene)
    {
        ManageScreens_ActivateScreen(index);
    }
    SetLayoutMode(layout);
    return true;
}

static void OnLayoutComboChanged()
{
    if (g_fillingCombo || g_layoutCombo == nullptr)
    {
        return;
    }

    const int sel = static_cast<int>(SendMessage(g_layoutCombo, CB_GETCURSEL, 0, 0));
    SetLayoutMode(sel);
}

static void CaptureActiveScene()
{
    EnsureDefaultScene();
    if (g_activeScene < 0 || g_activeScene >= g_sceneCount)
    {
        g_activeScene = 0;
    }

    ScreenScene* scene = &g_scenes[g_activeScene];
    scene->count = g_outputCount;
    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        if (i < g_outputCount)
        {
            lstrcpynA(scene->devices[i], g_outputs[i].device, 32);
            if (IsViewerHwnd(g_outputs[i].hwnd))
            {
                scene->viewers[i] = g_outputs[i].hwnd;
            }
            else if (!IsVirtualDevice(g_outputs[i].device))
            {
                scene->viewers[i] = nullptr;
            }
        }
        else
        {
            scene->devices[i][0] = '\0';
        }
    }
}

static void SaveScenes()
{
    if (!Store_Ready())
    {
        return;
    }

    CaptureActiveScene();

    StoreScreen stored[STORE_SCREEN_MAX] = {};
    const int count = g_sceneCount < STORE_SCREEN_MAX ? g_sceneCount : STORE_SCREEN_MAX;
    for (int s = 0; s < count; ++s)
    {
        lstrcpynA(stored[s].name, g_scenes[s].name, sizeof(stored[s].name));
        stored[s].layoutMode = ClampLayoutMode(g_scenes[s].layoutMode);
        stored[s].canvasW = g_scenes[s].canvasW > 0 ? g_scenes[s].canvasW : 1920;
        stored[s].canvasH = g_scenes[s].canvasH > 0 ? g_scenes[s].canvasH : 1080;
        stored[s].refreshHz = g_scenes[s].refreshHz > 0 ? g_scenes[s].refreshHz : 60;
        stored[s].outputCount = g_scenes[s].count;
        if (stored[s].outputCount < 0)
        {
            stored[s].outputCount = 0;
        }
        if (stored[s].outputCount > STORE_SCREEN_OUTPUT_MAX)
        {
            stored[s].outputCount = STORE_SCREEN_OUTPUT_MAX;
        }
        for (int i = 0; i < stored[s].outputCount; ++i)
        {
            lstrcpynA(stored[s].devices[i], g_scenes[s].devices[i], 32);
        }
    }
    Store_SaveScreens(stored, count, g_activeScene);

    SaveVirtualsOnly();

    Store_SetInt("screens.count", g_outputCount);
    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        char key[32] = {};
        wsprintfA(key, "screens.%d.device", i);
        Store_SetString(key, i < g_outputCount ? g_outputs[i].device : "");
    }
    RefreshScreensPanel();
    App_RebuildScreenMenus();
}

static void SaveScreens()
{
    SaveScenes();
}

static void EnsureDefaultScene()
{
    if (g_sceneCount > 0)
    {
        return;
    }

    ZeroMemory(&g_scenes[0], sizeof(g_scenes[0]));
    lstrcpyA(g_scenes[0].name, "Screen 1");
    g_scenes[0].count = 0;
    g_scenes[0].canvasW = 1920;
    g_scenes[0].canvasH = 1080;
    g_scenes[0].refreshHz = 60;
    g_sceneCount = 1;
    g_activeScene = 0;
}

static void ApplyScene(int index, bool placeOutputs)
{
    EnsureDefaultScene();
    if (index < 0 || index >= g_sceneCount)
    {
        return;
    }

    g_activeScene = index;
    const ScreenScene* scene = &g_scenes[index];
    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        if (IsFullscreenHwnd(g_outputs[i].hwnd))
        {
            g_outputs[i].hwnd = nullptr;
        }
        else if (IsViewerHwnd(g_outputs[i].hwnd))
        {
            g_outputs[i].hwnd = nullptr;
        }
    }
    ApplyScreenCount(scene->count, false);
    for (int i = 0; i < g_outputCount; ++i)
    {
        lstrcpynA(g_outputs[i].device, scene->devices[i], 32);
        if (IsVirtualDevice(g_outputs[i].device))
        {
            g_outputs[i].monitorIndex = -1;
            const int id = VirtualIdFromDevice(g_outputs[i].device);
            VirtualGlass* glass = FindVirtualById(id);
            g_outputs[i].hwnd = glass != nullptr ? glass->hwnd : scene->viewers[i];
            if (placeOutputs && id > 0 && (g_outputs[i].hwnd == nullptr || !IsWindow(g_outputs[i].hwnd)))
            {
                App_OpenVirtualDisplay(id);
                glass = FindVirtualById(id);
                g_outputs[i].hwnd = glass != nullptr ? glass->hwnd : g_outputs[i].hwnd;
            }
        }
        else
        {
            g_outputs[i].monitorIndex = FindMonitorByDevice(g_outputs[i].device);
            if (g_outputs[i].monitorIndex >= 0)
            {
                MonitorClaim* claim = GetOrCreateClaim(g_outputs[i].device, g_outputs[i].monitorIndex);
                g_outputs[i].hwnd = claim != nullptr ? claim->hwnd : nullptr;
            }
            else
            {
                g_outputs[i].hwnd = ClaimHwnd(g_outputs[i].device);
            }
        }
    }
    RebuildSlotCombos();
    SetCountText(g_outputCount);
    SyncLayoutCombo();
    if (g_sceneRail != nullptr)
    {
        InvalidateRect(g_sceneRail, nullptr, FALSE);
    }
    if (g_outputMap != nullptr)
    {
        InvalidateRect(g_outputMap, nullptr, FALSE);
    }
    RefreshProgramScreenMirror();
    if (!App_LiveHold())
    {
        PresentCurrent();
    }
}

static void ApplyDefaultCanvas(ScreenScene* scene)
{
    if (scene == nullptr)
    {
        return;
    }
    if (scene->canvasW <= 0)
    {
        scene->canvasW = 1920;
    }
    if (scene->canvasH <= 0)
    {
        scene->canvasH = 1080;
    }
    if (scene->refreshHz <= 0)
    {
        scene->refreshHz = 60;
    }
}

static int CreateScreenScene(int canvasW, int canvasH, int refreshHz)
{
    EnsureDefaultScene();
    if (g_sceneCount >= MAX_SCREEN_SCENES)
    {
        return -1;
    }

    CaptureActiveScene();
    ScreenScene* scene = &g_scenes[g_sceneCount];
    ZeroMemory(scene, sizeof(*scene));
    wsprintfA(scene->name, "Screen %d", g_sceneCount + 1);
    scene->count = 0;
    scene->canvasW = canvasW > 0 ? canvasW : 1920;
    scene->canvasH = canvasH > 0 ? canvasH : 1080;
    scene->refreshHz = refreshHz > 0 ? refreshHz : 60;
    g_sceneCount += 1;
    ApplyScene(g_sceneCount - 1, true);
    SaveScenes();
    return g_sceneCount - 1;
}

static void AddScreenScene()
{
    CreateScreenScene(1920, 1080, 60);
}

static void RemoveActiveScreenScene()
{
    EnsureDefaultScene();
    if (g_sceneCount <= 1)
    {
        MessageBoxA(
            g_manageWindow != nullptr ? g_manageWindow : g_owner,
            "You need at least one screen in the list.",
            "Screens",
            MB_OK | MB_ICONINFORMATION
        );
        return;
    }

    const int result = MessageBoxA(
        g_manageWindow != nullptr ? g_manageWindow : g_owner,
        "Remove the selected screen from this list?",
        "Screens",
        MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2
    );
    if (result != IDOK)
    {
        return;
    }

    const int remove = g_activeScene;
    for (int i = remove; i + 1 < g_sceneCount; ++i)
    {
        g_scenes[i] = g_scenes[i + 1];
    }
    ZeroMemory(&g_scenes[g_sceneCount - 1], sizeof(g_scenes[0]));
    g_sceneCount -= 1;
    if (g_activeScene >= g_sceneCount)
    {
        g_activeScene = g_sceneCount - 1;
    }
    ApplyScene(g_activeScene, false);
    for (int i = g_claimCount - 1; i >= 0; --i)
    {
        bool used = false;
        for (int s = 0; s < g_sceneCount && !used; ++s)
        {
            for (int slot = 0; slot < MAX_MANAGED_SCREENS; ++slot)
            {
                if (lstrcmpiA(g_scenes[s].devices[slot], g_claims[i].device) == 0)
                {
                    used = true;
                    break;
                }
            }
        }
        if (!used)
        {
            ReleasePhysicalDevice(g_claims[i].device);
        }
    }
    SaveScenes();
}

static void GetRailHeaderButtons(const RECT* client, RECT* plusRc, RECT* minusRc)
{
    const int top = (SCENE_HEADER_H - SCENE_BTN) / 2;
    minusRc->right = client->right - 10;
    minusRc->left = minusRc->right - SCENE_BTN;
    minusRc->top = top;
    minusRc->bottom = top + SCENE_BTN;
    plusRc->right = minusRc->left - 6;
    plusRc->left = plusRc->right - SCENE_BTN;
    plusRc->top = top;
    plusRc->bottom = top + SCENE_BTN;
}

static void LoadScenesFromStore()
{
    ZeroMemory(g_scenes, sizeof(g_scenes));
    g_sceneCount = 0;
    g_activeScene = 0;
    if (!Store_Ready())
    {
        EnsureDefaultScene();
        return;
    }

    StoreScreen stored[STORE_SCREEN_MAX] = {};
    int active = 0;
    g_sceneCount = Store_LoadScreens(stored, STORE_SCREEN_MAX, &active);
    if (g_sceneCount > MAX_SCREEN_SCENES)
    {
        g_sceneCount = MAX_SCREEN_SCENES;
    }

    if (g_sceneCount > 0)
    {
        for (int s = 0; s < g_sceneCount; ++s)
        {
            lstrcpynA(g_scenes[s].name, stored[s].name, sizeof(g_scenes[s].name));
            if (g_scenes[s].name[0] == '\0')
            {
                wsprintfA(g_scenes[s].name, "Screen %d", s + 1);
            }
            g_scenes[s].layoutMode = ClampLayoutMode(stored[s].layoutMode);
            g_scenes[s].count = stored[s].outputCount;
            if (g_scenes[s].count < 0)
            {
                g_scenes[s].count = 0;
            }
            if (g_scenes[s].count > MAX_MANAGED_SCREENS)
            {
                g_scenes[s].count = MAX_MANAGED_SCREENS;
            }
            for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
            {
                lstrcpynA(g_scenes[s].devices[i], stored[s].devices[i], 32);
            }
            g_scenes[s].canvasW = stored[s].canvasW;
            g_scenes[s].canvasH = stored[s].canvasH;
            g_scenes[s].refreshHz = stored[s].refreshHz;
            ApplyDefaultCanvas(&g_scenes[s]);
        }
        g_activeScene = (active >= 0 && active < g_sceneCount) ? active : 0;
        EnsureVirtualPool();
        return;
    }

    g_sceneCount = Store_GetInt("scenes.count", 0);
    if (g_sceneCount < 0)
    {
        g_sceneCount = 0;
    }
    if (g_sceneCount > MAX_SCREEN_SCENES)
    {
        g_sceneCount = MAX_SCREEN_SCENES;
    }

    if (g_sceneCount == 0)
    {
        EnsureDefaultScene();
        g_scenes[0].count = Store_GetInt("screens.count", 0);
        if (g_scenes[0].count < 0)
        {
            g_scenes[0].count = 0;
        }
        if (g_scenes[0].count > MAX_MANAGED_SCREENS)
        {
            g_scenes[0].count = MAX_MANAGED_SCREENS;
        }
        for (int i = 0; i < g_scenes[0].count; ++i)
        {
            char key[32] = {};
            wsprintfA(key, "screens.%d.device", i);
            Store_GetString(key, g_scenes[0].devices[i], 32, "");
        }
        ApplyDefaultCanvas(&g_scenes[0]);
        g_activeScene = 0;
        EnsureVirtualPool();
        SaveScenes();
        return;
    }

    for (int s = 0; s < g_sceneCount; ++s)
    {
        char key[48] = {};
        char fallback[32] = {};
        wsprintfA(fallback, "Screen %d", s + 1);
        wsprintfA(key, "scenes.%d.name", s);
        Store_GetString(key, g_scenes[s].name, 64, fallback);
        if (g_scenes[s].name[0] == '\0')
        {
            lstrcpynA(g_scenes[s].name, fallback, 64);
        }
        wsprintfA(key, "scenes.%d.count", s);
        g_scenes[s].count = Store_GetInt(key, 0);
        if (g_scenes[s].count < 0)
        {
            g_scenes[s].count = 0;
        }
        if (g_scenes[s].count > MAX_MANAGED_SCREENS)
        {
            g_scenes[s].count = MAX_MANAGED_SCREENS;
        }
        wsprintfA(key, "scenes.%d.mode", s);
        g_scenes[s].layoutMode = ClampLayoutMode(Store_GetInt(key, SCREEN_LAYOUT_SINGLE));
        for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
        {
            wsprintfA(key, "scenes.%d.%d.device", s, i);
            Store_GetString(key, g_scenes[s].devices[i], 32, "");
        }
        ApplyDefaultCanvas(&g_scenes[s]);
    }

    g_activeScene = Store_GetInt("scenes.active", Store_GetInt("screens.active", 0));
    if (g_activeScene < 0 || g_activeScene >= g_sceneCount)
    {
        g_activeScene = 0;
    }
    EnsureVirtualPool();
    SaveScenes();
}

static LRESULT CALLBACK SceneRailProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_LBUTTONUP:
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            RECT client = {};
            GetClientRect(hwnd, &client);
            RECT plusRc = {};
            RECT minusRc = {};
            GetRailHeaderButtons(&client, &plusRc, &minusRc);
            POINT pt = { x, y };
            if (PtInRect(&plusRc, pt))
            {
                AddScreenScene();
                return 0;
            }
            if (PtInRect(&minusRc, pt))
            {
                RemoveActiveScreenScene();
                return 0;
            }

            const int index = (y - SCENE_HEADER_H) / SCENE_ROW_H;
            if (index >= 0 && index < g_sceneCount && index != g_activeScene)
            {
                CaptureActiveScene();
                ApplyScene(index, false);
                SaveScenes();
            }
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);

            HBRUSH fill = CreateSolidBrush(COLOR_RAIL);
            FillRect(hdc, &client, fill);
            DeleteObject(fill);

            EnsureUiFont();
            HFONT oldFont = (HFONT)SelectObject(
                hdc,
                g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT)
            );
            SetBkMode(hdc, TRANSPARENT);

            RECT plusRc = {};
            RECT minusRc = {};
            GetRailHeaderButtons(&client, &plusRc, &minusRc);

            RECT header = { 14, 0, plusRc.left - 8, SCENE_HEADER_H };
            SetTextColor(hdc, COLOR_MUTED);
            HFONT headerFont = g_smallFont != nullptr ? g_smallFont : g_uiFont;
            SelectObject(hdc, headerFont);
            DrawTextA(hdc, "SCREENS", -1, &header, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            DrawRoundFill(hdc, &plusRc, 6, COLOR_RAISED, COLOR_EDGE);
            DrawRoundFill(hdc, &minusRc, 6, COLOR_RAISED, COLOR_EDGE);
            SetTextColor(hdc, COLOR_INK_BRIGHT);
            SelectObject(hdc, g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT));
            DrawTextA(hdc, "+", -1, &plusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawTextA(hdc, "-", -1, &minusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            for (int i = 0; i < g_sceneCount; ++i)
            {
                RECT row = {
                    6,
                    SCENE_HEADER_H + (i * SCENE_ROW_H),
                    client.right - 6,
                    SCENE_HEADER_H + (i * SCENE_ROW_H) + SCENE_ROW_H - 4
                };
                if (row.bottom > client.bottom)
                {
                    break;
                }

                const bool on = i == g_activeScene;
                if (on)
                {
                    DrawRoundFill(hdc, &row, 7, COLOR_RAISED, COLOR_RAISED);
                }

                const char* chip = LayoutChipText(g_scenes[i].layoutMode);
                SelectObject(hdc, g_smallFont != nullptr ? g_smallFont : g_uiFont);
                SIZE chipSize = {};
                GetTextExtentPoint32A(hdc, chip, lstrlenA(chip), &chipSize);
                RECT chipRc = row;
                chipRc.right -= 8;
                chipRc.left = chipRc.right - chipSize.cx - 14;
                chipRc.top = row.top + ((row.bottom - row.top) - (chipSize.cy + 4)) / 2;
                chipRc.bottom = chipRc.top + chipSize.cy + 4;
                DrawRoundFill(hdc, &chipRc, 9, on ? RGB(0x47, 0x47, 0x47) : RGB(0x34, 0x34, 0x34), on ? RGB(0x47, 0x47, 0x47) : RGB(0x34, 0x34, 0x34));
                SetTextColor(hdc, on ? COLOR_INK : COLOR_MUTED);
                DrawTextA(hdc, chip, -1, &chipRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT));
                SetTextColor(hdc, on ? COLOR_INK_BRIGHT : COLOR_INK);
                RECT text = row;
                text.left += 10;
                text.right = chipRc.left - 8;
                DrawTextA(
                    hdc,
                    g_scenes[i].name,
                    -1,
                    &text,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS
                );
            }

            SelectObject(hdc, oldFont);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void DestroySlotCombo(int slot)
{
    if (slot < 0 || slot >= MAX_MANAGED_SCREENS)
    {
        return;
    }
    if (g_outputs[slot].combo != nullptr)
    {
        DestroyWindow(g_outputs[slot].combo);
        g_outputs[slot].combo = nullptr;
    }
}

static void FillSlotCombo(int slot)
{
    if (slot < 0 || slot >= g_outputCount || g_outputs[slot].combo == nullptr)
    {
        return;
    }

    char text[160] = {};
    if (g_outputs[slot].monitorIndex >= 0 && g_outputs[slot].monitorIndex < g_displayCount)
    {
        lstrcpynA(text, g_displays[g_outputs[slot].monitorIndex].title, sizeof(text));
    }
    else if (IsVirtualDevice(g_outputs[slot].device))
    {
        const int id = VirtualIdFromDevice(g_outputs[slot].device);
        const VirtualMode* mode = FindVirtualMode(g_outputs[slot].device);
        if (id > 0)
        {
            wsprintfA(text, "Display %d - %s", id, mode != nullptr ? mode->label : "Virtual");
        }
        else
        {
            wsprintfA(text, "Virtual %s", mode != nullptr ? mode->label : "display");
        }
    }
    else
    {
        lstrcpyA(text, "Choose a monitor...");
    }
    SetWindowTextA(g_outputs[slot].combo, text);
    InvalidateRect(g_outputs[slot].combo, nullptr, FALSE);
}

static void LayoutSlotCombos()
{
    if (g_outputMap == nullptr)
    {
        return;
    }

    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        if (g_outputs[i].combo == nullptr)
        {
            continue;
        }

        if (i >= g_outputCount)
        {
            ShowWindow(g_outputs[i].combo, SW_HIDE);
            continue;
        }

        const RECT rc = g_outputs[i].chooserRc;
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        if (width <= 8 || height <= 8)
        {
            ShowWindow(g_outputs[i].combo, SW_HIDE);
            continue;
        }

        MoveWindow(g_outputs[i].combo, rc.left, rc.top, width, height, TRUE);
        ShowWindow(g_outputs[i].combo, SW_SHOW);
        if (g_uiFont != nullptr)
        {
            SendMessageA(g_outputs[i].combo, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        }
    }
}

static void RebuildSlotCombos()
{
    if (g_outputMap == nullptr || g_instance == nullptr)
    {
        return;
    }

    EnsureUiFont();
    RefreshDisplays();
    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        if (i >= g_outputCount)
        {
            DestroySlotCombo(i);
            continue;
        }

        if (g_outputs[i].combo != nullptr)
        {
            char className[32] = {};
            GetClassNameA(g_outputs[i].combo, className, sizeof(className));
            if (lstrcmpiA(className, "BUTTON") != 0)
            {
                DestroySlotCombo(i);
            }
        }

        if (g_outputs[i].combo == nullptr)
        {
            g_outputs[i].combo = CreateWindowExA(
                0,
                "BUTTON",
                "Choose a monitor...",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                0, 0, 160, CHOOSER_H,
                g_outputMap,
                (HMENU)(INT_PTR)(ID_MONITOR_COMBO_BASE + i),
                g_instance,
                nullptr
            );
        }
        FillSlotCombo(i);
    }
    LayoutSlotCombos();
}

static LRESULT CALLBACK FullscreenProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            if (App_OutputCleared() || App_LiveHold())
            {
                HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(hdc, &client, fill);
                DeleteObject(fill);
            }
            else if (!App_LiveHold() && client.right > 0 && client.bottom > 0 && !GpuClient_OwnsOutputs())
            {
                LiveOutput_PaintHwnd(hwnd);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_DESTROY:
            ForgetClaimHwnd(hwnd);
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void AdoptOwner(HWND owner)
{
    if (owner == nullptr || !IsWindow(owner))
    {
        return;
    }

    g_owner = owner;
    if (g_manageWindow != nullptr && IsWindow(g_manageWindow))
    {
        SetWindowLongPtrA(g_manageWindow, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
    }

    for (int i = 0; i < g_claimCount; ++i)
    {
        if (g_claims[i].hwnd != nullptr && IsWindow(g_claims[i].hwnd))
        {
            SetWindowLongPtrA(g_claims[i].hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
        }
    }
    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        if (g_outputs[i].hwnd != nullptr && IsWindow(g_outputs[i].hwnd))
        {
            SetWindowLongPtrA(g_outputs[i].hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
        }
    }
    for (int s = 0; s < g_sceneCount; ++s)
    {
        for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
        {
            if (g_scenes[s].viewers[i] != nullptr && IsWindow(g_scenes[s].viewers[i]))
            {
                SetWindowLongPtrA(g_scenes[s].viewers[i], GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
            }
        }
    }
}

static HWND CreateFullscreenOnMonitor(const RECT* monitor)
{
    return CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "ManagedFullscreenWindow",
        "",
        WS_POPUP,
        monitor->left,
        monitor->top,
        monitor->right - monitor->left,
        monitor->bottom - monitor->top,
        g_owner,
        nullptr,
        g_instance,
        nullptr
    );
}

static void ClearOutput(int slot)
{
    if (slot < 0 || slot >= MAX_MANAGED_SCREENS)
    {
        return;
    }

    if (IsVirtualDevice(g_outputs[slot].device) || IsViewerHwnd(g_outputs[slot].hwnd))
    {
        if (g_outputs[slot].hwnd != nullptr)
        {
            DestroyWindow(g_outputs[slot].hwnd);
            g_outputs[slot].hwnd = nullptr;
        }
        if (g_activeScene >= 0 && g_activeScene < g_sceneCount)
        {
            g_scenes[g_activeScene].viewers[slot] = nullptr;
        }
        g_outputs[slot].monitorIndex = -1;
        g_outputs[slot].device[0] = '\0';
        return;
    }

    if (g_outputs[slot].device[0] != '\0')
    {
        char device[32] = {};
        lstrcpynA(device, g_outputs[slot].device, 32);
        g_outputs[slot].hwnd = nullptr;
        g_outputs[slot].monitorIndex = -1;
        g_outputs[slot].device[0] = '\0';
        if (g_activeScene >= 0 && g_activeScene < g_sceneCount)
        {
            g_scenes[g_activeScene].devices[slot][0] = '\0';
        }
        if (!DeviceAssignedOnAnyScene(device))
        {
            ReleasePhysicalDevice(device);
        }
        return;
    }

    if (g_outputs[slot].hwnd != nullptr)
    {
        if (IsFullscreenHwnd(g_outputs[slot].hwnd))
        {
            g_outputs[slot].hwnd = nullptr;
        }
        else
        {
            DestroyWindow(g_outputs[slot].hwnd);
            g_outputs[slot].hwnd = nullptr;
        }
    }
    g_outputs[slot].monitorIndex = -1;
    g_outputs[slot].device[0] = '\0';
}

static void PlaceOutputOnMonitor(int slot)
{
    if (slot < 0 || slot >= g_outputCount)
    {
        return;
    }

    const int monitor = g_outputs[slot].monitorIndex;
    if (monitor < 0 || monitor >= g_displayCount)
    {
        return;
    }

    if (IsViewerHwnd(g_outputs[slot].hwnd))
    {
        HWND viewer = g_outputs[slot].hwnd;
        g_outputs[slot].hwnd = nullptr;
        if (g_activeScene >= 0 && g_activeScene < g_sceneCount)
        {
            g_scenes[g_activeScene].viewers[slot] = nullptr;
        }
        DestroyWindow(viewer);
    }

    char previous[32] = {};
    lstrcpynA(previous, g_outputs[slot].device, 32);
    const char* device = g_displays[monitor].device;
    MonitorClaim* claim = GetOrCreateClaim(device, monitor);
    if (claim == nullptr)
    {
        return;
    }

    lstrcpynA(g_outputs[slot].device, device, 32);
    g_outputs[slot].monitorIndex = monitor;
    g_outputs[slot].hwnd = claim->hwnd;
    if (g_activeScene >= 0 && g_activeScene < g_sceneCount)
    {
        lstrcpynA(g_scenes[g_activeScene].devices[slot], device, 32);
    }
    if (previous[0] != '\0' && !IsVirtualDevice(previous) && lstrcmpiA(previous, device) != 0 &&
        !DeviceAssignedOnAnyScene(previous))
    {
        ReleasePhysicalDevice(previous);
    }
    if (claim->hwnd != nullptr)
    {
        const RECT* rect = &g_displays[monitor].rect;
        SetWindowPos(
            claim->hwnd,
            HWND_TOPMOST,
            rect->left,
            rect->top,
            rect->right - rect->left,
            rect->bottom - rect->top,
            g_outputsEnabled ? SWP_SHOWWINDOW : SWP_NOACTIVATE
        );
        UpdateWindow(claim->hwnd);
    }
    PresentCurrent();
}

static void RefreshChooserLabels()
{
    for (int i = 0; i < g_outputCount; ++i)
    {
        FillSlotCombo(i);
    }
    if (g_outputMap != nullptr)
    {
        InvalidateRect(g_outputMap, nullptr, FALSE);
    }
}

static void SaveVirtualsOnly()
{
    if (!Store_Ready())
    {
        return;
    }
    StoreVirtual items[STORE_VIRTUAL_MAX] = {};
    const int count = g_virtualCount < STORE_VIRTUAL_MAX ? g_virtualCount : STORE_VIRTUAL_MAX;
    for (int i = 0; i < count; ++i)
    {
        items[i].id = g_virtuals[i].id;
        lstrcpynA(items[i].mode, g_virtuals[i].mode[0] != '\0' ? g_virtuals[i].mode : "1080p60", sizeof(items[i].mode));
    }
    Store_SaveVirtuals(items, count);
}

static VirtualGlass* FindVirtualById(int id)
{
    if (id <= 0)
    {
        return nullptr;
    }
    for (int i = 0; i < g_virtualCount; ++i)
    {
        if (g_virtuals[i].id == id)
        {
            return &g_virtuals[i];
        }
    }
    return nullptr;
}

static void BindVirtualHwnd(int id, HWND hwnd)
{
    VirtualGlass* glass = FindVirtualById(id);
    if (glass != nullptr)
    {
        glass->hwnd = hwnd;
    }
    for (int s = 0; s < g_sceneCount; ++s)
    {
        for (int slot = 0; slot < MAX_MANAGED_SCREENS; ++slot)
        {
            if (VirtualIdFromDevice(g_scenes[s].devices[slot]) == id)
            {
                g_scenes[s].viewers[slot] = hwnd;
            }
        }
    }
    for (int i = 0; i < g_outputCount; ++i)
    {
        if (VirtualIdFromDevice(g_outputs[i].device) == id)
        {
            g_outputs[i].hwnd = hwnd;
        }
    }
}

static VirtualGlass* FindVirtualByDevice(const char* device)
{
    const int id = VirtualIdFromDevice(device);
    if (id > 0)
    {
        return FindVirtualById(id);
    }
    return nullptr;
}

static const VirtualMode* FindModeByLabel(const char* label)
{
    if (label == nullptr || label[0] == '\0')
    {
        return &kVirtualModes[5];
    }
    for (int i = 0; i < VIRTUAL_MODE_COUNT; ++i)
    {
        if (lstrcmpiA(kVirtualModes[i].label, label) == 0)
        {
            return &kVirtualModes[i];
        }
    }
    return &kVirtualModes[5];
}

static int NextVirtualId()
{
    int maxId = 0;
    for (int i = 0; i < g_virtualCount; ++i)
    {
        if (g_virtuals[i].id > maxId)
        {
            maxId = g_virtuals[i].id;
        }
    }
    return maxId + 1;
}

static VirtualGlass* AddVirtualRecord(int id, const char* mode)
{
    if (g_virtualCount >= MAX_VIRTUAL_DISPLAYS || id <= 0)
    {
        return nullptr;
    }
    if (FindVirtualById(id) != nullptr)
    {
        return FindVirtualById(id);
    }
    VirtualGlass* glass = &g_virtuals[g_virtualCount];
    ZeroMemory(glass, sizeof(*glass));
    glass->id = id;
    const VirtualMode* virtMode = FindModeByLabel(mode);
    lstrcpynA(glass->mode, virtMode->label, sizeof(glass->mode));
    FormatVirtualDevice(glass->device, 32, virtMode, id);
    g_virtualCount += 1;
    return glass;
}

static void EnsureVirtualPool()
{
    VirtualGlass previous[MAX_VIRTUAL_DISPLAYS] = {};
    const int prevCount = g_virtualCount < MAX_VIRTUAL_DISPLAYS ? g_virtualCount : MAX_VIRTUAL_DISPLAYS;
    for (int i = 0; i < prevCount; ++i)
    {
        previous[i] = g_virtuals[i];
    }

    StoreVirtual stored[STORE_VIRTUAL_MAX] = {};
    const int loaded = Store_Ready() ? Store_LoadVirtuals(stored, STORE_VIRTUAL_MAX) : 0;
    g_virtualCount = 0;
    for (int i = 0; i < loaded; ++i)
    {
        VirtualGlass* glass = AddVirtualRecord(stored[i].id, stored[i].mode);
        if (glass == nullptr)
        {
            continue;
        }
        for (int p = 0; p < prevCount; ++p)
        {
            if (previous[p].id == glass->id)
            {
                glass->hwnd = previous[p].hwnd;
                break;
            }
        }
    }

    bool dirty = false;
    for (int s = 0; s < g_sceneCount; ++s)
    {
        for (int slot = 0; slot < g_scenes[s].count && slot < MAX_MANAGED_SCREENS; ++slot)
        {
            const char* device = g_scenes[s].devices[slot];
            if (!IsVirtualDevice(device))
            {
                continue;
            }
            int id = VirtualIdFromDevice(device);
            char token[16] = {};
            VirtualModeToken(device, token, sizeof(token));
            if (id <= 0)
            {
                id = NextVirtualId();
                if (AddVirtualRecord(id, token) == nullptr)
                {
                    continue;
                }
                FormatVirtualDevice(g_scenes[s].devices[slot], 32, FindModeByLabel(token), id);
                dirty = true;
            }
            else if (FindVirtualById(id) == nullptr)
            {
                AddVirtualRecord(id, token);
                dirty = true;
            }
        }
    }
    if (dirty)
    {
        SaveVirtualsOnly();
    }
}

static void UnassignVirtual(int id, int keepSlot)
{
    VirtualGlass* glass = FindVirtualById(id);
    if (glass == nullptr)
    {
        return;
    }
    CaptureActiveScene();
    for (int s = 0; s < g_sceneCount; ++s)
    {
        for (int slot = 0; slot < MAX_MANAGED_SCREENS; ++slot)
        {
            if (VirtualIdFromDevice(g_scenes[s].devices[slot]) != id)
            {
                continue;
            }
            if (s == g_activeScene && slot == keepSlot)
            {
                continue;
            }
            g_scenes[s].devices[slot][0] = '\0';
            g_scenes[s].viewers[slot] = nullptr;
        }
    }
    for (int i = 0; i < g_outputCount; ++i)
    {
        if (i == keepSlot)
        {
            continue;
        }
        if (VirtualIdFromDevice(g_outputs[i].device) == id)
        {
            g_outputs[i].device[0] = '\0';
            g_outputs[i].monitorIndex = -1;
            if (IsViewerHwnd(g_outputs[i].hwnd))
            {
                g_outputs[i].hwnd = nullptr;
            }
        }
    }
}

static void AssignVirtualToSlot(int slot, int id)
{
    VirtualGlass* glass = FindVirtualById(id);
    if (glass == nullptr || slot < 0 || slot >= g_outputCount)
    {
        return;
    }
    UnassignVirtual(id, slot);
    lstrcpynA(g_outputs[slot].device, glass->device, 32);
    g_outputs[slot].monitorIndex = -1;
    g_outputs[slot].hwnd = glass->hwnd;
    SaveScreens();
}

static int MintVirtual(int modeIndex)
{
    if (modeIndex < 0 || modeIndex >= VIRTUAL_MODE_COUNT)
    {
        modeIndex = 5;
    }
    if (g_virtualCount >= MAX_VIRTUAL_DISPLAYS)
    {
        return 0;
    }
    const int id = NextVirtualId();
    if (AddVirtualRecord(id, kVirtualModes[modeIndex].label) == nullptr)
    {
        return 0;
    }
    SaveVirtualsOnly();
    return id;
}

static void HandleChooserCommand(int slot, int command)
{
    if (slot < 0 || slot >= g_outputCount)
    {
        return;
    }

    if (command == ID_CHOOSER_UNASSIGN)
    {
        ClearOutput(slot);
        SaveScreens();
        RefreshChooserLabels();
        PresentCurrent();
        RefreshProgramScreenMirror();
        return;
    }

    if (command >= ID_CHOOSER_MONITOR_BASE && command < ID_CHOOSER_MONITOR_BASE + MAX_MANAGED_SCREENS)
    {
        const int monitor = command - ID_CHOOSER_MONITOR_BASE;
        if (monitor >= 0 && monitor < g_displayCount)
        {
            g_outputs[slot].monitorIndex = monitor;
            PlaceOutputOnMonitor(slot);
            SaveScreens();
            RefreshChooserLabels();
            RefreshProgramScreenMirror();
        }
        return;
    }

    if (command >= ID_CHOOSER_VIRTUAL_BASE && command < ID_CHOOSER_VIRTUAL_BASE + MAX_VIRTUAL_DISPLAYS)
    {
        const int index = command - ID_CHOOSER_VIRTUAL_BASE;
        if (index >= 0 && index < g_virtualCount)
        {
            AssignVirtualToSlot(slot, g_virtuals[index].id);
            RefreshChooserLabels();
            App_OpenVirtualDisplay(g_virtuals[index].id);
            RefreshProgramScreenMirror();
            PresentCurrent();
        }
        return;
    }

    if (command >= ID_CHOOSER_NEW_MODE_BASE && command < ID_CHOOSER_NEW_MODE_BASE + VIRTUAL_MODE_COUNT)
    {
        const int mode = command - ID_CHOOSER_NEW_MODE_BASE;
        int target = -1;
        if (slot < g_outputCount &&
            g_outputs[slot].monitorIndex < 0 &&
            !IsVirtualDevice(g_outputs[slot].device))
        {
            target = slot;
        }
        else if (g_outputCount < MAX_MANAGED_SCREENS)
        {
            const int layout = ClampLayoutMode(g_scenes[g_activeScene].layoutMode);
            if (layout == SCREEN_LAYOUT_SINGLE && g_outputCount >= 1)
            {
                MessageBoxA(
                    g_manageWindow != nullptr ? g_manageWindow : g_owner,
                    "Single mode can only have one display.",
                    "Screens",
                    MB_OK | MB_ICONINFORMATION
                );
                return;
            }
            ApplyScreenCount(g_outputCount + 1, false);
            target = g_outputCount - 1;
        }
        if (target < 0)
        {
            MessageBoxA(
                g_manageWindow != nullptr ? g_manageWindow : g_owner,
                "You already have the maximum number of displays.",
                "Screens",
                MB_OK | MB_ICONINFORMATION
            );
            return;
        }

        const int id = MintVirtual(mode);
        if (id <= 0)
        {
            MessageBoxA(
                g_manageWindow != nullptr ? g_manageWindow : g_owner,
                "You already have the maximum number of virtual displays.",
                "Screens",
                MB_OK | MB_ICONINFORMATION
            );
            return;
        }
        AssignVirtualToSlot(target, id);
        RefreshChooserLabels();
        App_OpenVirtualDisplay(id);
        RefreshProgramScreenMirror();
        PresentCurrent();
        return;
    }
}

struct ChooserRow
{
    int command;
    int cascade;
    bool checked;
    bool separator;
    char text[160];
    char aside[48];
};

static ChooserRow g_chooserRows[3][48] = {};
static int g_chooserCount[3] = {};
static int g_chooserHover[3] = { -1, -1, -1 };
static bool g_chooserVirt = false;
static bool g_chooserNew = false;
static HWND g_chooserPop = nullptr;
static int g_chooserSlot = -1;
static RECT g_chooserPanel[3] = {};

static void AddChooserRow(int page, int command, int cascade, bool checked, bool separator, const char* text, const char* aside)
{
    if (page < 0 || page > 2 || g_chooserCount[page] >= 48)
    {
        return;
    }
    ChooserRow* row = &g_chooserRows[page][g_chooserCount[page]];
    ZeroMemory(row, sizeof(*row));
    row->command = command;
    row->cascade = cascade;
    row->checked = checked;
    row->separator = separator;
    if (text != nullptr)
    {
        lstrcpynA(row->text, text, sizeof(row->text));
    }
    if (aside != nullptr)
    {
        lstrcpynA(row->aside, aside, sizeof(row->aside));
    }
    g_chooserCount[page] += 1;
}

static void BuildChooserRows(int slot)
{
    for (int page = 0; page < 3; ++page)
    {
        g_chooserCount[page] = 0;
        g_chooserHover[page] = -1;
    }

    const bool unassigned = g_outputs[slot].monitorIndex < 0 && !IsVirtualDevice(g_outputs[slot].device);
    AddChooserRow(0, ID_CHOOSER_UNASSIGN, 0, unassigned, false, "Choose a monitor...", nullptr);
    AddChooserRow(0, 0, 0, false, true, nullptr, nullptr);
    for (int i = 0; i < g_displayCount; ++i)
    {
        char aside[48] = {};
        const int width = g_displays[i].rect.right - g_displays[i].rect.left;
        const int height = g_displays[i].rect.bottom - g_displays[i].rect.top;
        if (width > 0 && height > 0)
        {
            wsprintfA(aside, "%dx%d%s", width, height, g_displays[i].primary ? "  ·  Primary" : "");
        }
        const char* name = g_displays[i].label[0] != '\0' ? g_displays[i].label : g_displays[i].title;
        AddChooserRow(0, ID_CHOOSER_MONITOR_BASE + i, 0, g_outputs[slot].monitorIndex == i, false, name, aside);
    }
    AddChooserRow(0, 0, 0, false, true, nullptr, nullptr);
    AddChooserRow(0, 0, 1, false, false, "Virtual Screens", nullptr);

    EnsureVirtualPool();
    const int chosenVirtual = VirtualIdFromDevice(g_outputs[slot].device);
    for (int i = 0; i < g_virtualCount; ++i)
    {
        char label[80] = {};
        wsprintfA(label, "Display %d - %s", g_virtuals[i].id, g_virtuals[i].mode);
        AddChooserRow(1, ID_CHOOSER_VIRTUAL_BASE + i, 0, chosenVirtual == g_virtuals[i].id, false, label, nullptr);
    }
    if (g_virtualCount > 0)
    {
        AddChooserRow(1, 0, 0, false, true, nullptr, nullptr);
    }
    AddChooserRow(1, 0, 2, false, false, "New", nullptr);

    for (int i = 0; i < VIRTUAL_MODE_COUNT; ++i)
    {
        AddChooserRow(2, ID_CHOOSER_NEW_MODE_BASE + i, 0, false, false, kVirtualModes[i].label, nullptr);
    }
}

static int ChooserPageHeight(int page)
{
    int height = 10;
    for (int i = 0; i < g_chooserCount[page]; ++i)
    {
        height += g_chooserRows[page][i].separator ? 11 : 32;
    }
    return height;
}

static int ChooserRowTop(int page, int index, int panelTop)
{
    int top = panelTop + 5;
    for (int i = 0; i < index && i < g_chooserCount[page]; ++i)
    {
        top += g_chooserRows[page][i].separator ? 11 : 32;
    }
    return top;
}

static void PlaceChooserPop(const RECT* anchor)
{
    const int mainW = 360;
    const int mainH = ChooserPageHeight(0);
    const int virtW = 230;
    const int virtH = ChooserPageHeight(1);
    const int newW = 156;
    const int newH = ChooserPageHeight(2);

    g_chooserPanel[0].left = 0;
    g_chooserPanel[0].top = 0;
    g_chooserPanel[0].right = mainW;
    g_chooserPanel[0].bottom = mainH;

    int totalW = mainW;
    int totalH = mainH;
    int virtRow = 0;
    for (int i = 0; i < g_chooserCount[0]; ++i)
    {
        if (g_chooserRows[0][i].cascade == 1)
        {
            virtRow = ChooserRowTop(0, i, 0);
            break;
        }
    }
    if (g_chooserVirt)
    {
        g_chooserPanel[1].left = mainW - 6;
        g_chooserPanel[1].top = virtRow - 5;
        if (g_chooserPanel[1].top < 0)
        {
            g_chooserPanel[1].top = 0;
        }
        g_chooserPanel[1].right = g_chooserPanel[1].left + virtW;
        g_chooserPanel[1].bottom = g_chooserPanel[1].top + virtH;
        if (g_chooserPanel[1].right > totalW)
        {
            totalW = g_chooserPanel[1].right;
        }
        if (g_chooserPanel[1].bottom > totalH)
        {
            totalH = g_chooserPanel[1].bottom;
        }
    }

    if (g_chooserVirt && g_chooserNew)
    {
        int newRow = 0;
        for (int i = 0; i < g_chooserCount[1]; ++i)
        {
            if (g_chooserRows[1][i].cascade == 2)
            {
                newRow = ChooserRowTop(1, i, g_chooserPanel[1].top);
                break;
            }
        }
        g_chooserPanel[2].left = g_chooserPanel[1].right - 6;
        g_chooserPanel[2].top = newRow - 5;
        if (g_chooserPanel[2].top < 0)
        {
            g_chooserPanel[2].top = 0;
        }
        g_chooserPanel[2].right = g_chooserPanel[2].left + newW;
        g_chooserPanel[2].bottom = g_chooserPanel[2].top + newH;
        if (g_chooserPanel[2].right > totalW)
        {
            totalW = g_chooserPanel[2].right;
        }
        if (g_chooserPanel[2].bottom > totalH)
        {
            totalH = g_chooserPanel[2].bottom;
        }
    }

    RECT work = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    int x = anchor->left;
    int y = anchor->bottom + 4;
    if (x + totalW > work.right - 8)
    {
        x = anchor->right - totalW;
    }
    if (y + totalH > work.bottom - 8)
    {
        y = anchor->top - totalH - 4;
    }
    if (x < work.left + 8)
    {
        x = work.left + 8;
    }
    if (y < work.top + 8)
    {
        y = work.top + 8;
    }
    SetWindowPos(g_chooserPop, HWND_TOPMOST, x, y, totalW, totalH, SWP_NOACTIVATE);

    HRGN shape = CreateRectRgn(0, 0, 0, 0);
    const int shown = (g_chooserVirt && g_chooserNew) ? 3 : (g_chooserVirt ? 2 : 1);
    for (int page = 0; page < shown; ++page)
    {
        const RECT* panel = &g_chooserPanel[page];
        HRGN piece = CreateRoundRectRgn(
            panel->left,
            panel->top,
            panel->right + 1,
            panel->bottom + 1,
            18,
            18
        );
        CombineRgn(shape, shape, piece, RGN_OR);
        DeleteObject(piece);
    }
    SetWindowRgn(g_chooserPop, shape, TRUE);
}

static int ChooserHit(int page, POINT pt)
{
    if (!PtInRect(&g_chooserPanel[page], pt))
    {
        return -1;
    }
    for (int i = 0; i < g_chooserCount[page]; ++i)
    {
        const int top = ChooserRowTop(page, i, g_chooserPanel[page].top);
        const int height = g_chooserRows[page][i].separator ? 11 : 32;
        if (pt.y >= top && pt.y < top + height)
        {
            return g_chooserRows[page][i].separator ? -1 : i;
        }
    }
    return -1;
}

static void PaintChooserPage(HDC hdc, int page)
{
    const RECT panel = g_chooserPanel[page];
    HBRUSH back = CreateSolidBrush(RGB(0x2C, 0x2C, 0x2C));
    HPEN edge = CreatePen(PS_SOLID, 1, RGB(0x3E, 0x3E, 0x3E));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, back);
    HPEN oldPen = (HPEN)SelectObject(hdc, edge);
    RoundRect(hdc, panel.left, panel.top, panel.right, panel.bottom, 18, 18);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(back);
    DeleteObject(edge);

    EnsureUiFont();
    HFONT oldFont = (HFONT)SelectObject(hdc, g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < g_chooserCount[page]; ++i)
    {
        const ChooserRow* row = &g_chooserRows[page][i];
        const int top = ChooserRowTop(page, i, panel.top);
        if (row->separator)
        {
            HPEN line = CreatePen(PS_SOLID, 1, RGB(0x3E, 0x3E, 0x3E));
            HPEN old = (HPEN)SelectObject(hdc, line);
            MoveToEx(hdc, panel.left + 8, top + 5, nullptr);
            LineTo(hdc, panel.right - 8, top + 5);
            SelectObject(hdc, old);
            DeleteObject(line);
            continue;
        }

        RECT item = { panel.left + 5, top, panel.right - 5, top + 32 };
        if (g_chooserHover[page] == i)
        {
            DrawRoundFill(hdc, &item, 5, RGB(0x3C, 0x3C, 0x3C), RGB(0x3C, 0x3C, 0x3C));
        }
            if (row->checked)
            {
                const int x = item.left + 8;
                const int y = (item.top + item.bottom) / 2;
                HPEN markPen = CreatePen(PS_SOLID, 2, COLOR_INK);
                HPEN oldMark = (HPEN)SelectObject(hdc, markPen);
                MoveToEx(hdc, x, y, nullptr);
                LineTo(hdc, x + 3, y + 3);
                LineTo(hdc, x + 8, y - 3);
                SelectObject(hdc, oldMark);
                DeleteObject(markPen);
            }
        SetTextColor(hdc, COLOR_INK_BRIGHT);
        RECT text = item;
        text.left += 24;
        text.right -= row->cascade != 0 ? 22 : 8;
        if (row->aside[0] != '\0')
        {
            SelectObject(hdc, g_smallFont != nullptr ? g_smallFont : g_uiFont);
            SIZE asideSize = {};
            GetTextExtentPoint32A(hdc, row->aside, lstrlenA(row->aside), &asideSize);
            RECT aside = text;
            aside.left = aside.right - asideSize.cx;
            SetTextColor(hdc, COLOR_MUTED);
            DrawTextA(hdc, row->aside, -1, &aside, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            text.right = aside.left - 12;
            SelectObject(hdc, g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT));
            SetTextColor(hdc, COLOR_INK_BRIGHT);
        }
        DrawTextA(hdc, row->text, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (row->cascade != 0)
        {
            RECT arrow = item;
            arrow.left = arrow.right - 18;
            SetTextColor(hdc, COLOR_MUTED);
            DrawTextA(hdc, ">", -1, &arrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    SelectObject(hdc, oldFont);
}

static LRESULT CALLBACK ChooserPopProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HBRUSH clear = CreateSolidBrush(COLOR_BASE);
            FillRect(hdc, &client, clear);
            DeleteObject(clear);
            PaintChooserPage(hdc, 0);
            if (g_chooserVirt)
            {
                PaintChooserPage(hdc, 1);
            }
            if (g_chooserVirt && g_chooserNew)
            {
                PaintChooserPage(hdc, 2);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int pages = g_chooserNew && g_chooserVirt ? 3 : (g_chooserVirt ? 2 : 1);
            bool changed = false;
            bool overCascade = false;
            for (int page = 0; page < 3; ++page)
            {
                int hit = -1;
                if (page < pages)
                {
                    hit = ChooserHit(page, pt);
                }
                if (g_chooserHover[page] != hit)
                {
                    g_chooserHover[page] = hit;
                    changed = true;
                }
                if (hit >= 0 && g_chooserRows[page][hit].cascade != 0)
                {
                    overCascade = true;
                    if (g_chooserRows[page][hit].cascade == 1)
                    {
                        g_chooserVirt = true;
                    }
                    if (g_chooserRows[page][hit].cascade == 2)
                    {
                        g_chooserNew = true;
                    }
                }
            }
            if (!overCascade && pages == 1)
            {
                g_chooserVirt = false;
                g_chooserNew = false;
            }
            if (changed || overCascade)
            {
                RECT anchor = {};
                if (g_chooserSlot >= 0 && g_outputs[g_chooserSlot].combo != nullptr)
                {
                    GetWindowRect(g_outputs[g_chooserSlot].combo, &anchor);
                    PlaceChooserPop(&anchor);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT track = {};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            TrackMouseEvent(&track);
            return 0;
        }

        case WM_LBUTTONUP:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int pages = g_chooserNew && g_chooserVirt ? 3 : (g_chooserVirt ? 2 : 1);
            bool inside = false;
            for (int page = 0; page < pages; ++page)
            {
                if (PtInRect(&g_chooserPanel[page], pt))
                {
                    inside = true;
                }
                const int hit = ChooserHit(page, pt);
                if (hit < 0)
                {
                    continue;
                }
                const int command = g_chooserRows[page][hit].command;
    if (command != 0)
    {
                    const int slot = g_chooserSlot;
                    DestroyWindow(hwnd);
        HandleChooserCommand(slot, command);
    }
                return 0;
            }
            if (!inside)
            {
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            if (hwnd == g_chooserPop)
            {
                g_chooserPop = nullptr;
                g_chooserSlot = -1;
                g_chooserVirt = false;
                g_chooserNew = false;
            }
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void ShowChooserMenu(int slot)
{
    if (slot < 0 || slot >= g_outputCount || g_outputs[slot].combo == nullptr)
    {
        return;
    }
    if (g_chooserPop != nullptr && IsWindow(g_chooserPop))
    {
        DestroyWindow(g_chooserPop);
        g_chooserPop = nullptr;
    }

    EnsureDefaultScene();
    RefreshDisplays();
    BuildChooserRows(slot);
    g_chooserSlot = slot;
    g_chooserVirt = false;
    g_chooserNew = false;

    WNDCLASSA klass = {};
    if (!GetClassInfoA(g_instance, "ManageScreensChooser", &klass))
    {
        WNDCLASSA popup = {};
        popup.lpfnWndProc = ChooserPopProc;
        popup.hInstance = g_instance;
        popup.lpszClassName = "ManageScreensChooser";
        popup.hCursor = LoadCursor(nullptr, IDC_ARROW);
        popup.hbrBackground = CreateSolidBrush(COLOR_BASE);
        RegisterClassA(&popup);
    }

    RECT anchor = {};
    GetWindowRect(g_outputs[slot].combo, &anchor);
    g_chooserPop = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "ManageScreensChooser",
        "",
        WS_POPUP,
        anchor.left,
        anchor.bottom + 4,
        360,
        ChooserPageHeight(0),
        g_outputMap != nullptr ? g_outputMap : g_manageWindow,
        nullptr,
        g_instance,
        nullptr
    );
    if (g_chooserPop == nullptr)
    {
        return;
    }
    PlaceChooserPop(&anchor);
    ShowWindow(g_chooserPop, SW_SHOWNOACTIVATE);
    SetForegroundWindow(g_chooserPop);
}

static void SetCountText(int count)
{
    (void)count;
    if (g_outputMap != nullptr)
    {
        InvalidateRect(g_outputMap, nullptr, FALSE);
    }
}

static void NudgeScreenCount(int delta)
{
    EnsureDefaultScene();
    const bool single = ClampLayoutMode(g_scenes[g_activeScene].layoutMode) == SCREEN_LAYOUT_SINGLE;
    if (single && g_outputCount >= 1 && delta > 0)
    {
        return;
    }
    if (single && g_outputCount <= 1 && delta < 0)
    {
        return;
    }
    int count = g_outputCount + delta;
    if (count < 0)
    {
        count = 0;
    }
    if (count > ActiveSceneMaxSlots())
    {
        count = ActiveSceneMaxSlots();
    }
    if (count != g_outputCount)
    {
        ApplyScreenCount(count, true);
    }
}

static void LayoutMapChrome(const RECT* client)
{
    const int segW = 214;
    const int segH = 32;
    g_segOuter.right = client->right - 22;
    g_segOuter.left = g_segOuter.right - segW;
    g_segOuter.top = 22;
    g_segOuter.bottom = g_segOuter.top + segH;
    if (g_segOuter.left < 220)
    {
        g_segOuter.left = 220;
        g_segOuter.right = g_segOuter.left + segW;
    }
    const int innerL = g_segOuter.left + 3;
    const int innerT = g_segOuter.top + 3;
    const int slice = (segW - 6) / 3;
    for (int i = 0; i < 3; ++i)
    {
        g_segRc[i].left = innerL + (i * slice);
        g_segRc[i].top = innerT;
        g_segRc[i].right = (i == 2) ? (g_segOuter.right - 3) : (g_segRc[i].left + slice);
        g_segRc[i].bottom = g_segOuter.bottom - 3;
    }

    const int boxW = 98;
    const int boxH = 32;
    g_stepBox.right = client->right - 22;
    g_stepBox.left = g_stepBox.right - boxW;
    g_stepBox.bottom = client->bottom - 14;
    g_stepBox.top = g_stepBox.bottom - boxH;
    g_stepMinus.left = g_stepBox.left;
    g_stepMinus.top = g_stepBox.top;
    g_stepMinus.right = g_stepBox.left + 32;
    g_stepMinus.bottom = g_stepBox.bottom;
    g_stepPlus.left = g_stepBox.right - 32;
    g_stepPlus.top = g_stepBox.top;
    g_stepPlus.right = g_stepBox.right;
    g_stepPlus.bottom = g_stepBox.bottom;
}

static void ApplyScreenCount(int count, bool persist)
{
    RefreshDisplays();

    if (count < 0)
    {
        count = 0;
    }
    if (count > MAX_MANAGED_SCREENS)
    {
        count = MAX_MANAGED_SCREENS;
        SetCountText(count);
    }

    for (int i = count; i < MAX_MANAGED_SCREENS; ++i)
    {
        DestroySlotCombo(i);
        if (persist)
        {
            ClearOutput(i);
        }
    }

    const int previous = g_outputCount;
    g_outputCount = count;

    for (int i = previous; i < count; ++i)
    {
        if (g_outputs[i].hwnd == nullptr)
        {
            g_outputs[i].monitorIndex = -1;
            g_outputs[i].device[0] = '\0';
        }
    }

    LayoutOutputTiles();
    RebuildSlotCombos();
    if (g_outputMap != nullptr)
    {
        InvalidateRect(g_outputMap, nullptr, TRUE);
    }
    if (persist)
    {
        SaveScreens();
    }
    ManageScreens_RefreshUi();
    RefreshProgramScreenMirror();
}

static void ApplyFromEdit()
{
    if (g_applying || g_countEdit == nullptr)
    {
        return;
    }

    char text[16] = {};
    GetWindowTextA(g_countEdit, text, sizeof(text));
    int count = atoi(text);
    if (count > ActiveSceneMaxSlots())
    {
        count = ActiveSceneMaxSlots();
        SetCountText(count);
    }
    ApplyScreenCount(count, true);
}

static int ActiveSceneMaxSlots()
{
    EnsureDefaultScene();
    if (ClampLayoutMode(g_scenes[g_activeScene].layoutMode) == SCREEN_LAYOUT_SINGLE)
    {
        return 1;
    }
    return MAX_MANAGED_SCREENS;
}

static void LayoutOutputTiles()
{
    if (g_outputMap == nullptr)
    {
        return;
    }

    RECT map = {};
    GetClientRect(g_outputMap, &map);
    LayoutMapChrome(&map);

    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        SetRectEmpty(&g_outputs[i].tile);
        SetRectEmpty(&g_outputs[i].preview);
        SetRectEmpty(&g_outputs[i].chooserRc);
    }

    if (g_outputCount <= 0)
    {
        LayoutLayoutCombo();
        LayoutSlotCombos();
        return;
    }

    const int margin = 22;
    const int gap = TILE_GAP;
    const int bottomLimit = g_stepBox.top - MAP_LEGEND_H - 12;
    int availW = map.right - (margin * 2) - (gap * (g_outputCount - 1));
    if (availW < 40)
    {
        availW = 40;
    }

    int width = availW / g_outputCount;
    if (width > 250)
    {
        width = g_outputCount == 1 ? 380 : 250;
    }
    if (width < 168)
    {
        width = 168;
    }

    const int innerW = width - (CARD_PAD * 2);
    int previewH = MulDiv(innerW, 9, 16);
    if (previewH < 72)
    {
        previewH = 72;
    }
    int height = CARD_PAD + 22 + previewH + 10 + CHOOSER_H + CARD_PAD;
    const int maxH = bottomLimit - TILE_TOP;
    if (height > maxH && maxH > 120)
    {
        height = maxH;
        previewH = height - CARD_PAD - 22 - 10 - CHOOSER_H - CARD_PAD;
        if (previewH < 48)
        {
            previewH = 48;
            height = CARD_PAD + 22 + previewH + 10 + CHOOSER_H + CARD_PAD;
        }
    }

    const int totalWidth = (width * g_outputCount) + (gap * (g_outputCount - 1));
    int x = (map.right - totalWidth) / 2;
    if (x < margin)
    {
        x = margin;
    }
    const int y = TILE_TOP;

    for (int i = 0; i < g_outputCount; ++i)
    {
        g_outputs[i].tile.left = x;
        g_outputs[i].tile.top = y;
        g_outputs[i].tile.right = x + width;
        g_outputs[i].tile.bottom = y + height;

        g_outputs[i].preview.left = x + CARD_PAD;
        g_outputs[i].preview.right = x + width - CARD_PAD;
        g_outputs[i].preview.top = y + CARD_PAD + 22;
        g_outputs[i].preview.bottom = g_outputs[i].preview.top + previewH;

        g_outputs[i].chooserRc.left = g_outputs[i].preview.left;
        g_outputs[i].chooserRc.right = g_outputs[i].preview.right;
        g_outputs[i].chooserRc.top = g_outputs[i].preview.bottom + 10;
        g_outputs[i].chooserRc.bottom = g_outputs[i].chooserRc.top + CHOOSER_H;

        x += width + gap;
    }

    LayoutLayoutCombo();
    LayoutSlotCombos();
}

static void DrawRoundFill(HDC hdc, const RECT* rc, int radius, COLORREF fill, COLORREF border)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius * 2, radius * 2);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static LRESULT CALLBACK OutputMapProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_LBUTTONDOWN:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            for (int i = 0; i < 3; ++i)
            {
                if (PtInRect(&g_segRc[i], pt))
                {
                    SetLayoutMode(i);
                    return 0;
                }
            }
            if (PtInRect(&g_stepMinus, pt))
            {
                NudgeScreenCount(-1);
                return 0;
            }
            if (PtInRect(&g_stepPlus, pt))
            {
                NudgeScreenCount(1);
                return 0;
            }
            return 0;
        }

        case WM_SIZE:
            LayoutOutputTiles();
            return 0;

        case WM_COMMAND:
        {
            const int id = LOWORD(wParam);
            if (id == ID_LAYOUT_MODE && HIWORD(wParam) == CBN_SELCHANGE)
            {
                OnLayoutComboChanged();
                return 0;
            }
            if (id >= ID_MONITOR_COMBO_BASE &&
                id < ID_MONITOR_COMBO_BASE + MAX_MANAGED_SCREENS &&
                HIWORD(wParam) == BN_CLICKED)
            {
                ShowChooserMenu(id - ID_MONITOR_COMBO_BASE);
            }
            return 0;
        }

        case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            if (measure->CtlType == ODT_COMBOBOX)
            {
                measure->itemHeight = (measure->itemID == static_cast<UINT>(-1))
                    ? ((measure->CtlID == ID_LAYOUT_MODE) ? (LAYOUT_COMBO_H - 6) : (CHOOSER_H - 6))
                    : 26;
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw->CtlType != ODT_COMBOBOX && draw->CtlType != ODT_BUTTON)
            {
                break;
            }

            EnsureUiFont();
            const bool selected = (draw->itemState & ODS_SELECTED) != 0;
            const bool comboBox = (draw->CtlType == ODT_BUTTON) ||
                ((draw->itemState & ODS_COMBOBOXEDIT) != 0);
            const COLORREF fill = comboBox ? COLOR_CHOOSER : (selected ? RGB(0x45, 0x45, 0x45) : COLOR_PANEL);
            HBRUSH brush = CreateSolidBrush(fill);
            FillRect(draw->hDC, &draw->rcItem, brush);
            DeleteObject(brush);

            if (comboBox)
            {
                HPEN pen = CreatePen(PS_SOLID, 1, COLOR_EDGE);
                HPEN oldPen = (HPEN)SelectObject(draw->hDC, pen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(draw->hDC, GetStockObject(NULL_BRUSH));
                RoundRect(
                    draw->hDC,
                    draw->rcItem.left,
                    draw->rcItem.top,
                    draw->rcItem.right,
                    draw->rcItem.bottom,
                    CHOOSER_RADIUS * 2,
                    CHOOSER_RADIUS * 2
                );
                SelectObject(draw->hDC, oldBrush);
                SelectObject(draw->hDC, oldPen);
                DeleteObject(pen);
            }

            char text[160] = {};
            if (draw->CtlType == ODT_BUTTON)
            {
                GetWindowTextA(draw->hwndItem, text, sizeof(text));
            }
            else if (draw->itemID != static_cast<UINT>(-1))
            {
                SendMessageA(draw->hwndItem, CB_GETLBTEXT, draw->itemID, reinterpret_cast<LPARAM>(text));
            }
            else
            {
                const int sel = static_cast<int>(SendMessageA(draw->hwndItem, CB_GETCURSEL, 0, 0));
                if (sel >= 0)
                {
                    SendMessageA(draw->hwndItem, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(text));
                }
            }

            const bool unset = draw->CtlType == ODT_BUTTON && lstrcmpA(text, "Choose a monitor...") == 0;
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, unset ? COLOR_MUTED : COLOR_INK_BRIGHT);
            HFONT old = (HFONT)SelectObject(draw->hDC, g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT));
            RECT textRc = draw->rcItem;
            textRc.left += (draw->CtlType == ODT_BUTTON) ? 30 : 10;
            textRc.right -= (draw->CtlType == ODT_BUTTON) ? 22 : 10;
            if (draw->CtlType == ODT_BUTTON)
            {
                const int ix = draw->rcItem.left + 8;
                const int iy = (draw->rcItem.top + draw->rcItem.bottom) / 2 - 6;
                HPEN iconPen = CreatePen(PS_SOLID, 1, COLOR_MUTED);
                HPEN oldPen = (HPEN)SelectObject(draw->hDC, iconPen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(draw->hDC, GetStockObject(NULL_BRUSH));
                RoundRect(draw->hDC, ix, iy, ix + 14, iy + 9, 3, 3);
                MoveToEx(draw->hDC, ix + 5, iy + 9, nullptr);
                LineTo(draw->hDC, ix + 5, iy + 12);
                MoveToEx(draw->hDC, ix + 9, iy + 9, nullptr);
                LineTo(draw->hDC, ix + 9, iy + 12);
                MoveToEx(draw->hDC, ix + 3, iy + 12, nullptr);
                LineTo(draw->hDC, ix + 11, iy + 12);
                SelectObject(draw->hDC, oldBrush);
                SelectObject(draw->hDC, oldPen);
                DeleteObject(iconPen);
            }
            DrawTextA(draw->hDC, text, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (draw->CtlType == ODT_BUTTON)
            {
                const int midX = draw->rcItem.right - 14;
                const int midY = (draw->rcItem.top + draw->rcItem.bottom) / 2;
                POINT arrow[3] = {
                    { midX - 4, midY - 2 },
                    { midX + 4, midY - 2 },
                    { midX, midY + 3 }
                };
                HBRUSH arrowBrush = CreateSolidBrush(COLOR_MUTED);
                HPEN arrowPen = CreatePen(PS_SOLID, 1, COLOR_MUTED);
                HBRUSH oldBrush = (HBRUSH)SelectObject(draw->hDC, arrowBrush);
                HPEN oldPen = (HPEN)SelectObject(draw->hDC, arrowPen);
                Polygon(draw->hDC, arrow, 3);
                SelectObject(draw->hDC, oldBrush);
                SelectObject(draw->hDC, oldPen);
                DeleteObject(arrowBrush);
                DeleteObject(arrowPen);
            }
            SelectObject(draw->hDC, old);
            return TRUE;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);

            HBRUSH back = CreateSolidBrush(COLOR_PANEL);
            FillRect(hdc, &client, back);
            DeleteObject(back);

            EnsureUiFont();
            HFONT oldFont = (HFONT)SelectObject(hdc, g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT));
            SetBkMode(hdc, TRANSPARENT);

            EnsureDefaultScene();
            const int layout = ClampLayoutMode(g_scenes[g_activeScene].layoutMode);
            SelectObject(hdc, g_titleFont != nullptr ? g_titleFont : g_uiFont);
            SetTextColor(hdc, COLOR_INK_BRIGHT);
            RECT titleRc = { 22, 16, g_segOuter.left - 12, 46 };
            DrawTextA(hdc, g_scenes[g_activeScene].name, -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, g_smallFont != nullptr ? g_smallFont : g_uiFont);
            SetTextColor(hdc, COLOR_MUTED);
            RECT subRc = { 22, 46, g_segOuter.left - 12, 68 };
            DrawTextA(hdc, "Pick a layout, then choose a monitor for each display.", -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            DrawRoundFill(hdc, &g_segOuter, 6, COLOR_SUNKEN, RGB(0x33, 0x33, 0x33));
            const char* segNames[3] = { "Single", "Span", "Mirror" };
            SelectObject(hdc, g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT));
            for (int s = 0; s < 3; ++s)
            {
                const bool on = s == layout;
                if (on)
                {
                    DrawRoundFill(hdc, &g_segRc[s], 4, COLOR_RAISED_HI, COLOR_RAISED_HI);
                }
                SetTextColor(hdc, on ? COLOR_INK_BRIGHT : COLOR_MUTED);
                DrawTextA(hdc, segNames[s], -1, &g_segRc[s], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            if (g_outputCount == 0)
            {
                RECT hint = client;
                hint.top = TILE_TOP + 8;
                SetTextColor(hdc, COLOR_MUTED);
                DrawTextA(
                    hdc,
                    "Add a screen below to get started.",
                    -1,
                    &hint,
                    DT_CENTER | DT_TOP | DT_SINGLELINE
                );
            }

            for (int i = 0; i < g_outputCount; ++i)
            {
                const RECT card = g_outputs[i].tile;
                DrawRoundFill(hdc, &card, CARD_RADIUS, COLOR_SUNKEN, RGB(0x33, 0x33, 0x33));

                char label[32] = {};
                wsprintfA(label, "Display %d", i + 1);
                RECT caption = card;
                caption.top = card.top + 8;
                caption.bottom = g_outputs[i].preview.top;
                caption.left += CARD_PAD;
                caption.right -= CARD_PAD;
                SetTextColor(hdc, COLOR_INK_BRIGHT);
                SelectObject(hdc, g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT));
                DrawTextA(hdc, label, -1, &caption, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                const RECT preview = g_outputs[i].preview;
                const int previewW = preview.right - preview.left;
                const int previewH = preview.bottom - preview.top;
                const bool assigned =
                    (g_outputs[i].monitorIndex >= 0 &&
                    g_outputs[i].monitorIndex < g_displayCount) ||
                    IsVirtualDevice(g_outputs[i].device);

                if (!assigned)
                {
                    HBRUSH empty = CreateSolidBrush(RGB(0x19, 0x19, 0x19));
                    FillRect(hdc, &preview, empty);
                    DeleteObject(empty);
                    HPEN dash = CreatePen(PS_DASH, 1, RGB(0x3E, 0x3E, 0x3E));
                    HPEN oldPen = (HPEN)SelectObject(hdc, dash);
                    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, preview.left, preview.top, preview.right, preview.bottom);
                    SelectObject(hdc, oldBrush);
                    SelectObject(hdc, oldPen);
                    DeleteObject(dash);
                    SetTextColor(hdc, COLOR_MUTED);
                    DrawTextA(hdc, "Unassigned", -1, const_cast<RECT*>(&preview), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
                else
                {
                DrawRoundFill(hdc, &preview, MONITOR_RADIUS, COLOR_MONITOR, COLOR_MONITOR);
                HRGN clip = CreateRoundRectRgn(
                    preview.left,
                    preview.top,
                    preview.right + 1,
                    preview.bottom + 1,
                    MONITOR_RADIUS * 2,
                    MONITOR_RADIUS * 2
                );
                SelectClipRgn(hdc, clip);
                    if (!App_OutputCleared() && !App_LiveHold() && previewW > 0 && previewH > 0)
                {
                    Renderer_BlitAt(hdc, preview.left, preview.top, previewW, previewH);
                }
                    else if (IsVirtualDevice(g_outputs[i].device))
                {
                    const VirtualMode* mode = FindVirtualMode(g_outputs[i].device);
                    SetTextColor(hdc, COLOR_MUTED);
                    DrawTextA(
                        hdc,
                        mode != nullptr ? mode->label : "Virtual",
                        -1,
                        const_cast<RECT*>(&preview),
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE
                    );
                }
                SelectClipRgn(hdc, nullptr);
                DeleteObject(clip);
                }
            }

            SelectObject(hdc, g_smallFont != nullptr ? g_smallFont : g_uiFont);
            const char* legendLead = LayoutChipText(layout);
            const char* legendRest = " sends the picture to one display";
            if (layout == SCREEN_LAYOUT_SPAN)
            {
                legendRest = " stretches one picture across every display on the screen";
            }
            else if (layout == SCREEN_LAYOUT_MIRROR)
            {
                legendRest = " repeats the whole picture on each display";
            }
            SIZE leadSize = {};
            GetTextExtentPoint32A(hdc, legendLead, lstrlenA(legendLead), &leadSize);
            RECT legend = { 22, g_stepBox.top - MAP_LEGEND_H - 4, client.right - 22, g_stepBox.top - 4 };
            RECT leadRc = legend;
            leadRc.right = leadRc.left + leadSize.cx;
            SetTextColor(hdc, COLOR_INK);
            DrawTextA(hdc, legendLead, -1, &leadRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            RECT restRc = legend;
            restRc.left = leadRc.right + 6;
            SetTextColor(hdc, COLOR_MUTED);
            DrawTextA(hdc, legendRest, -1, &restRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            HPEN rule = CreatePen(PS_SOLID, 1, RGB(0x3A, 0x3A, 0x3A));
            HPEN oldRule = (HPEN)SelectObject(hdc, rule);
            MoveToEx(hdc, 22, g_stepBox.top - 8, nullptr);
            LineTo(hdc, client.right - 22, g_stepBox.top - 8);
            SelectObject(hdc, oldRule);
            DeleteObject(rule);

            const bool singleLocked = layout == SCREEN_LAYOUT_SINGLE && g_outputCount >= 1;
            const bool minusOff = singleLocked || g_outputCount <= 0;
            const bool plusOff = singleLocked || g_outputCount >= ActiveSceneMaxSlots();
            DrawRoundFill(hdc, &g_stepBox, 6, COLOR_SUNKEN, RGB(0x33, 0x33, 0x33));
            SetTextColor(hdc, minusOff ? COLOR_FAINT : COLOR_INK_BRIGHT);
            DrawTextA(hdc, "-", -1, &g_stepMinus, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SetTextColor(hdc, plusOff ? COLOR_FAINT : COLOR_INK_BRIGHT);
            DrawTextA(hdc, "+", -1, &g_stepPlus, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            char countText[8] = {};
            wsprintfA(countText, "%d", g_outputCount);
            RECT countRc = g_stepBox;
            countRc.left = g_stepMinus.right;
            countRc.right = g_stepPlus.left;
            SetTextColor(hdc, COLOR_INK_BRIGHT);
            SelectObject(hdc, g_uiFont != nullptr ? g_uiFont : GetStockObject(DEFAULT_GUI_FONT));
            DrawTextA(hdc, countText, -1, &countRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, g_smallFont != nullptr ? g_smallFont : g_uiFont);
            RECT foot = { 22, g_stepBox.top, g_stepBox.left - 16, g_stepBox.bottom };
            RECT dot = { 22, g_stepBox.top + 12, 29, g_stepBox.top + 19 };
            HBRUSH dotBrush = CreateSolidBrush(COLOR_GREEN);
            HPEN dotPen = CreatePen(PS_SOLID, 1, COLOR_GREEN);
            HBRUSH oldDotBrush = (HBRUSH)SelectObject(hdc, dotBrush);
            HPEN oldDotPen = (HPEN)SelectObject(hdc, dotPen);
            Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
            SelectObject(hdc, oldDotBrush);
            SelectObject(hdc, oldDotPen);
            DeleteObject(dotBrush);
            DeleteObject(dotPen);
            char detected[64] = {};
            wsprintfA(detected, "Displays detected: %d", g_displayCount);
            RECT detRc = foot;
            detRc.left = 36;
            SetTextColor(hdc, COLOR_MUTED);
            DrawTextA(hdc, detected, -1, &detRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            RECT stepLabel = foot;
            stepLabel.right = g_stepBox.left - 12;
            DrawTextA(hdc, "Fullscreen screens", -1, &stepLabel, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, oldFont);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK ManageProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkColor(hdc, COLOR_PANEL);
            SetTextColor(hdc, COLOR_INK);
            static HBRUSH brush = CreateSolidBrush(COLOR_PANEL);
            return reinterpret_cast<LRESULT>(brush);
        }

        case WM_SIZE:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            LayoutChrome(client.right, client.bottom);
            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_COUNT_EDIT && HIWORD(wParam) == EN_CHANGE)
            {
                ApplyFromEdit();
            }
            return 0;

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
            {
                g_outputs[i].combo = nullptr;
            }
            if (hwnd == g_manageWindow)
            {
                g_manageWindow = nullptr;
                g_sceneRail = nullptr;
                g_outputMap = nullptr;
                g_countEdit = nullptr;
                g_countUpDown = nullptr;
                g_countLabel = nullptr;
                g_detectedLabel = nullptr;
                g_layoutCombo = nullptr;
            }
            if (hwnd == g_hostPanel)
            {
                g_hostPanel = nullptr;
                g_sceneRail = nullptr;
                g_outputMap = nullptr;
                g_countEdit = nullptr;
                g_countUpDown = nullptr;
                g_countLabel = nullptr;
                g_detectedLabel = nullptr;
                g_layoutCombo = nullptr;
            }
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HBRUSH fill = CreateSolidBrush(COLOR_BASE);
            FillRect(hdc, &client, fill);
            DeleteObject(fill);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void EnsureClasses(HINSTANCE instance)
{
    if (g_classReady)
    {
        return;
    }

    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&controls);
    EnsureUiFont();

    WNDCLASSA manageClass = {};
    manageClass.lpfnWndProc = ManageProc;
    manageClass.hInstance = instance;
    manageClass.lpszClassName = "ManageScreensWindow";
    manageClass.hbrBackground = CreateSolidBrush(COLOR_BASE);
    manageClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&manageClass);

    WNDCLASSA hostClass = {};
    hostClass.lpfnWndProc = ManageProc;
    hostClass.hInstance = instance;
    hostClass.lpszClassName = "ManageScreensHost";
    hostClass.hbrBackground = CreateSolidBrush(COLOR_BASE);
    hostClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    hostClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&hostClass);

    WNDCLASSA sceneClass = {};
    sceneClass.lpfnWndProc = SceneRailProc;
    sceneClass.hInstance = instance;
    sceneClass.lpszClassName = "ManageScreensSceneRail";
    sceneClass.hbrBackground = CreateSolidBrush(COLOR_RAIL);
    sceneClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    sceneClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&sceneClass);

    WNDCLASSA mapClass = {};
    mapClass.lpfnWndProc = OutputMapProc;
    mapClass.hInstance = instance;
    mapClass.lpszClassName = "ManageScreensMap";
    mapClass.hbrBackground = CreateSolidBrush(COLOR_PANEL);
    mapClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    mapClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&mapClass);

    WNDCLASSA outputClass = {};
    outputClass.lpfnWndProc = FullscreenProc;
    outputClass.hInstance = instance;
    outputClass.lpszClassName = "ManagedFullscreenWindow";
    outputClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    outputClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&outputClass);

    g_classReady = true;
}

static void LayoutChrome(int width, int height)
{
    if (g_outputMap == nullptr)
    {
        return;
    }

    const int railW = SCENE_RAIL_W;
    const int innerH = height - (CHROME_PAD * 2);
    if (g_sceneRail != nullptr)
    {
        MoveWindow(g_sceneRail, CHROME_PAD, CHROME_PAD, railW, innerH > 80 ? innerH : 80, TRUE);
        RoundChild(g_sceneRail, 10);
    }

    const int mapX = CHROME_PAD + railW + CHROME_PAD;
    const int mapW = width - mapX - CHROME_PAD;
    MoveWindow(g_outputMap, mapX, CHROME_PAD, mapW > 200 ? mapW : 200, innerH > 80 ? innerH : 80, TRUE);
    RoundChild(g_outputMap, 10);
            LayoutLayoutCombo();
    LayoutOutputTiles();
}

static void BuildChrome(HWND parent, HINSTANCE instance)
{
    EnsureUiFont();
    RefreshDisplays();
    EnsureDefaultScene();

    if (g_sceneRail == nullptr)
    {
        g_sceneRail = CreateWindowExA(
            0,
            "ManageScreensSceneRail",
            "",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, SCENE_RAIL_W, 100,
            parent,
            nullptr,
            instance,
            nullptr
        );
    }

    if (g_outputMap == nullptr)
    {
        g_outputMap = CreateWindowExA(
            0,
            "ManageScreensMap",
            "",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            SCENE_RAIL_W, 0, 100, 100,
            parent,
            nullptr,
            instance,
            nullptr
        );
    }

    EnsureLayoutCombo();
    SyncLayoutCombo();
    SetCountText(g_outputCount);
}

static void ApplyPendingSlots(bool placeOutputs)
{
    RefreshDisplays();
    int count = g_pendingCount;
    if (count < 0)
    {
        count = 0;
    }
    if (count > MAX_MANAGED_SCREENS)
    {
        count = MAX_MANAGED_SCREENS;
    }

    ApplyScreenCount(count, false);
    for (int i = 0; i < g_outputCount; ++i)
    {
        lstrcpynA(g_outputs[i].device, g_pendingDevices[i], 32);
        g_outputs[i].monitorIndex = FindMonitorByDevice(g_outputs[i].device);
        if (g_outputs[i].monitorIndex >= 0)
        {
            if (placeOutputs)
            {
                PlaceOutputOnMonitor(i);
            }
        }
        else if (g_pendingDevices[i][0] != '\0')
        {
            lstrcpynA(g_outputs[i].device, g_pendingDevices[i], 32);
        }
    }
    RebuildSlotCombos();
    SetCountText(g_outputCount);
    CaptureActiveScene();
}

void ManageScreens_LoadPending(const BootTicket* ticket)
{
    g_pendingReady = false;
    g_pendingCount = 0;
    ZeroMemory(g_pendingDevices, sizeof(g_pendingDevices));
    if (ticket == nullptr)
    {
        return;
    }

    g_pendingCount = ticket->screenCount;
    if (g_pendingCount < 0)
    {
        g_pendingCount = 0;
    }
    if (g_pendingCount > MAX_MANAGED_SCREENS)
    {
        g_pendingCount = MAX_MANAGED_SCREENS;
    }
    for (int i = 0; i < g_pendingCount; ++i)
    {
        lstrcpynA(g_pendingDevices[i], ticket->screens[i], 64);
    }
    g_pendingReady = true;
    g_restored = false;
}

void ManageScreens_LoadFromStore()
{
    if (!Store_Ready())
    {
        BootTicket ticket = {};
        Boot_Defaults(&ticket);
        ManageScreens_LoadPending(&ticket);
        EnsureDefaultScene();
        return;
    }

    LoadScenesFromStore();
    BootTicket ticket = {};
    Boot_Defaults(&ticket);
    EnsureDefaultScene();
    ticket.screenCount = g_scenes[g_activeScene].count;
    if (ticket.screenCount < 0)
    {
        ticket.screenCount = 0;
    }
    if (ticket.screenCount > BOOT_MAX_SCREENS)
    {
        ticket.screenCount = BOOT_MAX_SCREENS;
    }
    for (int i = 0; i < ticket.screenCount; ++i)
    {
        lstrcpynA(ticket.screens[i], g_scenes[g_activeScene].devices[i], 64);
    }
    ManageScreens_LoadPending(&ticket);
}

void ManageScreens_Restore(HWND owner, HINSTANCE instance)
{
    if (!App_Ready())
    {
        return;
    }

    if (owner != nullptr)
    {
        AdoptOwner(owner);
    }
    if (instance != nullptr)
    {
        g_instance = instance;
    }
    if (g_instance == nullptr)
    {
        g_instance = App_Instance();
    }
    if (g_instance == nullptr)
    {
        return;
    }

    if (!g_pendingReady && Store_Ready())
    {
        ManageScreens_LoadFromStore();
    }

    EnsureClasses(g_instance);

    if (!g_restored && g_pendingReady)
    {
        ApplyPendingSlots(false);
        EnsureClaimsForAllScenes();
        ApplyScene(g_activeScene, false);
        g_restored = true;
    }
    else if (g_outputCount > 0)
    {
        RefreshDisplays();
        EnsureClaimsForAllScenes();
        ApplyScene(g_activeScene, false);
        RebuildSlotCombos();
    }

    PresentCurrent();
}

void ManageScreens_CreateIn(HWND parent, HINSTANCE instance)
{
    if (parent == nullptr || instance == nullptr)
    {
        return;
    }

    g_owner = GetAncestor(parent, GA_ROOT);
    g_instance = instance;
    EnsureClasses(instance);

    if (g_hostPanel == nullptr)
    {
        g_hostPanel = CreateWindowExA(
            0,
            "ManageScreensHost",
            "",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 100, 100,
            parent,
            nullptr,
            instance,
            nullptr
        );
    }

    BuildChrome(g_hostPanel, instance);

    if (g_pendingReady && g_outputCount == 0 && !g_restored)
    {
        ApplyPendingSlots(false);
    }

    RECT client = {};
    GetClientRect(parent, &client);
    ManageScreens_Layout(0, 0, client.right, client.bottom);
    RebuildSlotCombos();
    ManageScreens_RefreshUi();
}

void ManageScreens_Layout(int x, int y, int width, int height)
{
    if (g_hostPanel == nullptr)
    {
        return;
    }
    MoveWindow(g_hostPanel, x, y, width, height, TRUE);
    LayoutChrome(width, height);
}

void ManageScreens_RefreshUi()
{
    if (g_detectedLabel != nullptr)
    {
        char detected[64] = {};
        wsprintfA(detected, "Displays detected: %d", g_displayCount);
        SetWindowTextA(g_detectedLabel, detected);
    }
    if (g_countUpDown != nullptr)
    {
        SendMessage(g_countUpDown, UDM_SETRANGE32, 0, ActiveSceneMaxSlots());
    }
    SetCountText(g_outputCount);
    if (g_outputMap != nullptr)
    {
        InvalidateRect(g_outputMap, nullptr, FALSE);
    }
    if (g_hostPanel != nullptr)
    {
        InvalidateRect(g_hostPanel, nullptr, FALSE);
    }
}

int ManageScreens_OutputCount()
{
    return g_outputCount;
}

bool ManageScreens_GetOutputLabel(int index, char* out, int outChars)
{
    if (out == nullptr || outChars <= 0 || index < 0 || index >= g_outputCount)
    {
        return false;
    }

    if (g_outputs[index].monitorIndex >= 0 && g_outputs[index].monitorIndex < g_displayCount)
    {
        wsprintfA(out, "Display %d - %s", index + 1, g_displays[g_outputs[index].monitorIndex].title);
    }
    else if (IsVirtualDevice(g_outputs[index].device))
    {
        const int id = VirtualIdFromDevice(g_outputs[index].device);
        const VirtualMode* mode = FindVirtualMode(g_outputs[index].device);
        if (id > 0)
        {
            wsprintfA(out, "Display %d - %s", id, mode != nullptr ? mode->label : "Virtual");
        }
        else
        {
            wsprintfA(out, "Display %d - %s", index + 1, mode != nullptr ? mode->label : "Virtual");
        }
    }
    else
    {
        wsprintfA(out, "Display %d - Unassigned", index + 1);
    }
    out[outChars - 1] = '\0';
    return true;
}

int ManageScreens_ScreenCount()
{
    EnsureDefaultScene();
    return g_sceneCount;
}

int ManageScreens_ActiveScreen()
{
    EnsureDefaultScene();
    if (g_activeScene < 0 || g_activeScene >= g_sceneCount)
    {
        return 0;
    }
    return g_activeScene;
}

bool ManageScreens_ActivateScreen(int index)
{
    EnsureDefaultScene();
    if (index < 0 || index >= g_sceneCount)
    {
        return false;
    }
    CaptureActiveScene();
    ApplyScene(index, false);
    SaveScenes();
    return true;
}

bool ManageScreens_GetScreenLabel(int index, char* out, int outChars)
{
    EnsureDefaultScene();
    if (out == nullptr || outChars <= 0 || index < 0 || index >= g_sceneCount)
    {
        return false;
    }

    lstrcpynA(out, g_scenes[index].name, outChars);
    return out[0] != '\0';
}

int ManageScreens_GetScreenLayout(int index)
{
    EnsureDefaultScene();
    if (index < 0 || index >= g_sceneCount)
    {
        return SCREEN_LAYOUT_SINGLE;
    }
    return ClampLayoutMode(g_scenes[index].layoutMode);
}

int ManageScreens_GetScreenOutputCount(int index)
{
    EnsureDefaultScene();
    if (index < 0 || index >= g_sceneCount)
    {
        return 0;
    }
    int count = g_scenes[index].count;
    if (count < 0)
    {
        return 0;
    }
    if (count > MAX_MANAGED_SCREENS)
    {
        return MAX_MANAGED_SCREENS;
    }
    return count;
}

bool ManageScreens_GetScreenOutputLabel(int index, int slot, char* out, int outChars)
{
    EnsureDefaultScene();
    if (out == nullptr || outChars <= 0 || index < 0 || index >= g_sceneCount)
    {
        return false;
    }
    if (slot < 0 || slot >= g_scenes[index].count)
    {
        return false;
    }

    const char* device = g_scenes[index].devices[slot];
    RefreshDisplays();
    const int monitor = FindMonitorByDevice(device);
    if (monitor >= 0 && monitor < g_displayCount)
    {
        wsprintfA(out, "Disp %d", slot + 1);
        const char* title = g_displays[monitor].title;
        if (title != nullptr && title[0] != '\0')
        {
            char shortTitle[48] = {};
            lstrcpynA(shortTitle, title, 28);
            wsprintfA(out, "Disp %d - %s", slot + 1, shortTitle);
        }
    }
    else if (IsVirtualDevice(device))
    {
        const int id = VirtualIdFromDevice(device);
        const VirtualMode* mode = FindVirtualMode(device);
        if (id > 0)
        {
            wsprintfA(out, "Display %d - %s", id, mode != nullptr ? mode->label : "Virtual");
        }
        else
        {
            wsprintfA(out, "Disp %d - %s", slot + 1, mode != nullptr ? mode->label : "Virtual");
        }
    }
    else
    {
        wsprintfA(out, "Disp %d", slot + 1);
    }
    out[outChars - 1] = '\0';
    return true;
}

bool ManageScreens_GetScreenOutputSlice(int index, int slot, int* left, int* right)
{
    EnsureDefaultScene();
    if (index < 0 || index >= g_sceneCount || slot < 0)
    {
        return false;
    }

    const int count = ManageScreens_GetScreenOutputCount(index);
    if (slot >= count)
    {
        return false;
    }

    ApplyDefaultCanvas(&g_scenes[index]);
    const int width = g_scenes[index].canvasW;
    const int layout = ClampLayoutMode(g_scenes[index].layoutMode);
    int x0 = 0;
    int x1 = width > 0 ? width - 1 : 0;
    if (layout == SCREEN_LAYOUT_SPAN && count > 1 && width > 0)
    {
        const int slice = width / count;
        x0 = slot * slice;
        x1 = (slot + 1 == count) ? width - 1 : (slot + 1) * slice - 1;
    }
    if (left != nullptr)
    {
        *left = x0;
    }
    if (right != nullptr)
    {
        *right = x1;
    }
    return true;
}

int ManageScreens_AddVirtualScreen()
{
    return ManageScreens_AddVirtualDisplay(5);
}

int ManageScreens_VirtualCount()
{
    EnsureVirtualPool();
    return g_virtualCount;
}

int ManageScreens_GetVirtualId(int index)
{
    EnsureVirtualPool();
    if (index < 0 || index >= g_virtualCount)
    {
        return 0;
    }
    return g_virtuals[index].id;
}

bool ManageScreens_GetVirtualLabel(int index, char* out, int outChars)
{
    EnsureVirtualPool();
    if (out == nullptr || outChars <= 0 || index < 0 || index >= g_virtualCount)
    {
        return false;
    }
    wsprintfA(out, "Display %d - %s", g_virtuals[index].id, g_virtuals[index].mode);
    out[outChars - 1] = '\0';
    return true;
}

HWND ManageScreens_GetVirtualHwnd(int id)
{
    VirtualGlass* glass = FindVirtualById(id);
    if (glass == nullptr)
    {
        return nullptr;
    }
    return glass->hwnd;
}

void ManageScreens_SetVirtualHwnd(int id, HWND hwnd)
{
    BindVirtualHwnd(id, hwnd);
}

void ManageScreens_NoteVirtualClosed(int id)
{
    BindVirtualHwnd(id, nullptr);
}

int ManageScreens_AddVirtualDisplay(int modeIndex)
{
    EnsureDefaultScene();
    EnsureVirtualPool();
    const int id = MintVirtual(modeIndex);
    if (id <= 0)
    {
        return -1;
    }

    int target = -1;
    for (int i = 0; i < g_outputCount; ++i)
    {
        if (g_outputs[i].device[0] == '\0' && g_outputs[i].monitorIndex < 0)
        {
            target = i;
            break;
        }
    }
    if (target < 0 && g_outputCount < MAX_MANAGED_SCREENS)
    {
        const int layout = ClampLayoutMode(g_scenes[g_activeScene].layoutMode);
        if (layout != SCREEN_LAYOUT_SINGLE || g_outputCount < 1)
        {
            ApplyScreenCount(g_outputCount + 1, false);
            target = g_outputCount - 1;
        }
    }
    if (target >= 0)
    {
        AssignVirtualToSlot(target, id);
        RefreshChooserLabels();
        RefreshProgramScreenMirror();
    }
    else
    {
        App_RebuildScreenMenus();
    }
    return id;
}

bool ManageScreens_GetScreenCanvas(int index, int* width, int* height, int* refreshHz)
{
    EnsureDefaultScene();
    if (index < 0 || index >= g_sceneCount)
    {
        return false;
    }

    ApplyDefaultCanvas(&g_scenes[index]);
    if (width != nullptr)
    {
        *width = g_scenes[index].canvasW;
    }
    if (height != nullptr)
    {
        *height = g_scenes[index].canvasH;
    }
    if (refreshHz != nullptr)
    {
        *refreshHz = g_scenes[index].refreshHz;
    }
    return true;
}

bool ManageScreens_SetScreenCanvas(int index, int width, int height, int refreshHz)
{
    EnsureDefaultScene();
    if (index < 0 || index >= g_sceneCount || width <= 0 || height <= 0 || refreshHz <= 0)
    {
        return false;
    }

    CaptureActiveScene();
    g_scenes[index].canvasW = width;
    g_scenes[index].canvasH = height;
    g_scenes[index].refreshHz = refreshHz;
    SaveScenes();
    return true;
}

void ManageScreens_SetOutputHwnd(int index, HWND hwnd)
{
    if (index < 0 || index >= MAX_MANAGED_SCREENS)
    {
        return;
    }
    g_outputs[index].hwnd = hwnd;
    if (IsViewerHwnd(hwnd) && g_activeScene >= 0 && g_activeScene < g_sceneCount)
    {
        g_scenes[g_activeScene].viewers[index] = hwnd;
    }
    const int id = VirtualIdFromDevice(g_outputs[index].device);
    if (id > 0)
    {
        BindVirtualHwnd(id, hwnd);
    }
}

void ManageScreens_NoteViewerClosed(int index)
{
    if (index < 0 || index >= MAX_MANAGED_SCREENS)
    {
        return;
    }
    const int id = VirtualIdFromDevice(g_outputs[index].device);
    g_outputs[index].hwnd = nullptr;
    if (g_activeScene >= 0 && g_activeScene < g_sceneCount)
    {
        g_scenes[g_activeScene].viewers[index] = nullptr;
    }
    if (id > 0)
    {
        BindVirtualHwnd(id, nullptr);
    }
}

HWND ManageScreens_GetOutputHwnd(int index)
{
    if (index < 0 || index >= MAX_MANAGED_SCREENS)
    {
        return nullptr;
    }
    const int id = VirtualIdFromDevice(g_outputs[index].device);
    if (id > 0)
    {
        VirtualGlass* glass = FindVirtualById(id);
        if (glass != nullptr && glass->hwnd != nullptr && IsWindow(glass->hwnd))
        {
            return glass->hwnd;
        }
    }
    return g_outputs[index].hwnd;
}

HWND ManageScreens_GetScreenOutputHwnd(int screen, int slot)
{
    EnsureDefaultScene();
    CaptureActiveScene();
    if (screen < 0 || screen >= g_sceneCount || slot < 0 || slot >= g_scenes[screen].count)
    {
        return nullptr;
    }

    const char* device = g_scenes[screen].devices[slot];
    if (device[0] == '\0')
    {
        return nullptr;
    }

    if (IsVirtualDevice(device))
    {
        VirtualGlass* glass = FindVirtualById(VirtualIdFromDevice(device));
        if (glass != nullptr && glass->hwnd != nullptr && IsWindow(glass->hwnd))
        {
            return glass->hwnd;
        }
        HWND hwnd = g_scenes[screen].viewers[slot];
        if (hwnd != nullptr && IsWindow(hwnd))
        {
            return hwnd;
        }
        if (screen == g_activeScene && slot < g_outputCount)
        {
            hwnd = g_outputs[slot].hwnd;
            if (hwnd != nullptr && IsWindow(hwnd))
            {
                return hwnd;
            }
        }
        return nullptr;
    }

    HWND hwnd = ClaimHwnd(device);
    if (hwnd != nullptr && IsWindow(hwnd))
    {
        return hwnd;
    }
    if (screen == g_activeScene && slot < g_outputCount)
    {
        hwnd = g_outputs[slot].hwnd;
        if (hwnd != nullptr && IsWindow(hwnd))
        {
            return hwnd;
        }
    }
    return nullptr;
}

bool ManageScreens_OutputsEnabled()
{
    LoadOutputsEnabled();
    return g_outputsEnabled;
}

void ManageScreens_SetOutputsEnabled(bool enabled)
{
    LoadOutputsEnabled();
    g_outputsEnabled = enabled;
    if (Store_Ready())
    {
        Store_SetInt("outputs.enabled", enabled ? 1 : 0);
    }
    for (int i = 0; i < g_claimCount; ++i)
    {
        HWND hwnd = g_claims[i].hwnd;
        if (hwnd == nullptr || !IsWindow(hwnd))
        {
            continue;
        }
        if (enabled)
        {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        else
        {
            ShowWindow(hwnd, SW_HIDE);
        }
    }
    if (enabled && GpuClient_OwnsOutputs())
    {
        GpuClient_Repaint();
    }
}

void ManageScreens_Show(HWND owner, HINSTANCE instance)
{
    if (!App_Ready())
    {
        return;
    }

    AdoptOwner(owner);
    g_instance = instance;
    EnsureClasses(instance);
    RefreshDisplays();

    if (g_pendingReady && g_outputCount == 0 && !g_restored)
    {
        ApplyPendingSlots(false);
    }

    if (g_hostPanel != nullptr)
    {
        DestroyWindow(g_hostPanel);
        g_hostPanel = nullptr;
    }

    if (g_manageWindow != nullptr)
    {
        ShowWindow(g_manageWindow, SW_SHOW);
        ApplyDarkCaption(g_manageWindow);
        SetForegroundWindow(g_manageWindow);
        LayoutOutputTiles();
        RebuildSlotCombos();
        InvalidateRect(g_outputMap, nullptr, TRUE);
        RefreshProgramScreenMirror();
        return;
    }

    RECT ownerRect = {};
    GetWindowRect(owner, &ownerRect);

    g_manageWindow = CreateWindowExA(
        0,
        "ManageScreensWindow",
        "Manage screens",
        WS_OVERLAPPEDWINDOW,
        ownerRect.left + 40,
        ownerRect.top + 40,
        1120,
        720,
        owner,
        nullptr,
        instance,
        nullptr
    );

    BuildChrome(g_manageWindow, instance);
    ApplyDarkCaption(g_manageWindow);
    RECT client = {};
    GetClientRect(g_manageWindow, &client);
    LayoutChrome(client.right, client.bottom);
    RebuildSlotCombos();
    ShowWindow(g_manageWindow, SW_SHOW);
    RefreshProgramScreenMirror();
}

int ManageScreens_CollectHwnds(HWND* out, int max)
{
    if (out == nullptr || max <= 0)
    {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < g_claimCount && count < max; ++i)
    {
        if (g_claims[i].hwnd != nullptr)
        {
            out[count] = g_claims[i].hwnd;
            count += 1;
        }
    }
    for (int i = 0; i < MAX_MANAGED_SCREENS && count < max; ++i)
    {
        HWND hwnd = g_outputs[i].hwnd;
        if (hwnd == nullptr)
        {
            continue;
        }
        bool seen = false;
        for (int k = 0; k < count; ++k)
        {
            if (out[k] == hwnd)
            {
                seen = true;
                break;
            }
        }
        if (!seen)
        {
            out[count] = hwnd;
            count += 1;
        }
    }
    for (int i = 0; i < g_virtualCount && count < max; ++i)
    {
        HWND hwnd = g_virtuals[i].hwnd;
        if (hwnd == nullptr || !IsWindow(hwnd))
        {
            continue;
        }
        bool seen = false;
        for (int k = 0; k < count; ++k)
        {
            if (out[k] == hwnd)
            {
                seen = true;
                break;
            }
        }
        if (!seen)
        {
            out[count] = hwnd;
            count += 1;
        }
    }
    return count;
}

void ManageScreens_Present()
{
    if (!App_Ready())
    {
        return;
    }

    HWND hwnds[MAX_MANAGED_SCREENS * 2] = {};
    const int count = ManageScreens_CollectHwnds(hwnds, MAX_MANAGED_SCREENS * 2);
    for (int i = 0; i < count; ++i)
    {
        if (hwnds[i] == nullptr)
        {
            continue;
        }
        if (App_OutputCleared() || App_LiveHold())
        {
            if (GpuClient_OwnsOutputs())
            {
                continue;
            }
            HDC hdc = GetDC(hwnds[i]);
            if (hdc != nullptr)
            {
                RECT client = {};
                GetClientRect(hwnds[i], &client);
                HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(hdc, &client, fill);
                DeleteObject(fill);
                ReleaseDC(hwnds[i], hdc);
            }
        }
        else if (!App_LiveHold() && !GpuClient_OwnsOutputs())
        {
            LiveOutput_PaintHwnd(hwnds[i]);
        }
    }

    if (g_outputMap != nullptr)
    {
        InvalidateRect(g_outputMap, nullptr, FALSE);
    }
}

void ManageScreens_Close()
{
    for (int i = 0; i < g_claimCount; ++i)
    {
        HWND hwnd = g_claims[i].hwnd;
        g_claims[i].hwnd = nullptr;
        if (hwnd != nullptr && IsWindow(hwnd))
        {
            DestroyWindow(hwnd);
        }
    }
    g_claimCount = 0;

    for (int s = 0; s < g_sceneCount; ++s)
    {
        for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
        {
            HWND viewer = g_scenes[s].viewers[i];
            g_scenes[s].viewers[i] = nullptr;
            if (viewer != nullptr && IsWindow(viewer))
            {
                DestroyWindow(viewer);
            }
        }
    }

    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        DestroySlotCombo(i);
        HWND hwnd = g_outputs[i].hwnd;
        g_outputs[i].hwnd = nullptr;
        if (hwnd != nullptr && IsWindow(hwnd))
        {
            DestroyWindow(hwnd);
        }
    }

    if (g_manageWindow != nullptr)
    {
        DestroyWindow(g_manageWindow);
        g_manageWindow = nullptr;
    }
    if (g_hostPanel != nullptr)
    {
        DestroyWindow(g_hostPanel);
        g_hostPanel = nullptr;
    }
    g_outputMap = nullptr;
    g_countEdit = nullptr;
    g_countUpDown = nullptr;
    g_countLabel = nullptr;
    g_detectedLabel = nullptr;
    g_layoutCombo = nullptr;
}
