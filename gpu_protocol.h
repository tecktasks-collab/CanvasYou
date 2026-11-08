#pragma once

#include <windows.h>

#define GPU_PIPE_NAME "\\\\.\\pipe\\PresentationSoftwareGpu"
#define GPU_STATUS_MAP_NAME L"Local\\PresentationSoftwareGpuStatus"
#define GPU_READY_EVENT_NAME "Local\\PresentationSoftwareGpuReady"
#define GPU_MAX_TARGETS 32
#define GPU_MAX_LAYERS 4

enum GpuCommand
{
    GPU_CMD_QUIT = 1,
    GPU_CMD_STOP = 2,
    GPU_CMD_PLAY = 3,
    GPU_CMD_PAUSE = 4,
    GPU_CMD_RESUME = 5,
    GPU_CMD_SEEK = 6,
    GPU_CMD_SET_AUDIO = 7,
    // The project's video list goes over as BEGIN, then PRELOAD messages
    // carrying up to GPU_MAX_LAYERS paths each in layers[], then END.
    GPU_CMD_PRELOAD_BEGIN = 8,
    GPU_CMD_PRELOAD = 9,
    GPU_CMD_PRELOAD_END = 10
};

#define GPU_PRELOAD_MAX 64

struct GpuLayer
{
    int x;
    int y;
    int w;
    int h;
    int slideW;
    int slideH;
    int mode;
    wchar_t path[MAX_PATH];
};

struct GpuMessage
{
    unsigned cmd;
    unsigned targetCount;
    unsigned layerCount;
    unsigned long long targets[GPU_MAX_TARGETS];
    GpuLayer layers[GPU_MAX_LAYERS];
    double seekTime;
    double volume;
    int muted;
    int srcX[GPU_MAX_TARGETS];
    int srcY[GPU_MAX_TARGETS];
    int srcW[GPU_MAX_TARGETS];
    int srcH[GPU_MAX_TARGETS];
    int ovX[GPU_MAX_TARGETS];
    int ovY[GPU_MAX_TARGETS];
    int ovW[GPU_MAX_TARGETS];
    int ovH[GPU_MAX_TARGETS];
    int videoOn[GPU_MAX_TARGETS];
    int overlayOn[GPU_MAX_TARGETS];
    int paneIndex[GPU_MAX_TARGETS];
    int paneCount[GPU_MAX_TARGETS];
};

#define GPU_OVERLAY_MAP_NAME L"Local\\PresentationSoftwareSlideOverlay"
#define GPU_OVERLAY_MAX_W 1920
#define GPU_OVERLAY_MAX_H 1080
#define GPU_OVERLAY_KEY 0x00FF00FF

struct GpuOverlayHeader
{
    volatile LONG generation;
    int width;
    int height;
    int active;
};

#define GPU_OVERLAY_MAP_BYTES (sizeof(GpuOverlayHeader) + (GPU_OVERLAY_MAX_W * GPU_OVERLAY_MAX_H * 4))

struct GpuPlaybackStatus
{
    volatile LONG sequence;
    double current;
    double duration;
    int playing;
    int muted;
    int ready;
    int hasMedia;
};
