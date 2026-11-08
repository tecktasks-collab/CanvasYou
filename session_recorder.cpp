#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "session_recorder.h"
#include "store.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

// Indexes match the Recording page dropdowns.
static const DWORD RECORDER_RATE_MS[] = { 100, 250, 500, 1000 };
static const int RECORDER_KEEP_N[] = { 3, 5, 10 };

// Safety limits. Recording stops (STOPPED.txt explains why) when any is hit.
static const ULONGLONG RECORDER_MAX_SESSION_MS = 15ull * 60 * 1000;               // 15 minutes
static const ULONGLONG RECORDER_MAX_SESSION_BYTES = 500ull * 1024 * 1024;         // 500 MB per session
static const ULONGLONG RECORDER_MAX_TOTAL_BYTES = 2048ull * 1024 * 1024;          // 2 GB across all sessions
static const ULONGLONG RECORDER_MIN_FREE_BYTES = 5ull * 1024 * 1024 * 1024;       // keep 5 GB free on the drive

struct RecConfig
{
    bool on;
    DWORD intervalMs;
    int keep;
    char root[MAX_PATH];
};

static HANDLE g_recorderThread = nullptr;
static HANDLE g_recorderStop = nullptr;
static HANDLE g_reload = nullptr;
static RecConfig g_cfg = {};
static bool g_sessionOpen = false;
static volatile LONG g_resetFrames = 0;
static char g_appFolder[MAX_PATH] = {};       // folder Initializer.exe lives in
static char g_sessionFolder[MAX_PATH] = {};   // "<root>YYYYMMDD_HHMMSS/" (backslash-terminated)
static ULONGLONG g_sessionStartTick = 0;
static ULONGLONG g_sessionBytes = 0;          // written this session
static ULONGLONG g_olderSessionsBytes = 0;    // what the kept older sessions already occupy

// 32bpp top-down DIB we BitBlt the screen into.
struct Frame
{
    int width;
    int height;
    BYTE* pixels;
    HBITMAP bitmap;
    HDC dc;
};

static void FreeFrame(Frame* frame)
{
    if (frame->dc != nullptr)
    {
        DeleteDC(frame->dc);
    }
    if (frame->bitmap != nullptr)
    {
        DeleteObject(frame->bitmap);
    }
    ZeroMemory(frame, sizeof(*frame));
}

static bool EnsureFrame(Frame* frame, int width, int height)
{
    if (frame->bitmap != nullptr && frame->width == width && frame->height == height)
    {
        return true;
    }
    FreeFrame(frame);

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;   // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    frame->bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (frame->bitmap == nullptr || bits == nullptr)
    {
        FreeFrame(frame);
        return false;
    }
    frame->dc = CreateCompatibleDC(nullptr);
    if (frame->dc == nullptr)
    {
        FreeFrame(frame);
        return false;
    }
    SelectObject(frame->dc, frame->bitmap);
    frame->pixels = static_cast<BYTE*>(bits);
    frame->width = width;
    frame->height = height;
    return true;
}

// Only record windows that belong to this app (anything running out of our
// folder: Window.exe, Gpu.exe, Basic.exe, Initializer.exe). Alt-tabbing to
// another program pauses recording rather than filling the folder with it.
static bool WindowBelongsToApp(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == GetCurrentProcessId())
    {
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr)
    {
        return false;
    }
    char path[MAX_PATH] = {};
    DWORD chars = MAX_PATH;
    const BOOL ok = QueryFullProcessImageNameA(process, 0, path, &chars);
    CloseHandle(process);
    if (!ok)
    {
        return false;
    }
    return _strnicmp(path, g_appFolder, lstrlenA(g_appFolder)) == 0;
}

