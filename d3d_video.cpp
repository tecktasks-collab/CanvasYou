#include "d3d_video.h"
#include "video_cache.h"

#include <d3d11.h>
#include <d3d10.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <share.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")

struct GpuTarget
{
    HWND hwnd;
    int width;
    int height;
    int srcX;
    int srcY;
    int srcW;
    int srcH;
    int ovX;
    int ovY;
    int ovW;
    int ovH;
    int videoOn;
    int overlayOn;
    int paneIndex;
    int paneCount;
};

static bool g_wantPlay = false;
static bool g_seeking = false;
static double g_seekTarget = 0.0;
static DWORD g_seekStartedMs = 0;
static bool g_forceFrame = false;
// Until a new source decodes its first frame, the glass keeps its last picture
// so slide content never lands on a black video.
static bool g_sourceHasFrame = false;
static DWORD g_sourceSetMs = 0;
#define GPU_FIRST_FRAME_WAIT_MS 1500

class EngineNotify;
static void LogGpu(const char* text);
static DWORD g_playStartMs = 0;

// One media engine per video. Players for the open project's videos stay
// loaded and paused at the start, so a click only has to press play.
struct Player
{
    IMFMediaEngine* engine;
    IMFMediaEngineEx* ex;
    EngineNotify* notify;
    wchar_t path[MAX_PATH];
    int wantRank;
    DWORD lastUsed;
};

#define GPU_MAX_PLAYERS 8
static Player g_players[GPU_MAX_PLAYERS] = {};
static Player* volatile g_active = nullptr;

class EngineNotify : public IMFMediaEngineNotify
{
public:
    long refs;
    Player* owner;

    explicit EngineNotify(Player* player) : refs(1), owner(player) {}

    STDMETHODIMP QueryInterface(REFIID id, void** out)
    {
        if (out == nullptr)
        {
            return E_POINTER;
        }
        if (id == IID_IUnknown || id == IID_IMFMediaEngineNotify)
        {
            *out = static_cast<IMFMediaEngineNotify*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&refs);
    }

    STDMETHODIMP_(ULONG) Release()
    {
        const long value = InterlockedDecrement(&refs);
        if (value == 0)
        {
            delete this;
        }
        return value;
    }

    STDMETHODIMP EventNotify(DWORD event, DWORD_PTR, DWORD)
    {
        Player* player = owner;
        if (player == nullptr || player->engine == nullptr)
        {
            return S_OK;
        }
        const bool active = player == g_active;
        if (!active)
        {
            return S_OK;
        }

        if (event == MF_MEDIA_ENGINE_EVENT_SEEKING)
        {
            g_seeking = true;
        }
        else if (event == MF_MEDIA_ENGINE_EVENT_SEEKED)
        {
            g_seeking = false;
            g_forceFrame = true;
        }

        if (
            (event == MF_MEDIA_ENGINE_EVENT_CANPLAY || event == MF_MEDIA_ENGINE_EVENT_LOADEDDATA) &&
            g_wantPlay)
        {
            player->engine->Play();
            g_forceFrame = true;
        }
        return S_OK;
    }
};

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static IDXGIFactory2* g_factory = nullptr;
static IMFDXGIDeviceManager* g_mfManager = nullptr;
static IMFMediaEngineClassFactory* g_engineFactory = nullptr;
static IMFMediaEngine* g_engine = nullptr;
static IMFMediaEngineEx* g_engineEx = nullptr;
static wchar_t g_preloadList[GPU_PRELOAD_MAX][MAX_PATH] = {};
static int g_preloadCount = 0;
static UINT g_resetToken = 0;
static GpuTarget g_targets[GPU_MAX_TARGETS] = {};
static int g_targetCount = 0;
static GpuLayer g_layers[GPU_MAX_LAYERS] = {};
static int g_layerCount = 0;
static wchar_t g_playingPath[MAX_PATH] = {};
static bool g_started = false;
static bool g_muted = false;
static double g_volume = 1.0;
static HANDLE g_statusMap = nullptr;
static GpuPlaybackStatus* g_status = nullptr;
static ID3D11Texture2D* g_frameTex = nullptr;
static ID3D11RenderTargetView* g_frameRtv = nullptr;
static int g_frameW = 0;
static int g_frameH = 0;
static HANDLE g_overlayMap = nullptr;
static const GpuOverlayHeader* g_overlayView = nullptr;
static HBITMAP g_overlayDib = nullptr;
static void* g_overlayDibBits = nullptr;
static int g_overlayDibW = 0;
static int g_overlayDibH = 0;
static LONG g_overlayCopiedGen = 0;
static DWORD g_lastVideoW = 0;
static DWORD g_lastVideoH = 0;
static LONG g_lastPresentedOverlayGen = 0;
static int* g_overlaySpanL = nullptr;
static int* g_overlaySpanR = nullptr;
static int g_overlaySpanRows = 0;
static ID3D11Texture2D* g_stagingTex = nullptr;
static int g_stagingW = 0;
static int g_stagingH = 0;
static HBITMAP g_canvasDib = nullptr;
static void* g_canvasBits = nullptr;
static int g_canvasDibW = 0;
static int g_canvasDibH = 0;

struct ReadyWindow
{
    HWND hwnd;
    HBITMAP dib;
    void* bits;
    int w;
    int h;
};

static ReadyWindow g_readyWindows[GPU_MAX_TARGETS] = {};

static void ReleaseReadyDibs()
{
    if (g_canvasDib != nullptr)
    {
        DeleteObject(g_canvasDib);
        g_canvasDib = nullptr;
    }
    g_canvasBits = nullptr;
    g_canvasDibW = 0;
    g_canvasDibH = 0;
    for (int i = 0; i < GPU_MAX_TARGETS; ++i)
    {
        if (g_readyWindows[i].dib != nullptr)
        {
            DeleteObject(g_readyWindows[i].dib);
        }
        g_readyWindows[i].hwnd = nullptr;
        g_readyWindows[i].dib = nullptr;
        g_readyWindows[i].bits = nullptr;
        g_readyWindows[i].w = 0;
        g_readyWindows[i].h = 0;
    }
}

static void ReleaseOverlayDib()
{
    if (g_overlayDib != nullptr)
    {
        DeleteObject(g_overlayDib);
        g_overlayDib = nullptr;
    }
    g_overlayDibBits = nullptr;
    g_overlayDibW = 0;
    g_overlayDibH = 0;
    g_overlayCopiedGen = 0;
    delete[] g_overlaySpanL;
    delete[] g_overlaySpanR;
    g_overlaySpanL = nullptr;
    g_overlaySpanR = nullptr;
    g_overlaySpanRows = 0;
}

// Records, per overlay row, where the non-transparent pixels start and end so
// blending can skip the empty parts of a slide.
static void ComputeOverlaySpans()
{
    if (g_overlayDibBits == nullptr || g_overlayDibW <= 0 || g_overlayDibH <= 0)
    {
        return;
    }
    if (g_overlaySpanRows != g_overlayDibH)
    {
        delete[] g_overlaySpanL;
        delete[] g_overlaySpanR;
        g_overlaySpanL = new int[g_overlayDibH];
        g_overlaySpanR = new int[g_overlayDibH];
        g_overlaySpanRows = g_overlayDibH;
    }
    const UINT32* pixels = static_cast<const UINT32*>(g_overlayDibBits);
    for (int y = 0; y < g_overlayDibH; ++y)
    {
        const UINT32* row = pixels + (static_cast<size_t>(y) * static_cast<size_t>(g_overlayDibW));
        int left = 0;
        while (left < g_overlayDibW && (row[left] >> 24) == 0)
        {
            ++left;
        }
        if (left == g_overlayDibW)
        {
            g_overlaySpanL[y] = -1;
            g_overlaySpanR[y] = -1;
            continue;
        }
        int right = g_overlayDibW;
        while (right > left && (row[right - 1] >> 24) == 0)
        {
            --right;
        }
        g_overlaySpanL[y] = left;
        g_overlaySpanR[y] = right;
    }
}

