#pragma once

#include <windows.h>
#include "gpu_protocol.h"

struct GpuOutputTarget
{
    HWND hwnd;
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

bool GpuClient_Connect();
void GpuClient_Disconnect();
bool GpuClient_Ready();
bool GpuClient_OwnsOutputs();
void GpuClient_Stop();
void GpuClient_Play(HWND* targets, int targetCount, const GpuLayer* layers, int layerCount);
void GpuClient_PlayOutputs(const GpuOutputTarget* outputs, int outputCount, const GpuLayer* layers, int layerCount);
void GpuClient_Pause();
void GpuClient_Resume();
void GpuClient_Seek(double seconds);
void GpuClient_SetAudio(bool muted, double volume);
bool GpuClient_GetStatus(GpuPlaybackStatus* out);
void GpuClient_SetSlideOverlay(HBITMAP frame, int width, int height, bool active);
void GpuClient_SetSlideOverlayBits(const unsigned char* bits, int width, int height, bool active);
// Makes Gpu.exe present its current frame again, even while paused.
void GpuClient_Repaint();
// Sends the project's video list (priority order) only when it changed.
void GpuClient_Preload(const wchar_t (*paths)[MAX_PATH], int count);