static bool ActiveWindowRect(HWND hwnd, RECT* rc)
{
    if (IsIconic(hwnd))
    {
        return false;
    }
    // Extended frame bounds exclude the invisible resize borders DWM adds.
    RECT bounds = {};
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds))))
    {
        *rc = bounds;
    }
    else if (!GetWindowRect(hwnd, rc))
    {
        return false;
    }
    return rc->right > rc->left && rc->bottom > rc->top;
}

#define RECORDER_FS_MAX 8

struct FullscreenDisplay
{
    HWND hwnd;
    RECT rc;
    char title[128];
    char className[64];
};

static bool WindowCoversMonitor(HWND hwnd, const RECT* rc)
{
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (monitor == nullptr)
    {
        return false;
    }
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoA(monitor, &info))
    {
        return false;
    }
    RECT overlap = {};
    if (!IntersectRect(&overlap, rc, &info.rcMonitor))
    {
        return false;
    }
    const LONG monitorArea =
        (info.rcMonitor.right - info.rcMonitor.left) * (info.rcMonitor.bottom - info.rcMonitor.top);
    if (monitorArea <= 0)
    {
        return false;
    }
    const LONG overlapArea =
        (overlap.right - overlap.left) * (overlap.bottom - overlap.top);
    return overlapArea * 100 / monitorArea >= 90;
}

static bool IsFullscreenDisplayWindow(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindowVisible(hwnd) || IsIconic(hwnd) || !WindowBelongsToApp(hwnd))
    {
        return false;
    }
    char className[64] = {};
    GetClassNameA(hwnd, className, sizeof(className));
    if (lstrcmpiA(className, "ManagedFullscreenWindow") == 0)
    {
        return true;
    }
    RECT rc = {};
    if (!ActiveWindowRect(hwnd, &rc))
    {
        return false;
    }
    return WindowCoversMonitor(hwnd, &rc);
}

struct CollectFsState
{
    FullscreenDisplay* items;
    int count;
};

static BOOL CALLBACK CollectFullscreenProc(HWND hwnd, LPARAM lParam)
{
    CollectFsState* state = reinterpret_cast<CollectFsState*>(lParam);
    if (state->count >= RECORDER_FS_MAX || !IsFullscreenDisplayWindow(hwnd))
    {
        return TRUE;
    }
    FullscreenDisplay* item = &state->items[state->count];
    ZeroMemory(item, sizeof(*item));
    item->hwnd = hwnd;
    if (!ActiveWindowRect(hwnd, &item->rc))
    {
        return TRUE;
    }
    GetWindowTextA(hwnd, item->title, sizeof(item->title));
    GetClassNameA(hwnd, item->className, sizeof(item->className));
    state->count += 1;
    return TRUE;
}

static int CollectFullscreenDisplays(FullscreenDisplay* out)
{
    CollectFsState state = { out, 0 };
    EnumWindows(CollectFullscreenProc, reinterpret_cast<LPARAM>(&state));
    return state.count;
}

// Number of visible top-level windows stacked above hwnd (0 = frontmost).
static int WindowsAbove(HWND hwnd)
{
    int above = 0;
    for (HWND h = GetWindow(hwnd, GW_HWNDPREV); h != nullptr; h = GetWindow(h, GW_HWNDPREV))
    {
        if (IsWindowVisible(h) && !IsIconic(h))
        {
            RECT r = {};
            if (GetWindowRect(h, &r) && r.right > r.left && r.bottom > r.top)
            {
                ++above;
            }
        }
    }
    return above;
}

struct MonitorIndexSearch
{
    HMONITOR target;
    int index;
    int found;
};

static BOOL CALLBACK MonitorIndexProc(HMONITOR monitor, HDC, LPRECT, LPARAM lParam)
{
    MonitorIndexSearch* search = reinterpret_cast<MonitorIndexSearch*>(lParam);
    ++search->index;
    if (monitor == search->target)
    {
        search->found = search->index;
    }
    return TRUE;
}