static void CloseOverlayMap()
{
    ReleaseOverlayDib();
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

static const GpuOverlayHeader* OpenOverlayMap()
{
    if (g_overlayView != nullptr)
    {
        return g_overlayView;
    }

    g_overlayMap = OpenFileMappingW(FILE_MAP_READ, FALSE, GPU_OVERLAY_MAP_NAME);
    if (g_overlayMap == nullptr)
    {
        return nullptr;
    }

    g_overlayView = static_cast<const GpuOverlayHeader*>(
        MapViewOfFile(g_overlayMap, FILE_MAP_READ, 0, 0, GPU_OVERLAY_MAP_BYTES)
    );
    if (g_overlayView == nullptr)
    {
        CloseHandle(g_overlayMap);
        g_overlayMap = nullptr;
    }
    return g_overlayView;
}

static bool EnsureOverlayDib(HDC refDc)
{
    const GpuOverlayHeader* overlay = OpenOverlayMap();
    if (overlay == nullptr || overlay->active == 0 || overlay->width <= 0 || overlay->height <= 0)
    {
        return false;
    }

    const LONG first = overlay->generation;
    if ((first & 1) != 0)
    {
        return g_overlayDib != nullptr;
    }

    const int srcW = overlay->width;
    const int srcH = overlay->height;
    if (srcW <= 0 || srcH <= 0)
    {
        return false;
    }

    if (g_overlayDib != nullptr &&
        g_overlayDibW == srcW &&
        g_overlayDibH == srcH &&
        g_overlayCopiedGen == first)
    {
        return true;
    }

    if (g_overlayDib == nullptr || g_overlayDibW != srcW || g_overlayDibH != srcH)
    {
        ReleaseOverlayDib();
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = srcW;
        info.bmiHeader.biHeight = -srcH;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        g_overlayDib = CreateDIBSection(refDc, &info, DIB_RGB_COLORS, &g_overlayDibBits, nullptr, 0);
        if (g_overlayDib == nullptr || g_overlayDibBits == nullptr)
        {
            ReleaseOverlayDib();
            return false;
        }
        g_overlayDibW = srcW;
        g_overlayDibH = srcH;
    }

    const unsigned char* pixels = reinterpret_cast<const unsigned char*>(overlay + 1);
    memcpy(g_overlayDibBits, pixels, static_cast<size_t>(srcW) * static_cast<size_t>(srcH) * 4);
    const LONG second = overlay->generation;
    if (first != second || (second & 1) != 0)
    {
        return g_overlayCopiedGen != 0 && g_overlayDib != nullptr;
    }
    g_overlayCopiedGen = first;
    ComputeOverlaySpans();
    return true;
}

static HBITMAP CreateTopDownDib(int width, int height, void** bits)
{
    if (bits != nullptr)
    {
        *bits = nullptr;
    }
    if (width < 1 || height < 1)
    {
        return nullptr;
    }
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* local = nullptr;
    HBITMAP dib = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &local, nullptr, 0);
    if (dib == nullptr || local == nullptr)
    {
        if (dib != nullptr)
        {
            DeleteObject(dib);
        }
        return nullptr;
    }
    if (bits != nullptr)
    {
        *bits = local;
    }
    return dib;
}

static void LogGpu(const char* text)
{
    char path[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0)
    {
        return;
    }
    char* slash = strrchr(path, '\\');
    if (slash != nullptr)
    {
        slash[1] = '\0';
    }
    lstrcatA(path, "gpu_debug.log");
    FILE* file = _fsopen(path, "a", _SH_DENYNO);
    if (file == nullptr)
    {
        return;
    }
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    fprintf(file, "%02u:%02u:%02u.%03u %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, text);
    fclose(file);
}

static void PublishStatus()
{
    if (g_status == nullptr)
    {
        return;
    }

    InterlockedIncrement(&g_status->sequence);
    if (g_engine != nullptr && g_layerCount > 0)
    {
        g_status->current = g_seeking ? g_seekTarget : g_engine->GetCurrentTime();
        const double duration = g_engine->GetDuration();
        g_status->duration = (duration > 0.0 && duration == duration) ? duration : 0.0;
        g_status->playing = (g_wantPlay && g_engine->IsPaused() != TRUE) ? 1 : 0;
        g_status->muted = g_muted ? 1 : 0;
        g_status->ready = (g_engine->GetReadyState() >= 2) ? 1 : 0;
        g_status->hasMedia = 1;
    }
    else
    {
        g_status->current = 0.0;
        g_status->duration = 0.0;
        g_status->playing = 0;
        g_status->muted = g_muted ? 1 : 0;
        g_status->ready = 0;
        g_status->hasMedia = 0;
    }
    InterlockedIncrement(&g_status->sequence);
}

static void EnsureStatusMap()
{
    if (g_status != nullptr)
    {
        return;
    }

    g_statusMap = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(GpuPlaybackStatus),
        GPU_STATUS_MAP_NAME
    );
    if (g_statusMap == nullptr)
    {
        return;
    }

    g_status = static_cast<GpuPlaybackStatus*>(
        MapViewOfFile(g_statusMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(GpuPlaybackStatus))
    );
    if (g_status != nullptr)
    {
        ZeroMemory(g_status, sizeof(*g_status));
    }
}

static void ApplyEngineAudio()
{
    if (g_engine == nullptr)
    {
        return;
    }
    g_engine->SetMuted(g_muted ? TRUE : FALSE);
    g_engine->SetVolume(g_volume);
}

static void PaintHwndBlack(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return;
    }
    RECT client = {};
    GetClientRect(hwnd, &client);
    if (client.right <= 0 || client.bottom <= 0)
    {
        return;
    }
    HDC hdc = GetDC(hwnd);
    if (hdc == nullptr)
    {
        return;
    }
    HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &client, fill);
    DeleteObject(fill);
    ReleaseDC(hwnd, hdc);
}

static void ReleaseFrameTex()
{
    if (g_stagingTex != nullptr)
    {
        g_stagingTex->Release();
        g_stagingTex = nullptr;
    }
    g_stagingW = 0;
    g_stagingH = 0;
    if (g_frameRtv != nullptr)
    {
        g_frameRtv->Release();
        g_frameRtv = nullptr;
    }
    if (g_frameTex != nullptr)
    {
        g_frameTex->Release();
        g_frameTex = nullptr;
    }
    g_frameW = 0;
    g_frameH = 0;
}

static void DropTargets()
{
    for (int i = 0; i < g_targetCount; ++i)
    {
        g_targets[i].hwnd = nullptr;
        g_targets[i].width = 0;
        g_targets[i].height = 0;
    }
    g_targetCount = 0;
}

static void ClearAndReleaseAllTargets()
{
    for (int i = 0; i < g_targetCount; ++i)
    {
        PaintHwndBlack(g_targets[i].hwnd);
    }
    DropTargets();
}

