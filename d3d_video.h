#pragma once

#include "gpu_protocol.h"

bool D3DVideo_Startup();
void D3DVideo_Shutdown();
void D3DVideo_Apply(const GpuMessage* message);
void D3DVideo_Tick();
bool D3DVideo_IsPlaying();
// Signalled right after each vertical blank of the monitor showing the main
// output. While D3DVideo_PacedByVBlank() is false, tick on a timer instead.
HANDLE D3DVideo_VBlankEvent();
bool D3DVideo_PacedByVBlank();
