#include "gpu_client.h"

#include <string.h>

static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static HANDLE g_statusMap = nullptr;
static const GpuPlaybackStatus* g_statusView = nullptr;
static unsigned long long g_lastTargets[GPU_MAX_TARGETS] = {};
static unsigned g_lastTargetCount = 0;
static GpuOutputTarget g_lastOutputs[GPU_MAX_TARGETS] = {};
static int g_lastOutputCount = 0;
static wchar_t g_lastPath[MAX_PATH] = {};
static int g_lastMode = -1;
static bool g_hasPlay = false;
static HANDLE g_overlayMap = nullptr;
static GpuOverlayHeader* g_overlayView = nullptr;
static wchar_t g_preloadSent[GPU_PRELOAD_MAX][MAX_PATH] = {};
static int g_preloadSentCount = -1;

static bool Send(const GpuMessage* message)
{
    if (g_pipe == INVALID_HANDLE_VALUE || message == nullptr)
    {
        return false;
    }

    DWORD written = 0;
    return WriteFile(g_pipe, message, sizeof(*message), &written, nullptr) == TRUE &&
        written == sizeof(*message);
}

static void OpenStatusMap()
{
    if (g_statusView != nullptr)
    {
        return;
    }

    g_statusMap = OpenFileMappingW(FILE_MAP_READ, FALSE, GPU_STATUS_MAP_NAME);
    if (g_statusMap == nullptr)
    {
        return;
    }

    g_statusView = static_cast<const GpuPlaybackStatus*>(
        MapViewOfFile(g_statusMap, FILE_MAP_READ, 0, 0, sizeof(GpuPlaybackStatus))
    );
}

static void CloseStatusMap()
{
    if (g_statusView != nullptr)
    {
        UnmapViewOfFile(g_statusView);
        g_statusView = nullptr;
    }
    if (g_statusMap != nullptr)
    {
        CloseHandle(g_statusMap);
        g_statusMap = nullptr;
    }
}

static void CloseOverlayMap()
{
    if (g_overlayView != nullptr)
    {
        UnmapViewOfFile(g_overlayView);
        g_overlayView = nullptr;
    }
    if (g_overlayMap != nullptr)
    {
        CloseHandle(g_overlayMap);
        g_overlayMap = nullptr;
    }
}

static bool EnsureOverlayMap()
{
    if (g_overlayView != nullptr)
    {
        return true;
    }

    g_overlayMap = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(GPU_OVERLAY_MAP_BYTES),
        GPU_OVERLAY_MAP_NAME
    );
    if (g_overlayMap == nullptr)
    {
        return false;
    }

    g_overlayView = static_cast<GpuOverlayHeader*>(
        MapViewOfFile(g_overlayMap, FILE_MAP_WRITE, 0, 0, GPU_OVERLAY_MAP_BYTES)
    );
    if (g_overlayView == nullptr)
    {
        CloseHandle(g_overlayMap);
        g_overlayMap = nullptr;
        return false;
    }
    ZeroMemory(g_overlayView, GPU_OVERLAY_MAP_BYTES);
    return true;
}

static void ClearPlayCache()
{
    ZeroMemory(g_lastTargets, sizeof(g_lastTargets));
    ZeroMemory(g_lastOutputs, sizeof(g_lastOutputs));
    g_lastTargetCount = 0;
    g_lastOutputCount = 0;
    g_lastPath[0] = L'\0';
    g_lastMode = -1;
    g_hasPlay = false;
}