static void FitDest(int boxW, int boxH, int videoW, int videoH, int mode, RECT* dest)
{
    dest->left = 0;
    dest->top = 0;
    dest->right = boxW;
    dest->bottom = boxH;

    if (mode == 0 || videoW <= 0 || videoH <= 0 || boxW <= 0 || boxH <= 0)
    {
        return;
    }

    const float boxAspect = static_cast<float>(boxW) / static_cast<float>(boxH);
    const float videoAspect = static_cast<float>(videoW) / static_cast<float>(videoH);
    int w = boxW;
    int h = boxH;

    if (mode == 1)
    {
        if (videoAspect > boxAspect)
        {
            h = static_cast<int>((boxW / videoAspect) + 0.5f);
        }
        else
        {
            w = static_cast<int>((boxH * videoAspect) + 0.5f);
        }
    }
    else if (mode == 2)
    {
        if (videoAspect > boxAspect)
        {
            w = static_cast<int>((boxH * videoAspect) + 0.5f);
        }
        else
        {
            h = static_cast<int>((boxW / videoAspect) + 0.5f);
        }
    }

    dest->left = (boxW - w) / 2;
    dest->top = (boxH - h) / 2;
    dest->right = dest->left + w;
    dest->bottom = dest->top + h;
}

static CRITICAL_SECTION g_vblankLock;
static bool g_vblankLockReady = false;
static IDXGIOutput* g_vblankOutput = nullptr;
static HMONITOR g_vblankMonitor = nullptr;
static HANDLE g_vblankEvent = nullptr;
static HANDLE g_vblankThread = nullptr;
static volatile LONG g_vblankStop = 0;
static volatile LONG g_vblankPaced = 0;

static DWORD WINAPI VBlankThread(LPVOID)
{
    while (g_vblankStop == 0)
    {
        EnterCriticalSection(&g_vblankLock);
        IDXGIOutput* output = g_vblankOutput;
        if (output != nullptr)
        {
            output->AddRef();
        }
        LeaveCriticalSection(&g_vblankLock);
        if (output == nullptr)
        {
            InterlockedExchange(&g_vblankPaced, 0);
            Sleep(50);
            continue;
        }
        const HRESULT hr = output->WaitForVBlank();
        output->Release();
        if (FAILED(hr))
        {
            InterlockedExchange(&g_vblankPaced, 0);
            Sleep(50);
            continue;
        }
        InterlockedExchange(&g_vblankPaced, 1);
        SetEvent(g_vblankEvent);
    }
    return 0;
}

// Paces presentation to the monitor that shows the largest target (the
// audience output), so each frame is ready before that monitor's next refresh.
static void UpdateVBlankOutput()
{
    if (!g_vblankLockReady || g_device == nullptr)
    {
        return;
    }
    HWND largest = nullptr;
    long long largestArea = 0;
    for (int i = 0; i < g_targetCount; ++i)
    {
        const long long area = static_cast<long long>(g_targets[i].width) * g_targets[i].height;
        if (g_targets[i].hwnd != nullptr && area > largestArea)
        {
            largestArea = area;
            largest = g_targets[i].hwnd;
        }
    }
    if (largest == nullptr)
    {
        return;
    }
    HMONITOR monitor = MonitorFromWindow(largest, MONITOR_DEFAULTTONEAREST);
    if (monitor == g_vblankMonitor)
    {
        return;
    }

    IDXGIOutput* found = nullptr;
    IDXGIDevice* dxgiDevice = nullptr;
    if (SUCCEEDED(g_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice))))
    {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter)))
        {
            IDXGIOutput* output = nullptr;
            for (UINT i = 0; found == nullptr && adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                DXGI_OUTPUT_DESC desc = {};
                if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == monitor)
                {
                    found = output;
                }
                else
                {
                    output->Release();
                }
            }
            adapter->Release();
        }
        dxgiDevice->Release();
    }

    EnterCriticalSection(&g_vblankLock);
    if (g_vblankOutput != nullptr)
    {
        g_vblankOutput->Release();
    }
    g_vblankOutput = found;
    g_vblankMonitor = monitor;
    LeaveCriticalSection(&g_vblankLock);
    LogGpu(found != nullptr ? "pacing: vblank of main output" : "pacing: timer (output not on this adapter)");
}

HANDLE D3DVideo_VBlankEvent()
{
    return g_vblankEvent;
}

bool D3DVideo_PacedByVBlank()
{
    return g_vblankPaced != 0;
}

static void StartVBlankPacing()
{
    InitializeCriticalSection(&g_vblankLock);
    g_vblankLockReady = true;
    g_vblankStop = 0;
    g_vblankEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (g_vblankEvent != nullptr)
    {
        g_vblankThread = CreateThread(nullptr, 0, VBlankThread, nullptr, 0, nullptr);
    }
}

static void StopVBlankPacing()
{
    if (!g_vblankLockReady)
    {
        return;
    }
    InterlockedExchange(&g_vblankStop, 1);
    if (g_vblankThread != nullptr)
    {
        WaitForSingleObject(g_vblankThread, 500);
        CloseHandle(g_vblankThread);
        g_vblankThread = nullptr;
    }
    InterlockedExchange(&g_vblankPaced, 0);
    EnterCriticalSection(&g_vblankLock);
    if (g_vblankOutput != nullptr)
    {
        g_vblankOutput->Release();
        g_vblankOutput = nullptr;
    }
    g_vblankMonitor = nullptr;
    LeaveCriticalSection(&g_vblankLock);
    if (g_vblankEvent != nullptr)
    {
        CloseHandle(g_vblankEvent);
        g_vblankEvent = nullptr;
    }
}

static void SyncTargets(const GpuMessage* message)
{
    DropTargets();
    const unsigned count = message->targetCount < GPU_MAX_TARGETS ? message->targetCount : GPU_MAX_TARGETS;
    for (unsigned i = 0; i < count; ++i)
    {
        HWND hwnd = reinterpret_cast<HWND>(static_cast<UINT_PTR>(message->targets[i]));
        if (hwnd == nullptr || !IsWindow(hwnd))
        {
            continue;
        }
        RECT client = {};
        GetClientRect(hwnd, &client);
        if (client.right <= 0 || client.bottom <= 0)
        {
            continue;
        }
        g_targets[g_targetCount].hwnd = hwnd;
        g_targets[g_targetCount].width = client.right;
        g_targets[g_targetCount].height = client.bottom;
        g_targets[g_targetCount].srcX = message->srcX[i];
        g_targets[g_targetCount].srcY = message->srcY[i];
        g_targets[g_targetCount].srcW = message->srcW[i];
        g_targets[g_targetCount].srcH = message->srcH[i];
        g_targets[g_targetCount].ovX = message->ovX[i];
        g_targets[g_targetCount].ovY = message->ovY[i];
        g_targets[g_targetCount].ovW = message->ovW[i];
        g_targets[g_targetCount].ovH = message->ovH[i];
        g_targets[g_targetCount].videoOn = message->videoOn[i];
        g_targets[g_targetCount].overlayOn = message->overlayOn[i];
        g_targets[g_targetCount].paneIndex = message->paneIndex[i];
        g_targets[g_targetCount].paneCount = message->paneCount[i] > 0 ? message->paneCount[i] : 1;
        g_targetCount += 1;
    }
    UpdateVBlankOutput();
}

static void MapLayerToClient(
    const GpuLayer* layer,
    int clientW,
    int clientH,
    DWORD videoW,
    DWORD videoH,
    RECT* dest
)
{
    int boxX = 0;
    int boxY = 0;
    int boxW = clientW;
    int boxH = clientH;

    if (layer->slideW > 0 && layer->slideH > 0)
    {
        boxX = MulDiv(layer->x, clientW, layer->slideW);
        boxY = MulDiv(layer->y, clientH, layer->slideH);
        boxW = MulDiv(layer->w, clientW, layer->slideW);
        boxH = MulDiv(layer->h, clientH, layer->slideH);
    }

    RECT fitted = {};
    FitDest(boxW, boxH, static_cast<int>(videoW), static_cast<int>(videoH), layer->mode, &fitted);
    dest->left = boxX + fitted.left;
    dest->top = boxY + fitted.top;
    dest->right = boxX + fitted.right;
    dest->bottom = boxY + fitted.bottom;
}

