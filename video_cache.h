#pragma once

#include <windows.h>
#include <mfidl.h>

// RAM cache for video files, owned by Gpu.exe. Each video gets up to 256 MB of
// file bytes in RAM (the whole file when it fits); a background thread fills it
// and, while a long video plays, keeps the window rolling ahead of playback.
bool VideoCache_Startup();
void VideoCache_Shutdown();

// Paths in priority order. Entries for older paths stay cached until the
// 4 GB budget needs their memory.
void VideoCache_Want(const wchar_t (*paths)[MAX_PATH], int count);

// Byte stream that reads from RAM where cached and from disk otherwise.
HRESULT VideoCache_OpenStream(const wchar_t* path, IMFByteStream** out);