static void ProcessImageName(HWND hwnd, char* out, int outChars, DWORD* outPid)
{
    out[0] = '\0';
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    *outPid = pid;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr)
    {
        return;
    }
    char path[MAX_PATH] = {};
    DWORD chars = MAX_PATH;
    if (QueryFullProcessImageNameA(process, 0, path, &chars))
    {
        const char* slash = strrchr(path, '\\');
        lstrcpynA(out, slash != nullptr ? slash + 1 : path, outChars);
    }
    CloseHandle(process);
}

// Sidecar text: what the window was, how big, where, on which display.
// Gathered before the screen copy so a window closing mid-frame can't leave
// the fields blank.
static void AppendFullscreenNotes(char* text, size_t textChars)
{
    FullscreenDisplay displays[RECORDER_FS_MAX] = {};
    const int count = CollectFullscreenDisplays(displays);
    char extra[2048] = {};
    int used = _snprintf_s(extra, sizeof(extra), _TRUNCATE, "\r\nfullscreen displays: %d\r\n", count);
    for (int i = 0; i < count && used > 0 && used < static_cast<int>(sizeof(extra) - 8); ++i)
    {
        HMONITOR monitor = MonitorFromWindow(displays[i].hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEXA info = {};
        info.cbSize = sizeof(info);
        GetMonitorInfoA(monitor, &info);
        used += _snprintf_s(
            extra + used, sizeof(extra) - used, _TRUNCATE,
            "  [%d] hwnd=%p class=%s title=\"%s\" size=%ldx%ld pos=%ld,%ld-%ld,%ld display=%s\r\n",
            i,
            reinterpret_cast<void*>(displays[i].hwnd),
            displays[i].className,
            displays[i].title,
            displays[i].rc.right - displays[i].rc.left,
            displays[i].rc.bottom - displays[i].rc.top,
            displays[i].rc.left,
            displays[i].rc.top,
            displays[i].rc.right,
            displays[i].rc.bottom,
            info.szDevice
        );
    }
    if (used > 0)
    {
        const size_t already = lstrlenA(text);
        if (already + 1 < textChars)
        {
            lstrcpynA(text + already, extra, static_cast<int>(textChars - already));
        }
    }
}

static void BuildMetadata(HWND hwnd, const RECT& rc, const SYSTEMTIME& t, char* text, size_t textChars)
{
    char title[256] = {};
    GetWindowTextA(hwnd, title, sizeof(title));
    char className[128] = {};
    GetClassNameA(hwnd, className, sizeof(className));
    char exe[MAX_PATH] = {};
    DWORD pid = 0;
    ProcessImageName(hwnd, exe, MAX_PATH, &pid);

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    GetWindowPlacement(hwnd, &placement);
    const char* state = placement.showCmd == SW_SHOWMAXIMIZED ? "maximized"
        : (placement.showCmd == SW_SHOWMINIMIZED ? "minimized" : "normal");

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXA info = {};
    info.cbSize = sizeof(info);
    GetMonitorInfoA(monitor, &info);
    MonitorIndexSearch search = { monitor, 0, 0 };
    EnumDisplayMonitors(nullptr, nullptr, MonitorIndexProc, reinterpret_cast<LPARAM>(&search));
    const UINT dpi = GetDpiForWindow(hwnd);

    // How much of the window actually lies on that display.
    RECT onDisplay = {};
    LONG coverage = 0;
    if (IntersectRect(&onDisplay, &rc, &info.rcMonitor))
    {
        const LONG windowArea = (rc.right - rc.left) * (rc.bottom - rc.top);
        if (windowArea > 0)
        {
            coverage = (onDisplay.right - onDisplay.left) * (onDisplay.bottom - onDisplay.top) * 100 / windowArea;
        }
    }

    _snprintf_s(
        text, textChars, _TRUNCATE,
        "time: %04u-%02u-%02u %02u:%02u:%02u.%03u\r\n"
        "window: \"%s\"\r\n"
        "class: %s\r\n"
        "process: %s (pid %lu)\r\n"
        "state: %s\r\n"
        "\r\n"
        "size: %ld x %ld  (width x height, physical pixels)\r\n"
        "z-order: %d  (visible top-level windows stacked above it; 0 = frontmost)\r\n"
        "\r\n"
        "position (virtual desktop): left=%ld top=%ld right=%ld bottom=%ld\r\n"
        "\r\n"
        "display: %d of %d  %s%s\r\n"
        "display bounds: left=%ld top=%ld right=%ld bottom=%ld  (%ld x %ld)\r\n"
        "display work area: left=%ld top=%ld right=%ld bottom=%ld\r\n"
        "display scale: %u%% (%u dpi)\r\n"
        "position on display: x=%ld y=%ld  (window top-left relative to display top-left)\r\n"
        "on this display: %ld%% of the window\r\n",
        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
        title, className, exe, static_cast<unsigned long>(pid), state,
        rc.right - rc.left, rc.bottom - rc.top,
        WindowsAbove(hwnd),
        rc.left, rc.top, rc.right, rc.bottom,
        search.found, search.index, info.szDevice,
        (info.dwFlags & MONITORINFOF_PRIMARY) ? " (primary)" : "",
        info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right, info.rcMonitor.bottom,
        info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top,
        info.rcWork.left, info.rcWork.top, info.rcWork.right, info.rcWork.bottom,
        dpi * 100 / 96, dpi,
        rc.left - info.rcMonitor.left, rc.top - info.rcMonitor.top,
        coverage
    );
    AppendFullscreenNotes(text, textChars);
}

static void WriteSidecar(const wchar_t* pngPath, const char* text)
{
    wchar_t txtPath[MAX_PATH] = {};
    lstrcpynW(txtPath, pngPath, MAX_PATH);
    wchar_t* dot = wcsrchr(txtPath, L'.');
    if (dot == nullptr)
    {
        return;
    }
    lstrcpyW(dot, L".txt");

    HANDLE file = CreateFileW(txtPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    CloseHandle(file);
}

static bool SavePng(const Frame* frame, const wchar_t* path)
{
    Gdiplus::Bitmap bitmap(frame->width, frame->height, frame->width * 4, PixelFormat32bppRGB, frame->pixels);
    static const CLSID pngEncoder =
        { 0x557CF406, 0x1A04, 0x11D3, { 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E } };
    return bitmap.Save(path, &pngEncoder, nullptr) == Gdiplus::Ok;
}

// ---- housekeeping -----------------------------------------------------------

static bool IsSessionName(const char* name)
{
    // YYYYMMDD_HHMMSS — only folders we created are ever touched.
    if (lstrlenA(name) != 15 || name[8] != '_')
    {
        return false;
    }
    for (int i = 0; i < 15; ++i)
    {
        if (i != 8 && (name[i] < '0' || name[i] > '9'))
        {
            return false;
        }
    }
    return true;
}

static ULONGLONG FolderBytes(const char* folder)
{
    char pattern[MAX_PATH] = {};
    wsprintfA(pattern, "%s\\*", folder);
    WIN32_FIND_DATAA data = {};
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    ULONGLONG total = 0;
    do
    {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (lstrcmpA(data.cFileName, ".") != 0 && lstrcmpA(data.cFileName, "..") != 0)
            {
                char sub[MAX_PATH] = {};
                wsprintfA(sub, "%s\\%s", folder, data.cFileName);
                total += FolderBytes(sub);
            }
        }
        else
        {
            total += (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    return total;
}

// Deletes one of our session folders (flat: png/txt files only).
static void DeleteSessionFolder(const char* folder)
{
    char pattern[MAX_PATH] = {};
    wsprintfA(pattern, "%s\\*", folder);
    WIN32_FIND_DATAA data = {};
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                char file[MAX_PATH] = {};
                wsprintfA(file, "%s\\%s", folder, data.cFileName);
                DeleteFileA(file);
            }
        } while (FindNextFileA(find, &data));
        FindClose(find);
    }
    RemoveDirectoryA(folder);
}

static int CompareNames(const void* a, const void* b)
{
    return lstrcmpA(static_cast<const char*>(a), static_cast<const char*>(b));
}

// Keep only the newest `keep` session folders under root.
static void PruneOldSessions(const char* root, int keep)
{
    static char names[256][16];
    int count = 0;
    char pattern[MAX_PATH] = {};
    wsprintfA(pattern, "%s*", root);
    WIN32_FIND_DATAA data = {};
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && IsSessionName(data.cFileName) && count < 256)
        {
            lstrcpynA(names[count++], data.cFileName, 16);
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);

    qsort(names, count, sizeof(names[0]), CompareNames);   // oldest first
    for (int i = 0; i < count - keep; ++i)
    {
        char folder[MAX_PATH] = {};
        wsprintfA(folder, "%s%s", root, names[i]);
        DeleteSessionFolder(folder);
    }
}