static bool EnsureFrameTex(int width, int height)
{
    if (width <= 0 || height <= 0 || g_device == nullptr)
    {
        return false;
    }
    if (g_frameTex != nullptr && g_frameW == width && g_frameH == height)
    {
        return true;
    }

    ReleaseFrameTex();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;

    if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &g_frameTex)) || g_frameTex == nullptr)
    {
        return false;
    }
    if (FAILED(g_device->CreateRenderTargetView(g_frameTex, nullptr, &g_frameRtv)) || g_frameRtv == nullptr)
    {
        ReleaseFrameTex();
        return false;
    }
    g_frameW = width;
    g_frameH = height;
    return true;
}

// CPU-readable copy of g_frameTex. Mapping it is much faster than reading the
// frame back through a GDI DC.
static bool EnsureStagingTex(int width, int height)
{
    if (g_stagingTex != nullptr && g_stagingW == width && g_stagingH == height)
    {
        return true;
    }
    if (g_stagingTex != nullptr)
    {
        g_stagingTex->Release();
        g_stagingTex = nullptr;
    }
    g_stagingW = 0;
    g_stagingH = 0;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &g_stagingTex)) || g_stagingTex == nullptr)
    {
        g_stagingTex = nullptr;
        return false;
    }
    g_stagingW = width;
    g_stagingH = height;
    return true;
}

static void ComposeCanvasSize(int* width, int* height)
{
    if (g_layerCount > 0 && g_layers[0].slideW > 0 && g_layers[0].slideH > 0)
    {
        *width = g_layers[0].slideW;
        *height = g_layers[0].slideH;
        return;
    }

    int maxW = 0;
    int maxH = 0;
    for (int i = 0; i < g_targetCount; ++i)
    {
        if (g_targets[i].width > maxW)
        {
            maxW = g_targets[i].width;
        }
        if (g_targets[i].height > maxH)
        {
            maxH = g_targets[i].height;
        }
    }
    if (maxW > 0 && maxH > 0)
    {
        *width = maxW;
        *height = maxH;
        return;
    }

    *width = 1920;
    *height = 1080;
}

static void GpuPaneRect(int clientW, int clientH, int paneIndex, int paneCount, RECT* out)
{
    if (out == nullptr)
    {
        return;
    }
    if (clientW < 1)
    {
        clientW = 1;
    }
    if (clientH < 1)
    {
        clientH = 1;
    }
    if (paneCount < 1)
    {
        paneCount = 1;
    }
    if (paneIndex < 0)
    {
        paneIndex = 0;
    }
    if (paneIndex >= paneCount)
    {
        paneIndex = paneCount - 1;
    }
    const int slice = clientW / paneCount;
    out->left = paneIndex * slice;
    out->top = 0;
    out->right = (paneIndex + 1 >= paneCount) ? clientW : (paneIndex + 1) * slice;
    out->bottom = clientH;
    if (out->right <= out->left)
    {
        out->right = out->left + 1;
    }
}

static bool EnsureBitsDib(HBITMAP* slot, void** bits, int* haveW, int* haveH, int width, int height)
{
    if (slot == nullptr || bits == nullptr || haveW == nullptr || haveH == nullptr || width < 1 || height < 1)
    {
        return false;
    }
    if (*slot != nullptr && *bits != nullptr && *haveW == width && *haveH == height)
    {
        return true;
    }
    if (*slot != nullptr)
    {
        DeleteObject(*slot);
        *slot = nullptr;
        *bits = nullptr;
    }
    *slot = CreateTopDownDib(width, height, bits);
    if (*slot == nullptr || *bits == nullptr)
    {
        *haveW = 0;
        *haveH = 0;
        return false;
    }
    *haveW = width;
    *haveH = height;
    return true;
}

static ReadyWindow* ReadyWindowFor(HWND hwnd, int width, int height)
{
    if (hwnd == nullptr || width < 1 || height < 1)
    {
        return nullptr;
    }
    int freeSlot = -1;
    for (int i = 0; i < GPU_MAX_TARGETS; ++i)
    {
        if (g_readyWindows[i].hwnd == hwnd)
        {
            if (!EnsureBitsDib(&g_readyWindows[i].dib, &g_readyWindows[i].bits, &g_readyWindows[i].w, &g_readyWindows[i].h, width, height))
            {
                return nullptr;
            }
            return &g_readyWindows[i];
        }
        if (g_readyWindows[i].hwnd != nullptr && !IsWindow(g_readyWindows[i].hwnd))
        {
            g_readyWindows[i].hwnd = nullptr;
        }
        if (freeSlot < 0 && g_readyWindows[i].hwnd == nullptr)
        {
            freeSlot = i;
        }
    }
    if (freeSlot < 0)
    {
        return nullptr;
    }
    if (!EnsureBitsDib(&g_readyWindows[freeSlot].dib, &g_readyWindows[freeSlot].bits, &g_readyWindows[freeSlot].w, &g_readyWindows[freeSlot].h, width, height))
    {
        return nullptr;
    }
    g_readyWindows[freeSlot].hwnd = hwnd;
    return &g_readyWindows[freeSlot];
}

static int DibStride(HBITMAP dib)
{
    DIBSECTION section = {};
    if (dib == nullptr || GetObject(dib, sizeof(section), &section) == 0)
    {
        return 0;
    }
    return section.dsBm.bmWidthBytes;
}

// Column lookup for nearest-pixel scaling: g_xMap[x] is the source column for
// destination column x. Rebuilt per call; it is at most one window wide.
static int* g_xMap = nullptr;
static int g_xMapCap = 0;

static const int* BuildXMap(int sx, int sw, int dw)
{
    if (dw > g_xMapCap)
    {
        delete[] g_xMap;
        g_xMapCap = dw;
        g_xMap = new int[g_xMapCap];
    }
    for (int x = 0; x < dw; ++x)
    {
        g_xMap[x] = sx + static_cast<int>((static_cast<long long>(x) * sw) / dw);
    }
    return g_xMap;
}

static inline int Div255(int value)
{
    return value / 255;
}

static void CopyStretch32(
    unsigned char* dst,
    int dstStride,
    int dx,
    int dy,
    int dw,
    int dh,
    const unsigned char* src,
    int srcStride,
    int sx,
    int sy,
    int sw,
    int sh)
{
    if (dst == nullptr || src == nullptr || dw < 1 || dh < 1 || sw < 1 || sh < 1 || dstStride < 4 || srcStride < 4)
    {
        return;
    }
    const int* xmap = sw == dw ? nullptr : BuildXMap(sx, sw, dw);
    int lastSrcRow = -1;
    const unsigned char* lastOut = nullptr;
    for (int y = 0; y < dh; ++y)
    {
        const int syy = sy + static_cast<int>((static_cast<long long>(y) * sh) / dh);
        unsigned char* out = dst + (static_cast<size_t>(dy + y) * static_cast<size_t>(dstStride)) + (static_cast<size_t>(dx) * 4);
        if (syy == lastSrcRow && lastOut != nullptr)
        {
            memcpy(out, lastOut, static_cast<size_t>(dw) * 4);
            continue;
        }
        const unsigned char* row = src + (static_cast<size_t>(syy) * static_cast<size_t>(srcStride));
        if (xmap == nullptr)
        {
            memcpy(out, row + (static_cast<size_t>(sx) * 4), static_cast<size_t>(dw) * 4);
        }
        else
        {
            const UINT32* in = reinterpret_cast<const UINT32*>(row);
            UINT32* px = reinterpret_cast<UINT32*>(out);
            for (int x = 0; x < dw; ++x)
            {
                px[x] = in[xmap[x]];
            }
        }
        lastSrcRow = syy;
        lastOut = out;
    }
}