bool GpuClient_Connect()
{
    if (g_pipe != INVALID_HANDLE_VALUE)
    {
        return true;
    }

    for (int i = 0; i < 40; ++i)
    {
        g_pipe = CreateFileA(
            GPU_PIPE_NAME,
            GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        if (g_pipe != INVALID_HANDLE_VALUE)
        {
            DWORD mode = PIPE_READMODE_MESSAGE;
            SetNamedPipeHandleState(g_pipe, &mode, nullptr, nullptr);
            OpenStatusMap();
            ClearPlayCache();
            g_preloadSentCount = -1;
            return true;
        }
        Sleep(50);
    }

    return false;
}

void GpuClient_Disconnect()
{
    if (g_pipe != INVALID_HANDLE_VALUE)
    {
        GpuMessage quit = {};
        quit.cmd = GPU_CMD_QUIT;
        Send(&quit);
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }
    CloseStatusMap();
    CloseOverlayMap();
    ClearPlayCache();
}

bool GpuClient_Ready()
{
    return g_pipe != INVALID_HANDLE_VALUE;
}

bool GpuClient_OwnsOutputs()
{
    return GpuClient_Ready() && g_hasPlay;
}

void GpuClient_Stop()
{
    GpuPlaybackStatus status = {};
    const bool mediaOn = GpuClient_GetStatus(&status) && status.hasMedia != 0;
    if (!g_hasPlay && !mediaOn)
    {
        return;
    }

    GpuMessage message = {};
    message.cmd = GPU_CMD_STOP;
    Send(&message);
    ClearPlayCache();

    for (int i = 0; i < 20; ++i)
    {
        GpuPlaybackStatus wait = {};
        if (!GpuClient_GetStatus(&wait) || wait.hasMedia == 0)
        {
            break;
        }
        Sleep(5);
    }
}

void GpuClient_PlayOutputs(const GpuOutputTarget* outputs, int outputCount, const GpuLayer* layers, int layerCount)
{
    if (!GpuClient_Ready() || layers == nullptr || layerCount <= 0 || outputs == nullptr || outputCount <= 0)
    {
        return;
    }

    OpenStatusMap();

    GpuMessage message = {};
    message.cmd = GPU_CMD_PLAY;
    message.volume = 1.0;
    const int count = outputCount < GPU_MAX_TARGETS ? outputCount : GPU_MAX_TARGETS;
    for (int i = 0; i < count; ++i)
    {
        if (outputs[i].hwnd == nullptr || !IsWindow(outputs[i].hwnd))
        {
            continue;
        }
        const unsigned slot = message.targetCount;
        message.targets[slot] = static_cast<unsigned long long>(
            reinterpret_cast<UINT_PTR>(outputs[i].hwnd)
        );
        message.srcX[slot] = outputs[i].srcX;
        message.srcY[slot] = outputs[i].srcY;
        message.srcW[slot] = outputs[i].srcW;
        message.srcH[slot] = outputs[i].srcH;
        message.ovX[slot] = outputs[i].ovX;
        message.ovY[slot] = outputs[i].ovY;
        message.ovW[slot] = outputs[i].ovW;
        message.ovH[slot] = outputs[i].ovH;
        message.videoOn[slot] = outputs[i].videoOn ? 1 : 0;
        message.overlayOn[slot] = outputs[i].overlayOn ? 1 : 0;
        message.paneIndex[slot] = outputs[i].paneIndex;
        message.paneCount[slot] = outputs[i].paneCount > 0 ? outputs[i].paneCount : 1;
        message.targetCount += 1;
    }

    const int layerN = layerCount < GPU_MAX_LAYERS ? layerCount : GPU_MAX_LAYERS;
    for (int i = 0; i < layerN; ++i)
    {
        message.layers[message.layerCount] = layers[i];
        message.layerCount += 1;
    }

    if (message.targetCount == 0 || message.layerCount == 0)
    {
        return;
    }

    const bool samePath = g_hasPlay && lstrcmpiW(g_lastPath, message.layers[0].path) == 0;
    const bool sameMode = g_hasPlay && g_lastMode == message.layers[0].mode;
    bool sameTargets = g_hasPlay && g_lastOutputCount == static_cast<int>(message.targetCount);
    if (sameTargets)
    {
        for (unsigned i = 0; i < message.targetCount; ++i)
        {
            if (g_lastOutputs[i].hwnd != reinterpret_cast<HWND>(static_cast<UINT_PTR>(message.targets[i])) ||
                g_lastOutputs[i].srcX != message.srcX[i] ||
                g_lastOutputs[i].srcY != message.srcY[i] ||
                g_lastOutputs[i].srcW != message.srcW[i] ||
                g_lastOutputs[i].srcH != message.srcH[i] ||
                g_lastOutputs[i].ovX != message.ovX[i] ||
                g_lastOutputs[i].ovY != message.ovY[i] ||
                g_lastOutputs[i].ovW != message.ovW[i] ||
                g_lastOutputs[i].ovH != message.ovH[i] ||
                g_lastOutputs[i].videoOn != message.videoOn[i] ||
                g_lastOutputs[i].overlayOn != message.overlayOn[i] ||
                g_lastOutputs[i].paneIndex != message.paneIndex[i] ||
                g_lastOutputs[i].paneCount != message.paneCount[i])
            {
                sameTargets = false;
                break;
            }
        }
    }

    if (samePath && sameMode && sameTargets)
    {
        return;
    }

    if (Send(&message))
    {
        ZeroMemory(g_lastOutputs, sizeof(g_lastOutputs));
        ZeroMemory(g_lastTargets, sizeof(g_lastTargets));
        g_lastOutputCount = static_cast<int>(message.targetCount);
        g_lastTargetCount = message.targetCount;
        for (unsigned i = 0; i < message.targetCount; ++i)
        {
            g_lastTargets[i] = message.targets[i];
            g_lastOutputs[i].hwnd = reinterpret_cast<HWND>(static_cast<UINT_PTR>(message.targets[i]));
            g_lastOutputs[i].srcX = message.srcX[i];
            g_lastOutputs[i].srcY = message.srcY[i];
            g_lastOutputs[i].srcW = message.srcW[i];
            g_lastOutputs[i].srcH = message.srcH[i];
            g_lastOutputs[i].ovX = message.ovX[i];
            g_lastOutputs[i].ovY = message.ovY[i];
            g_lastOutputs[i].ovW = message.ovW[i];
            g_lastOutputs[i].ovH = message.ovH[i];
            g_lastOutputs[i].videoOn = message.videoOn[i];
            g_lastOutputs[i].overlayOn = message.overlayOn[i];
            g_lastOutputs[i].paneIndex = message.paneIndex[i];
            g_lastOutputs[i].paneCount = message.paneCount[i];
        }
        lstrcpynW(g_lastPath, message.layers[0].path, MAX_PATH);
        g_lastMode = message.layers[0].mode;
        g_hasPlay = true;
    }
}

void GpuClient_Play(HWND* targets, int targetCount, const GpuLayer* layers, int layerCount)
{
    if (targets == nullptr || targetCount <= 0)
    {
        return;
    }

    GpuOutputTarget outputs[GPU_MAX_TARGETS] = {};
    const int count = targetCount < GPU_MAX_TARGETS ? targetCount : GPU_MAX_TARGETS;
    int filled = 0;
    for (int i = 0; i < count; ++i)
    {
        if (targets[i] == nullptr || !IsWindow(targets[i]))
        {
            continue;
        }
        outputs[filled].hwnd = targets[i];
        outputs[filled].videoOn = 1;
        outputs[filled].overlayOn = 1;
        filled += 1;
    }
    GpuClient_PlayOutputs(outputs, filled, layers, layerCount);
}

void GpuClient_Pause()
{
    GpuMessage message = {};
    message.cmd = GPU_CMD_PAUSE;
    Send(&message);
}

void GpuClient_Resume()
{
    GpuMessage message = {};
    message.cmd = GPU_CMD_RESUME;
    Send(&message);
}

void GpuClient_Seek(double seconds)
{
    GpuMessage message = {};
    message.cmd = GPU_CMD_SEEK;
    message.seekTime = seconds;
    Send(&message);
}

void GpuClient_Preload(const wchar_t (*paths)[MAX_PATH], int count)
{
    if (!GpuClient_Ready() || paths == nullptr || count < 0)
    {
        return;
    }
    if (count > GPU_PRELOAD_MAX)
    {
        count = GPU_PRELOAD_MAX;
    }
    if (count == g_preloadSentCount)
    {
        bool same = true;
        for (int i = 0; i < count && same; ++i)
        {
            same = lstrcmpiW(paths[i], g_preloadSent[i]) == 0;
        }
        if (same)
        {
            return;
        }
    }

    GpuMessage begin = {};
    begin.cmd = GPU_CMD_PRELOAD_BEGIN;
    if (!Send(&begin))
    {
        return;
    }
    for (int i = 0; i < count; i += GPU_MAX_LAYERS)
    {
        GpuMessage batch = {};
        batch.cmd = GPU_CMD_PRELOAD;
        for (int j = i; j < count && batch.layerCount < GPU_MAX_LAYERS; ++j)
        {
            lstrcpynW(batch.layers[batch.layerCount].path, paths[j], MAX_PATH);
            batch.layerCount += 1;
        }
        if (!Send(&batch))
        {
            return;
        }
    }
    GpuMessage end = {};
    end.cmd = GPU_CMD_PRELOAD_END;
    if (!Send(&end))
    {
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        lstrcpynW(g_preloadSent[i], paths[i], MAX_PATH);
    }
    g_preloadSentCount = count;
}

void GpuClient_SetAudio(bool muted, double volume)
{
    GpuMessage message = {};
    message.cmd = GPU_CMD_SET_AUDIO;
    message.muted = muted ? 1 : 0;
    message.volume = volume;
    Send(&message);
}

bool GpuClient_GetStatus(GpuPlaybackStatus* out)
{
    if (out == nullptr)
    {
        return false;
    }

    OpenStatusMap();
    if (g_statusView == nullptr)
    {
        ZeroMemory(out, sizeof(*out));
        return false;
    }

    for (int i = 0; i < 8; ++i)
    {
        const LONG a = g_statusView->sequence;
        *out = *g_statusView;
        const LONG b = g_statusView->sequence;
        if ((a & 1) == 0 && a == b)
        {
            return out->hasMedia != 0;
        }
    }

    *out = *g_statusView;
    return out->hasMedia != 0;
}

static void OverlayBeginWrite()
{
    InterlockedIncrement(&g_overlayView->generation);
}

static void OverlayEndWrite(int width, int height, int active)
{
    g_overlayView->width = width;
    g_overlayView->height = height;
    g_overlayView->active = active;
    InterlockedIncrement(&g_overlayView->generation);
}

void GpuClient_Repaint()
{
    if (!EnsureOverlayMap() || g_overlayView == nullptr)
    {
        return;
    }
    OverlayBeginWrite();
    OverlayEndWrite(g_overlayView->width, g_overlayView->height, g_overlayView->active);
}

void GpuClient_SetSlideOverlay(HBITMAP frame, int width, int height, bool active)
{
    if (!EnsureOverlayMap() || g_overlayView == nullptr)
    {
        return;
    }

    if (!active || frame == nullptr || width <= 0 || height <= 0)
    {
        OverlayBeginWrite();
        OverlayEndWrite(0, 0, 0);
        return;
    }

    int copyW = width;
    int copyH = height;
    if (copyW > GPU_OVERLAY_MAX_W)
    {
        copyW = GPU_OVERLAY_MAX_W;
    }
    if (copyH > GPU_OVERLAY_MAX_H)
    {
        copyH = GPU_OVERLAY_MAX_H;
    }

    OverlayBeginWrite();
    unsigned char* bits = reinterpret_cast<unsigned char*>(g_overlayView + 1);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = copyW;
    info.bmiHeader.biHeight = -copyH;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    const int got = GetDIBits(hdc, frame, 0, copyH, bits, &info, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);
    if (got <= 0)
    {
        OverlayEndWrite(0, 0, 0);
        return;
    }

    // Frames arrive alpha-sealed; an all-zero alpha means fully transparent.
    OverlayEndWrite(copyW, copyH, 1);
}

void GpuClient_SetSlideOverlayBits(const unsigned char* src, int width, int height, bool active)
{
    if (!EnsureOverlayMap() || g_overlayView == nullptr)
    {
        return;
    }

    if (!active || src == nullptr || width <= 0 || height <= 0)
    {
        OverlayBeginWrite();
        OverlayEndWrite(0, 0, 0);
        return;
    }

    int copyW = width;
    int copyH = height;
    if (copyW > GPU_OVERLAY_MAX_W)
    {
        copyW = GPU_OVERLAY_MAX_W;
    }
    if (copyH > GPU_OVERLAY_MAX_H)
    {
        copyH = GPU_OVERLAY_MAX_H;
    }

    OverlayBeginWrite();
    unsigned char* dest = reinterpret_cast<unsigned char*>(g_overlayView + 1);
    if (copyW == width)
    {
        memcpy(dest, src, static_cast<size_t>(copyW) * static_cast<size_t>(copyH) * 4);
    }
    else
    {
        for (int y = 0; y < copyH; ++y)
        {
            memcpy(dest + (static_cast<size_t>(y) * copyW * 4), src + (static_cast<size_t>(y) * width * 4), static_cast<size_t>(copyW) * 4);
        }
    }
    OverlayEndWrite(copyW, copyH, 1);
}