static void WriteTextFile(const char* path, const char* text)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    CloseHandle(file);
}

static void WriteStopped(const char* reason)
{
    char path[MAX_PATH] = {};
    wsprintfA(path, "%sSTOPPED.txt", g_sessionFolder);
    char text[512] = {};
    _snprintf_s(
        text, sizeof(text), _TRUNCATE,
        "Recording stopped: %s\r\n"
        "Session wrote %llu MB; all kept sessions total %llu MB.\r\n"
        "Limits: %llu min, %llu MB per session, %llu MB total, %llu GB free space.\r\n",
        reason,
        g_sessionBytes / (1024 * 1024),
        (g_sessionBytes + g_olderSessionsBytes) / (1024 * 1024),
        RECORDER_MAX_SESSION_MS / 60000,
        RECORDER_MAX_SESSION_BYTES / (1024 * 1024),
        RECORDER_MAX_TOTAL_BYTES / (1024 * 1024),
        RECORDER_MIN_FREE_BYTES / (1024 * 1024 * 1024)
    );
    WriteTextFile(path, text);
}

// Returns a reason string if a limit has been hit, nullptr otherwise.
static const char* LimitHit()
{
    if (GetTickCount64() - g_sessionStartTick > RECORDER_MAX_SESSION_MS)
    {
        return "time limit reached";
    }
    if (g_sessionBytes > RECORDER_MAX_SESSION_BYTES)
    {
        return "session size limit reached";
    }
    if (g_sessionBytes + g_olderSessionsBytes > RECORDER_MAX_TOTAL_BYTES)
    {
        return "total screenshots folder size limit reached";
    }
    ULARGE_INTEGER freeBytes = {};
    if (GetDiskFreeSpaceExA(g_cfg.root, &freeBytes, nullptr, nullptr) &&
        freeBytes.QuadPart < RECORDER_MIN_FREE_BYTES)
    {
        return "drive free space is below the safety floor";
    }
    return nullptr;
}