// spanL/spanR give, per source row, the first and one-past-last column with any
// alpha (spanL < 0 means the row is fully transparent). Null means unknown.
static void BlendStretch32(
    unsigned char* dst,
    int dstStride,
    int dx,
    int dy,
    int dw,
    int dh,
    const unsigned char* src,
    int srcStride,
    int sx,
    int sy,
    int sw,
    int sh,
    const int* spanL,
    const int* spanR)
{
    if (dst == nullptr || src == nullptr || dw < 1 || dh < 1 || sw < 1 || sh < 1 || dstStride < 4 || srcStride < 4)
    {
        return;
    }
    const int* xmap = BuildXMap(sx, sw, dw);
    for (int y = 0; y < dh; ++y)
    {
        const int syy = sy + static_cast<int>((static_cast<long long>(y) * sh) / dh);
        int x0 = 0;
        int x1 = dw;
        if (spanL != nullptr && spanR != nullptr)
        {
            const int left = spanL[syy] > sx ? spanL[syy] : sx;
            const int right = spanR[syy] < sx + sw ? spanR[syy] : sx + sw;
            if (spanL[syy] < 0 || right <= left)
            {
                continue;
            }
            x0 = static_cast<int>(((static_cast<long long>(left - sx) * dw) + sw - 1) / sw);
            x1 = static_cast<int>(((static_cast<long long>(right - sx) * dw) + sw - 1) / sw);
            if (x1 > dw)
            {
                x1 = dw;
            }
        }
        const UINT32* in = reinterpret_cast<const UINT32*>(src + (static_cast<size_t>(syy) * static_cast<size_t>(srcStride)));
        UINT32* px = reinterpret_cast<UINT32*>(dst + (static_cast<size_t>(dy + y) * static_cast<size_t>(dstStride)) + (static_cast<size_t>(dx) * 4));
        for (int x = x0; x < x1; ++x)
        {
            const UINT32 pix = in[xmap[x]];
            const int a = static_cast<int>(pix >> 24);
            if (a == 0)
            {
                continue;
            }
            if (a == 255)
            {
                px[x] = pix;
                continue;
            }
            const UINT32 back = px[x];
            const int inv = 255 - a;
            const int b = Div255(static_cast<int>(pix & 0xFF) * a + static_cast<int>(back & 0xFF) * inv);
            const int g = Div255(static_cast<int>((pix >> 8) & 0xFF) * a + static_cast<int>((back >> 8) & 0xFF) * inv);
            const int r = Div255(static_cast<int>((pix >> 16) & 0xFF) * a + static_cast<int>((back >> 16) & 0xFF) * inv);
            px[x] = 0xFF000000u | (static_cast<UINT32>(r) << 16) | (static_cast<UINT32>(g) << 8) | static_cast<UINT32>(b);
        }
    }
}

static bool PaintTargets(const unsigned char* canvasBytes, int canvasStride, int composeW, int composeH);

static bool PresentComposedFrame(DWORD videoW, DWORD videoH)
{
    if (g_engine == nullptr || g_context == nullptr || g_layerCount <= 0 || g_targetCount <= 0)
    {
        return false;
    }

    int composeW = 0;
    int composeH = 0;
    ComposeCanvasSize(&composeW, &composeH);
    if (!EnsureFrameTex(composeW, composeH))
    {
        return false;
    }

    const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    g_context->ClearRenderTargetView(g_frameRtv, clear);

    MFVideoNormalizedRect source = { 0.0f, 0.0f, 1.0f, 1.0f };
    MFARGB bg = { 0, 0, 0, 255 };
    RECT dest = {};
    MapLayerToClient(&g_layers[0], composeW, composeH, videoW, videoH, &dest);
    if (FAILED(g_engine->TransferVideoFrame(g_frameTex, &source, &dest, &bg)))
    {
        return false;
    }

    if (EnsureStagingTex(composeW, composeH))
    {
        g_context->CopyResource(g_stagingTex, g_frameTex);
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(g_context->Map(g_stagingTex, 0, D3D11_MAP_READ, 0, &mapped)) && mapped.pData != nullptr)
        {
            const bool painted = PaintTargets(
                static_cast<const unsigned char*>(mapped.pData),
                static_cast<int>(mapped.RowPitch),
                composeW,
                composeH
            );
            g_context->Unmap(g_stagingTex, 0);
            return painted;
        }
    }

    IDXGISurface1* surface = nullptr;
    if (FAILED(g_frameTex->QueryInterface(__uuidof(IDXGISurface1), reinterpret_cast<void**>(&surface))) ||
        surface == nullptr)
    {
        return false;
    }

    HDC srcDc = nullptr;
    if (FAILED(surface->GetDC(FALSE, &srcDc)) || srcDc == nullptr)
    {
        surface->Release();
        return false;
    }

    if (!EnsureBitsDib(&g_canvasDib, &g_canvasBits, &g_canvasDibW, &g_canvasDibH, composeW, composeH))
    {
        surface->ReleaseDC(nullptr);
        surface->Release();
        return false;
    }

    HDC canvasDc = CreateCompatibleDC(srcDc);
    if (canvasDc == nullptr)
    {
        surface->ReleaseDC(nullptr);
        surface->Release();
        return false;
    }
    HBITMAP oldCanvas = static_cast<HBITMAP>(SelectObject(canvasDc, g_canvasDib));
    StretchBlt(canvasDc, 0, 0, composeW, composeH, srcDc, 0, 0, composeW, composeH, SRCCOPY);
    SelectObject(canvasDc, oldCanvas);
    DeleteDC(canvasDc);
    surface->ReleaseDC(nullptr);
    surface->Release();

    const int canvasStride = DibStride(g_canvasDib);
    if (canvasStride <= 0 || g_canvasBits == nullptr)
    {
        return false;
    }
    return PaintTargets(static_cast<const unsigned char*>(g_canvasBits), canvasStride, composeW, composeH);
}

