#pragma once

// Session recorder. Lives in Initializer.exe (its own thread) so it keeps
// recording if Window.exe dies. Preferences in Window.exe store
// recording.on, recording.rate, recording.keep, and recording.path.
// A missing recording.on still means "on" when the save folder already exists.
//
// Guard rails: 15 minutes / 500 MB per session, a 2 GB total cap, and a
// 5 GB free-space floor. Older session folders beyond "keep" are deleted.

#define RECORDER_RELOAD_EVENT "Local\\PresentationSoftwareRecordingReload"

// Returns true if the recorder thread is running.
bool Recorder_Start();
void Recorder_Stop();