static ULONGLONG FileBytes(const wchar_t* path)
{
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data))
    {
        return 0;
    }
    return (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

static void EnsureTrailingSlash(char* path)
{
    const int n = lstrlenA(path);
    if (n <= 0 || n + 1 >= MAX_PATH)
    {
        return;
    }
    if (path[n - 1] != '\\' && path[n - 1] != '/')
    {
        path[n] = '\\';
        path[n + 1] = '\0';
    }
}

static bool DirectoryExists(const char* path)
{
    char folder[MAX_PATH] = {};
    lstrcpynA(folder, path, MAX_PATH);
    const int n = lstrlenA(folder);
    if (n > 1 && (folder[n - 1] == '\\' || folder[n - 1] == '/'))
    {
        folder[n - 1] = '\0';
    }
    const DWORD attrs = GetFileAttributesA(folder);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static void ReadConfig(RecConfig* cfg)
{
    cfg->on = false;
    cfg->intervalMs = RECORDER_RATE_MS[0];
    cfg->keep = RECORDER_KEEP_N[1];
    lstrcpynA(cfg->root, g_appFolder, MAX_PATH);
    lstrcatA(cfg->root, "screenshots\\");

    if (!Store_Open())
    {
        cfg->on = DirectoryExists(cfg->root);
        return;
    }

    char path[MAX_PATH] = {};
    Store_GetString("recording.path", path, MAX_PATH, "");
    char onText[8] = {};
    Store_GetString("recording.on", onText, sizeof(onText), "");
    int rate = Store_GetInt("recording.rate", 0);
    int keep = Store_GetInt("recording.keep", 1);
    Store_Close();

    if (path[0] != '\0')
    {
        lstrcpynA(cfg->root, path, MAX_PATH);
        EnsureTrailingSlash(cfg->root);
    }
    if (rate < 0 || rate >= (int)(sizeof(RECORDER_RATE_MS) / sizeof(RECORDER_RATE_MS[0])))
    {
        rate = 0;
    }
    if (keep < 0 || keep >= (int)(sizeof(RECORDER_KEEP_N) / sizeof(RECORDER_KEEP_N[0])))
    {
        keep = 1;
    }
    cfg->intervalMs = RECORDER_RATE_MS[rate];
    cfg->keep = RECORDER_KEEP_N[keep];
    if (onText[0] == '\0')
    {
        cfg->on = DirectoryExists(cfg->root);
    }
    else
    {
        cfg->on = atoi(onText) != 0;
    }
}

static bool BeginSession()
{
    char root[MAX_PATH] = {};
    lstrcpynA(root, g_cfg.root, MAX_PATH);
    const int n = lstrlenA(root);
    if (n > 1 && root[n - 1] == '\\')
    {
        root[n - 1] = '\0';
    }
    SHCreateDirectoryExA(nullptr, root, nullptr);
    PruneOldSessions(g_cfg.root, g_cfg.keep > 0 ? g_cfg.keep - 1 : 0);

    char rootNoSlash[MAX_PATH] = {};
    lstrcpynA(rootNoSlash, g_cfg.root, MAX_PATH);
    rootNoSlash[lstrlenA(rootNoSlash) - 1] = '\0';
    g_olderSessionsBytes = FolderBytes(rootNoSlash);
    g_sessionBytes = 0;

    SYSTEMTIME t = {};
    GetLocalTime(&t);
    wsprintfA(
        g_sessionFolder, "%s%04u%02u%02u_%02u%02u%02u\\",
        g_cfg.root, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond
    );
    if (!CreateDirectoryA(g_sessionFolder, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        g_sessionFolder[0] = '\0';
        return false;
    }
    g_sessionStartTick = GetTickCount64();
    InterlockedExchange(&g_resetFrames, 1);
    return true;
}

static void RefreshRecording()
{
    RecConfig next = {};
    ReadConfig(&next);
    const bool pathChanged = lstrcmpiA(next.root, g_cfg.root) != 0;
    const bool wasOn = g_cfg.on;
    g_cfg = next;
    if (!g_cfg.on)
    {
        g_sessionOpen = false;
        return;
    }
    if (!wasOn || pathChanged || !g_sessionOpen)
    {
        g_sessionOpen = BeginSession();
        return;
    }
    PruneOldSessions(g_cfg.root, g_cfg.keep > 0 ? g_cfg.keep - 1 : 0);
}

// ---- capture loop -------------------------------------------------------------

static DWORD WINAPI RecorderThread(LPVOID)
{
    ULONG_PTR gdiToken = 0;
    Gdiplus::GdiplusStartupInput startup;
    if (Gdiplus::GdiplusStartup(&gdiToken, &startup, nullptr) != Gdiplus::Ok)
    {
        return 0;
    }

    Frame frames[2] = {};
    Frame fsFrame = {};
    int current = 0;
    bool haveSaved = false;
    RECT lastRect = {};
    HWND lastFsHwnd[RECORDER_FS_MAX] = {};
    int lastFsCount = 0;
    if (g_cfg.on)
    {
        g_sessionOpen = BeginSession();
    }

    ULONGLONG next = GetTickCount64();
    HANDLE waits[2] = { g_recorderStop, g_reload };

    for (;;)
    {
        if (InterlockedCompareExchange(&g_resetFrames, 0, 1) == 1)
        {
            haveSaved = false;
            lastFsCount = 0;
        }

        DWORD wait = INFINITE;
        if (g_cfg.on)
        {
            const DWORD interval = g_cfg.intervalMs < 50 ? 50 : g_cfg.intervalMs;
            next += interval;
            const ULONGLONG now = GetTickCount64();
            if (now > next + 1000)
            {
                next = now;
            }
            wait = next > now ? static_cast<DWORD>(next - now) : 0;
        }
        const DWORD woke = WaitForMultipleObjects(2, waits, FALSE, wait);
        if (woke == WAIT_OBJECT_0)
        {
            break;
        }
        if (woke == WAIT_OBJECT_0 + 1)
        {
            RefreshRecording();
            next = GetTickCount64();
        }
        if (!g_cfg.on || !g_sessionOpen)
        {
            continue;
        }

        const char* limit = LimitHit();
        if (limit != nullptr)
        {
            WriteStopped(limit);
            break;
        }

        // Folder removed mid-session = recording switched off.
        const DWORD attrs = GetFileAttributesA(g_sessionFolder);
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            continue;
        }

        FullscreenDisplay displays[RECORDER_FS_MAX] = {};
        const int displayCount = CollectFullscreenDisplays(displays);

        HWND active = GetForegroundWindow();
        RECT rc = {};
        bool haveActive = active != nullptr && WindowBelongsToApp(active) && ActiveWindowRect(active, &rc);
        if (!haveActive && displayCount <= 0)
        {
            continue;
        }
        if (!haveActive)
        {
            active = displays[0].hwnd;
            rc = displays[0].rc;
            haveActive = true;
        }

        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;

        SYSTEMTIME t = {};
        GetLocalTime(&t);
        char metadata[4096] = {};
        BuildMetadata(active, rc, t, metadata, sizeof(metadata));

        Frame* frame = &frames[current];
        if (!EnsureFrame(frame, width, height))
        {
            continue;
        }
        HDC screen = GetDC(nullptr);
        if (screen == nullptr)
        {
            continue;
        }
        // Screen copy (not PrintWindow) so popup menus and message boxes over
        // the window are in the frame too. CAPTUREBLT includes layered windows.
        const BOOL blitted = BitBlt(frame->dc, 0, 0, width, height, screen, rc.left, rc.top, SRCCOPY | CAPTUREBLT);
        ReleaseDC(nullptr, screen);
        if (!blitted)
        {
            continue;
        }
        GdiFlush();

        // Skip frames identical to the last one written (same pixels, same
        // place on screen); timestamps in the filenames preserve the timeline,
        // and an idle UI costs nothing on disk.
        const Frame* last = &frames[current ^ 1];
        const bool unchanged = haveSaved && EqualRect(&lastRect, &rc) &&
            last->width == width && last->height == height &&
            memcmp(last->pixels, frame->pixels, static_cast<size_t>(width) * height * 4) == 0;
        bool fsSetChanged = lastFsCount != displayCount;
        if (!fsSetChanged)
        {
            for (int i = 0; i < displayCount; ++i)
            {
                if (lastFsHwnd[i] != displays[i].hwnd)
                {
                    fsSetChanged = true;
                    break;
                }
            }
        }
        if (unchanged && !fsSetChanged)
        {
            continue;
        }

        wchar_t path[MAX_PATH] = {};
        {
            swprintf_s(
                path, L"%S%02u%02u%02u_%03u.png",
                g_sessionFolder, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
            );
            if (SavePng(frame, path))
            {
                WriteSidecar(path, metadata);
                g_sessionBytes += FileBytes(path) + lstrlenA(metadata);
                current ^= 1;
                haveSaved = true;
                lastRect = rc;
            }
        }

        for (int i = 0; i < displayCount; ++i)
        {
            if (displays[i].hwnd == active)
            {
                continue;
            }
            const int fsW = displays[i].rc.right - displays[i].rc.left;
            const int fsH = displays[i].rc.bottom - displays[i].rc.top;
            if (fsW <= 0 || fsH <= 0 || !EnsureFrame(&fsFrame, fsW, fsH))
            {
                continue;
            }
            HDC fsScreen = GetDC(nullptr);
            if (fsScreen == nullptr)
            {
                continue;
            }
            const BOOL fsBlit = BitBlt(
                fsFrame.dc, 0, 0, fsW, fsH,
                fsScreen, displays[i].rc.left, displays[i].rc.top, SRCCOPY | CAPTUREBLT
            );
            ReleaseDC(nullptr, fsScreen);
            if (!fsBlit)
            {
                continue;
            }
            GdiFlush();

            char fsMeta[4096] = {};
            BuildMetadata(displays[i].hwnd, displays[i].rc, t, fsMeta, sizeof(fsMeta));
            wchar_t fsPath[MAX_PATH] = {};
            swprintf_s(
                fsPath, L"%S%02u%02u%02u_%03u_fs%d.png",
                g_sessionFolder, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, i
            );
            if (SavePng(&fsFrame, fsPath))
            {
                WriteSidecar(fsPath, fsMeta);
                g_sessionBytes += FileBytes(fsPath) + lstrlenA(fsMeta);
            }
        }
        lastFsCount = displayCount;
        ZeroMemory(lastFsHwnd, sizeof(lastFsHwnd));
        for (int i = 0; i < displayCount; ++i)
        {
            lastFsHwnd[i] = displays[i].hwnd;
        }
    }

    FreeFrame(&frames[0]);
    FreeFrame(&frames[1]);
    FreeFrame(&fsFrame);
    Gdiplus::GdiplusShutdown(gdiToken);
    return 0;
}

bool Recorder_Start()
{
    if (g_recorderThread != nullptr)
    {
        return true;
    }
    if (GetModuleFileNameA(nullptr, g_appFolder, MAX_PATH) == 0)
    {
        return false;
    }
    char* slash = strrchr(g_appFolder, '\\');
    if (slash == nullptr)
    {
        return false;
    }
    slash[1] = '\0';

    ReadConfig(&g_cfg);

    g_recorderStop = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    g_reload = CreateEventA(nullptr, FALSE, FALSE, RECORDER_RELOAD_EVENT);
    if (g_recorderStop == nullptr || g_reload == nullptr)
    {
        if (g_recorderStop != nullptr)
        {
            CloseHandle(g_recorderStop);
            g_recorderStop = nullptr;
        }
        if (g_reload != nullptr)
        {
            CloseHandle(g_reload);
            g_reload = nullptr;
        }
        return false;
    }
    g_recorderThread = CreateThread(nullptr, 0, RecorderThread, nullptr, 0, nullptr);
    if (g_recorderThread == nullptr)
    {
        CloseHandle(g_recorderStop);
        CloseHandle(g_reload);
        g_recorderStop = nullptr;
        g_reload = nullptr;
        return false;
    }
    return true;
}

void Recorder_Stop()
{
    if (g_recorderThread == nullptr)
    {
        return;
    }
    SetEvent(g_recorderStop);
    WaitForSingleObject(g_recorderThread, 3000);
    CloseHandle(g_recorderThread);
    CloseHandle(g_recorderStop);
    CloseHandle(g_reload);
    g_recorderThread = nullptr;
    g_recorderStop = nullptr;
    g_reload = nullptr;
}