// Crops the composed video frame into every target window and lays the slide
// overlay on top.
static bool PaintTargets(const unsigned char* canvasBytes, int canvasStride, int composeW, int composeH)
{
    const GpuOverlayHeader* overlay = OpenOverlayMap();
    if (overlay != nullptr && (overlay->generation & 1) != 0)
    {
        return false;
    }

    bool needOverlay = false;
    for (int i = 0; i < g_targetCount; ++i)
    {
        if (g_targets[i].overlayOn != 0)
        {
            needOverlay = true;
            break;
        }
    }

    bool haveOverlay = false;
    int overlayStride = 0;
    if (overlay != nullptr && overlay->active != 0)
    {
        haveOverlay = EnsureOverlayDib(nullptr) &&
            g_overlayDibBits != nullptr &&
            g_overlayCopiedGen == overlay->generation &&
            (overlay->generation & 1) == 0;
        if (haveOverlay)
        {
            overlayStride = DibStride(g_overlayDib);
            if (overlayStride <= 0)
            {
                haveOverlay = false;
            }
        }
        if (needOverlay && !haveOverlay)
        {
            return false;
        }
    }

    const unsigned char* overlayBytes = haveOverlay ? static_cast<const unsigned char*>(g_overlayDibBits) : nullptr;
    const bool spansReady = g_overlaySpanL != nullptr && g_overlaySpanRows == g_overlayDibH;

    bool painted[GPU_MAX_TARGETS] = {};
    for (int i = 0; i < g_targetCount; ++i)
    {
        if (painted[i])
        {
            continue;
        }
        HWND hwnd = g_targets[i].hwnd;
        if (hwnd == nullptr || !IsWindow(hwnd))
        {
            continue;
        }

        RECT client = {};
        GetClientRect(hwnd, &client);
        if (client.right <= 0 || client.bottom <= 0)
        {
            continue;
        }

        ReadyWindow* ready = ReadyWindowFor(hwnd, client.right, client.bottom);
        if (ready == nullptr || ready->bits == nullptr)
        {
            continue;
        }
        const int winStride = DibStride(ready->dib);
        if (winStride <= 0)
        {
            continue;
        }
        unsigned char* winBits = static_cast<unsigned char*>(ready->bits);
        memset(winBits, 0, static_cast<size_t>(winStride) * static_cast<size_t>(client.bottom));

        for (int j = 0; j < g_targetCount; ++j)
        {
            if (g_targets[j].hwnd != hwnd)
            {
                continue;
            }
            painted[j] = true;
            g_targets[j].width = client.right;
            g_targets[j].height = client.bottom;

            RECT pane = {};
            GpuPaneRect(client.right, client.bottom, g_targets[j].paneIndex, g_targets[j].paneCount, &pane);
            const int destX = pane.left;
            const int destY = pane.top;
            const int destW = pane.right - pane.left;
            const int destH = pane.bottom - pane.top;
            if (destW <= 0 || destH <= 0)
            {
                continue;
            }

            int srcX = g_targets[j].srcX;
            int srcY = g_targets[j].srcY;
            int srcW = g_targets[j].srcW;
            int srcH = g_targets[j].srcH;
            if (srcW <= 0 || srcH <= 0)
            {
                srcX = 0;
                srcY = 0;
                srcW = composeW;
                srcH = composeH;
            }
            if (srcX < 0)
            {
                srcX = 0;
            }
            if (srcY < 0)
            {
                srcY = 0;
            }
            if (srcX + srcW > composeW)
            {
                srcW = composeW - srcX;
            }
            if (srcY + srcH > composeH)
            {
                srcH = composeH - srcY;
            }

            if (g_targets[j].videoOn && srcW > 0 && srcH > 0)
            {
                CopyStretch32(
                    winBits,
                    winStride,
                    destX,
                    destY,
                    destW,
                    destH,
                    canvasBytes,
                    canvasStride,
                    srcX,
                    srcY,
                    srcW,
                    srcH
                );
            }

            if (g_targets[j].overlayOn && haveOverlay && overlayBytes != nullptr)
            {
                int ovX = g_targets[j].ovX;
                int ovY = g_targets[j].ovY;
                int ovW = g_targets[j].ovW;
                int ovH = g_targets[j].ovH;
                if (ovW <= 0 || ovH <= 0)
                {
                    ovX = 0;
                    ovY = 0;
                    ovW = g_overlayDibW;
                    ovH = g_overlayDibH;
                }
                if (g_overlayDibW > 0 && g_overlayDibH > 0 && (g_overlayDibW != composeW || g_overlayDibH != composeH))
                {
                    ovX = MulDiv(ovX, g_overlayDibW, composeW);
                    ovY = MulDiv(ovY, g_overlayDibH, composeH);
                    ovW = MulDiv(ovW, g_overlayDibW, composeW);
                    ovH = MulDiv(ovH, g_overlayDibH, composeH);
                }
                if (ovX < 0)
                {
                    ovX = 0;
                }
                if (ovY < 0)
                {
                    ovY = 0;
                }
                if (ovX + ovW > g_overlayDibW)
                {
                    ovW = g_overlayDibW - ovX;
                }
                if (ovY + ovH > g_overlayDibH)
                {
                    ovH = g_overlayDibH - ovY;
                }
                BlendStretch32(
                    winBits,
                    winStride,
                    destX,
                    destY,
                    destW,
                    destH,
                    overlayBytes,
                    overlayStride,
                    ovX,
                    ovY,
                    ovW,
                    ovH,
                    spansReady ? g_overlaySpanL : nullptr,
                    spansReady ? g_overlaySpanR : nullptr
                );
            }
        }

        HDC dstDc = GetDC(hwnd);
        if (dstDc == nullptr)
        {
            continue;
        }
        HDC winDc = CreateCompatibleDC(dstDc);
        if (winDc != nullptr)
        {
            HBITMAP oldWindow = static_cast<HBITMAP>(SelectObject(winDc, ready->dib));
            BitBlt(dstDc, 0, 0, client.right, client.bottom, winDc, 0, 0, SRCCOPY);
            SelectObject(winDc, oldWindow);
            DeleteDC(winDc);
        }
        ReleaseDC(hwnd, dstDc);
    }

    return true;
}

static void LogGpuPath(const char* what, const wchar_t* path)
{
    const wchar_t* name = path;
    for (const wchar_t* p = path; *p != L'\0'; ++p)
    {
        if (*p == L'\\' || *p == L'/')
        {
            name = p + 1;
        }
    }
    char text[MAX_PATH + 64] = {};
    sprintf_s(text, sizeof(text), "%s %ls", what, name);
    LogGpu(text);
}

static bool CreatePlayerEngine(Player* player)
{
    if (player->engine != nullptr)
    {
        return true;
    }
    if (g_engineFactory == nullptr || g_mfManager == nullptr)
    {
        return false;
    }

    player->notify = new EngineNotify(player);
    IMFAttributes* attrs = nullptr;
    if (FAILED(MFCreateAttributes(&attrs, 3)) || attrs == nullptr)
    {
        player->notify->Release();
        player->notify = nullptr;
        return false;
    }
    attrs->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, g_mfManager);
    attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, player->notify);
    attrs->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM);
    const HRESULT created = g_engineFactory->CreateInstance(0, attrs, &player->engine);
    attrs->Release();
    if (FAILED(created))
    {
        char text[64] = {};
        sprintf_s(text, sizeof(text), "player engine create failed 0x%08lX", static_cast<unsigned long>(created));
        LogGpu(text);
    }

    if (FAILED(created) || player->engine == nullptr ||
        FAILED(player->engine->QueryInterface(__uuidof(IMFMediaEngineEx), reinterpret_cast<void**>(&player->ex))) ||
        player->ex == nullptr)
    {
        if (player->engine != nullptr)
        {
            player->engine->Shutdown();
            player->engine->Release();
            player->engine = nullptr;
        }
        player->notify->owner = nullptr;
        player->notify->Release();
        player->notify = nullptr;
        return false;
    }

    player->engine->SetAutoPlay(FALSE);
    player->engine->SetPreload(MF_MEDIA_ENGINE_PRELOAD_AUTOMATIC);
    player->engine->SetMuted(TRUE);
    return true;
}

static void ReleasePlayer(Player* player)
{
    if (player->notify != nullptr)
    {
        player->notify->owner = nullptr;
    }
    if (player->ex != nullptr)
    {
        player->ex->Release();
        player->ex = nullptr;
    }
    if (player->engine != nullptr)
    {
        player->engine->Shutdown();
        player->engine->Release();
        player->engine = nullptr;
    }
    if (player->notify != nullptr)
    {
        player->notify->Release();
        player->notify = nullptr;
    }
    player->path[0] = L'\0';
}

static Player* FindPlayer(const wchar_t* path)
{
    for (int i = 0; i < GPU_MAX_PLAYERS; ++i)
    {
        if (g_players[i].engine != nullptr && g_players[i].path[0] != L'\0' &&
            lstrcmpiW(g_players[i].path, path) == 0)
        {
            return &g_players[i];
        }
    }
    return nullptr;
}

// Empty slot first, then the least recently used player that is neither on
// glass nor in the open project's top picks.
static Player* PlayerSlotFor(int maxKeepRank)
{
    for (int i = 0; i < GPU_MAX_PLAYERS; ++i)
    {
        if (g_players[i].path[0] == L'\0' && &g_players[i] != g_active)
        {
            return &g_players[i];
        }
    }
    Player* victim = nullptr;
    for (int i = 0; i < GPU_MAX_PLAYERS; ++i)
    {
        Player* player = &g_players[i];
        if (player == g_active || (player->wantRank >= 0 && player->wantRank < maxKeepRank))
        {
            continue;
        }
        if (victim == nullptr || (LONG)(player->lastUsed - victim->lastUsed) < 0)
        {
            victim = player;
        }
    }
    return victim;
}

static void BuildFileUrl(const wchar_t* path, wchar_t* url, int urlChars)
{
    if (wcsstr(path, L"://") != nullptr)
    {
        lstrcpynW(url, path, urlChars);
        return;
    }
    wchar_t full[MAX_PATH] = {};
    const DWORD got = GetFullPathNameW(path, MAX_PATH, full, nullptr);
    if (got == 0 || got >= MAX_PATH)
    {
        lstrcpynW(full, path, MAX_PATH);
    }
    for (wchar_t* p = full; *p != L'\0'; ++p)
    {
        if (*p == L'\\')
        {
            *p = L'/';
        }
    }
    wsprintfW(url, L"file:///%s", full);
}

static bool LoadPlayer(Player* player, const wchar_t* path)
{
    if (!CreatePlayerEngine(player))
    {
        LogGpuPath("player engine unavailable for", path);
        return false;
    }

    player->engine->Pause();
    player->engine->SetMuted(TRUE);

    wchar_t url[MAX_PATH * 2] = {};
    BuildFileUrl(path, url, MAX_PATH * 2);
    BSTR source = SysAllocString(url);
    if (source == nullptr)
    {
        return false;
    }

    HRESULT hr = E_FAIL;
    bool fromRam = false;
    IMFByteStream* stream = nullptr;
    if (SUCCEEDED(VideoCache_OpenStream(path, &stream)) && stream != nullptr)
    {
        hr = player->ex->SetSourceFromByteStream(stream, source);
        stream->Release();
        fromRam = SUCCEEDED(hr);
    }
    if (FAILED(hr))
    {
        hr = player->engine->SetSource(source);
        if (SUCCEEDED(hr))
        {
            player->engine->Load();
        }
    }
    SysFreeString(source);

    if (FAILED(hr))
    {
        player->path[0] = L'\0';
        LogGpuPath("player load failed", path);
        return false;
    }

    player->engine->SetLoop(TRUE);
    lstrcpynW(player->path, path, MAX_PATH);
    player->lastUsed = GetTickCount();
    LogGpuPath(fromRam ? "player load (RAM)" : "player load (disk)", path);
    return true;
}

// Pause, silence and rewind, so the next click starts on the first frame.
static void ParkPlayer(Player* player)
{
    if (player == nullptr || player->engine == nullptr)
    {
        return;
    }
    player->engine->Pause();
    player->engine->SetMuted(TRUE);
    player->engine->SetCurrentTime(0.0);
    player->lastUsed = GetTickCount();
}

// The glass keeps its last picture until this player decodes a frame, which
// for a pre-opened player is a frame or two after Play.
static void ActivatePlayer(Player* player, bool preopened)
{
    Player* previous = g_active;
    g_active = player;
    g_engine = player->engine;
    g_engineEx = player->ex;
    lstrcpynW(g_playingPath, player->path, MAX_PATH);
    player->lastUsed = GetTickCount();
    ApplyEngineAudio();

    g_sourceHasFrame = false;
    g_forceFrame = false;
    g_sourceSetMs = GetTickCount();
    g_seeking = false;
    g_playStartMs = GetTickCount();
    LogGpuPath(preopened ? "PLAY pre-opened" : "PLAY new load", player->path);

    if (previous != nullptr && previous != player)
    {
        ParkPlayer(previous);
    }
}

static void DeactivatePlayer()
{
    Player* previous = g_active;
    g_active = nullptr;
    g_engine = nullptr;
    g_engineEx = nullptr;
    g_playingPath[0] = L'\0';
    ParkPlayer(previous);
}

static bool PlayPath(const wchar_t* path)
{
    if (path == nullptr || path[0] == L'\0')
    {
        return false;
    }
    if (g_active != nullptr && lstrcmpiW(g_playingPath, path) == 0)
    {
        return true;
    }

    Player* player = FindPlayer(path);
    const bool preopened = player != nullptr;
    if (player == nullptr)
    {
        player = PlayerSlotFor(0);
        if (player == nullptr || !LoadPlayer(player, path))
        {
            return false;
        }
    }
    ActivatePlayer(player, preopened);
    g_wantPlay = true;
    return true;
}

static void ApplyPreloadList()
{
    VideoCache_Want(g_preloadList, g_preloadCount);

    for (int i = 0; i < GPU_MAX_PLAYERS; ++i)
    {
        g_players[i].wantRank = -1;
    }
    const int keep = g_preloadCount < GPU_MAX_PLAYERS ? g_preloadCount : GPU_MAX_PLAYERS;
    for (int i = 0; i < keep; ++i)
    {
        Player* player = FindPlayer(g_preloadList[i]);
        if (player != nullptr)
        {
            player->wantRank = i;
        }
    }
    for (int i = 0; i < keep; ++i)
    {
        if (FindPlayer(g_preloadList[i]) != nullptr)
        {
            continue;
        }
        Player* player = PlayerSlotFor(keep);
        if (player == nullptr)
        {
            LogGpuPath("no player slot for", g_preloadList[i]);
            break;
        }
        if (LoadPlayer(player, g_preloadList[i]))
        {
            player->wantRank = i;
        }
    }
}

bool D3DVideo_Startup()
{
    if (g_started)
    {
        return true;
    }

    EnsureStatusMap();
    LogGpu("Startup begin");

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        return false;
    }

    if (FAILED(MFStartup(MF_VERSION)))
    {
        CoUninitialize();
        return false;
    }

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
    HRESULT deviceHr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &g_device,
        nullptr,
        &g_context
    );
    if (FAILED(deviceHr))
    {
        deviceHr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &g_device,
            nullptr,
            &g_context
        );
    }
    if (FAILED(deviceHr))
    {
        MFShutdown();
        CoUninitialize();
        return false;
    }

    ID3D10Multithread* mt = nullptr;
    if (SUCCEEDED(g_device->QueryInterface(__uuidof(ID3D10Multithread), reinterpret_cast<void**>(&mt))) &&
        mt != nullptr)
    {
        mt->SetMultithreadProtected(TRUE);
        mt->Release();
    }

    IDXGIDevice* dxgiDevice = nullptr;
    if (SUCCEEDED(g_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice))))
    {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter)))
        {
            adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&g_factory));
            adapter->Release();
        }
        dxgiDevice->Release();
    }

    if (g_factory == nullptr)
    {
        D3DVideo_Shutdown();
        return false;
    }

    if (FAILED(MFCreateDXGIDeviceManager(&g_resetToken, &g_mfManager)) ||
        FAILED(g_mfManager->ResetDevice(g_device, g_resetToken)))
    {
        D3DVideo_Shutdown();
        return false;
    }

    if (FAILED(CoCreateInstance(
            CLSID_MFMediaEngineClassFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&g_engineFactory))))
    {
        D3DVideo_Shutdown();
        return false;
    }

    if (!CreatePlayerEngine(&g_players[0]))
    {
        D3DVideo_Shutdown();
        return false;
    }

    VideoCache_Startup();
    StartVBlankPacing();
    g_started = true;
    LogGpu("Startup ok");
    return true;
}

void D3DVideo_Shutdown()
{
    StopVBlankPacing();
    ClearAndReleaseAllTargets();
    ReleaseFrameTex();
    g_active = nullptr;
    g_engine = nullptr;
    g_engineEx = nullptr;
    g_playingPath[0] = L'\0';
    g_layerCount = 0;
    g_wantPlay = false;
    g_seeking = false;
    g_forceFrame = false;
    PublishStatus();

    for (int i = 0; i < GPU_MAX_PLAYERS; ++i)
    {
        ReleasePlayer(&g_players[i]);
    }
    VideoCache_Shutdown();
    if (g_engineFactory != nullptr)
    {
        g_engineFactory->Release();
        g_engineFactory = nullptr;
    }
    if (g_mfManager != nullptr)
    {
        g_mfManager->Release();
        g_mfManager = nullptr;
    }
    if (g_factory != nullptr)
    {
        g_factory->Release();
        g_factory = nullptr;
    }
    if (g_context != nullptr)
    {
        g_context->Release();
        g_context = nullptr;
    }
    if (g_device != nullptr)
    {
        g_device->Release();
        g_device = nullptr;
    }
    if (g_status != nullptr)
    {
        UnmapViewOfFile(g_status);
        g_status = nullptr;
    }
    if (g_statusMap != nullptr)
    {
        CloseHandle(g_statusMap);
        g_statusMap = nullptr;
    }
    ReleaseReadyDibs();
    CloseOverlayMap();

    if (g_started)
    {
        MFShutdown();
        CoUninitialize();
    }
    g_started = false;
}

void D3DVideo_Apply(const GpuMessage* message)
{
    if (message == nullptr)
    {
        return;
    }

    if (message->cmd == GPU_CMD_QUIT || message->cmd == GPU_CMD_STOP)
    {
        g_layerCount = 0;
        g_wantPlay = false;
        g_seeking = false;
        g_forceFrame = false;
        DeactivatePlayer();
        PublishStatus();
        DropTargets();
        LogGpu("STOP");
        return;
    }

    if (message->cmd == GPU_CMD_PRELOAD_BEGIN)
    {
        g_preloadCount = 0;
        return;
    }

    if (message->cmd == GPU_CMD_PRELOAD)
    {
        const unsigned count = message->layerCount < GPU_MAX_LAYERS ? message->layerCount : GPU_MAX_LAYERS;
        for (unsigned i = 0; i < count && g_preloadCount < GPU_PRELOAD_MAX; ++i)
        {
            if (message->layers[i].path[0] != L'\0')
            {
                lstrcpynW(g_preloadList[g_preloadCount], message->layers[i].path, MAX_PATH);
                g_preloadCount += 1;
            }
        }
        return;
    }

    if (message->cmd == GPU_CMD_PRELOAD_END)
    {
        ApplyPreloadList();
        return;
    }

    if (message->cmd == GPU_CMD_PAUSE)
    {
        g_wantPlay = false;
        if (g_engine != nullptr)
        {
            g_engine->Pause();
        }
        PublishStatus();
        return;
    }

    if (message->cmd == GPU_CMD_RESUME)
    {
        g_wantPlay = true;
        if (g_engine != nullptr)
        {
            g_engine->Play();
        }
        PublishStatus();
        return;
    }

    if (message->cmd == GPU_CMD_SEEK)
    {
        if (g_engine != nullptr && message->seekTime >= 0.0)
        {
            g_seeking = true;
            g_seekTarget = message->seekTime;
            g_seekStartedMs = GetTickCount();
            g_forceFrame = false;
            if (g_status != nullptr)
            {
                InterlockedIncrement(&g_status->sequence);
                g_status->current = g_seekTarget;
                InterlockedIncrement(&g_status->sequence);
            }
            g_engine->SetCurrentTime(message->seekTime);
        }
        PublishStatus();
        return;
    }

    if (message->cmd == GPU_CMD_SET_AUDIO)
    {
        g_muted = message->muted != 0;
        g_volume = message->volume;
        if (g_volume < 0.0)
        {
            g_volume = 0.0;
        }
        if (g_volume > 1.0)
        {
            g_volume = 1.0;
        }
        ApplyEngineAudio();
        PublishStatus();
        return;
    }

    if (message->cmd != GPU_CMD_PLAY)
    {
        return;
    }

    SyncTargets(message);
    g_layerCount = 0;
    const unsigned layers = message->layerCount < GPU_MAX_LAYERS ? message->layerCount : GPU_MAX_LAYERS;
    for (unsigned i = 0; i < layers; ++i)
    {
        if (message->layers[i].path[0] != L'\0')
        {
            g_layers[g_layerCount] = message->layers[i];
            g_layerCount += 1;
        }
    }

    if (g_layerCount > 0)
    {
        const bool samePath = g_active != nullptr && lstrcmpiW(g_playingPath, g_layers[0].path) == 0;
        PlayPath(g_layers[0].path);
        if (!samePath)
        {
            g_wantPlay = true;
        }
        if (g_wantPlay && g_engine != nullptr && g_engine->GetReadyState() >= 2)
        {
            g_engine->Play();
        }
    }
    PublishStatus();
}

void D3DVideo_Tick()
{
    PublishStatus();

    if (g_engine == nullptr || g_engineEx == nullptr || g_layerCount <= 0 || g_targetCount <= 0)
    {
        return;
    }

    if (g_wantPlay && g_engine->GetReadyState() >= 2 && g_engine->IsPaused() == TRUE)
    {
        g_engine->Play();
    }

    if (g_seeking)
    {
        if ((GetTickCount() - g_seekStartedMs) > 500)
        {
            g_seeking = false;
            g_forceFrame = true;
        }
        else
        {
            return;
        }
    }

    LONGLONG pts = 0;
    const bool decoded = g_engineEx->OnVideoStreamTick(&pts) == S_OK;
    if (decoded)
    {
        g_sourceHasFrame = true;
    }
    if (!g_sourceHasFrame && (GetTickCount() - g_sourceSetMs) < GPU_FIRST_FRAME_WAIT_MS)
    {
        return;
    }
    const bool newVideo = g_forceFrame || decoded;
    g_forceFrame = false;

    LONG overlayGen = 0;
    const GpuOverlayHeader* overlay = OpenOverlayMap();
    if (overlay != nullptr)
    {
        overlayGen = overlay->generation;
    }
    const bool overlayReady = overlayGen != 0 && (overlayGen & 1) == 0;
    const bool overlayChanged = overlayReady && overlayGen != g_lastPresentedOverlayGen;

    if (!newVideo && !overlayChanged)
    {
        return;
    }

    DWORD videoW = 0;
    DWORD videoH = 0;
    if (FAILED(g_engine->GetNativeVideoSize(&videoW, &videoH)) || videoW == 0 || videoH == 0)
    {
        videoW = g_lastVideoW;
        videoH = g_lastVideoH;
    }
    if (videoW == 0 || videoH == 0)
    {
        return;
    }

    if (overlay != nullptr && overlay->active != 0 && (overlayGen & 1) != 0)
    {
        return;
    }

    if (PresentComposedFrame(videoW, videoH))
    {
        if (g_playStartMs != 0)
        {
            char text[64] = {};
            sprintf_s(text, sizeof(text), "first frame on glass %lu ms after PLAY", GetTickCount() - g_playStartMs);
            LogGpu(text);
            g_playStartMs = 0;
        }
        g_lastVideoW = videoW;
        g_lastVideoH = videoH;
        if (overlayReady)
        {
            g_lastPresentedOverlayGen = overlayGen;
        }
    }
}

bool D3DVideo_IsPlaying()
{
    return g_layerCount > 0 && g_engine != nullptr && g_wantPlay;
}
