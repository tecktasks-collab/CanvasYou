#pragma once

#include <windows.h>

#define AUDIO_PLAYLIST_MAX 64
#define AUDIO_TRACK_MAX 256

struct AudioPlaylist
{
    int id;
    char name[128];
};

struct AudioTrack
{
    int id;
    int playlistId;
    char name[128];
    char path[MAX_PATH];
};

bool Audio_EnsureSchema();
void Audio_LoadUiPrefs();
void Audio_SaveSplit(int topHeight);
int Audio_SplitTop();

int Audio_ListPlaylists(AudioPlaylist* out, int max);
int Audio_ListTracks(int playlistId, AudioTrack* out, int max);
int Audio_ActivePlaylistId();
void Audio_SetActivePlaylistId(int id);
int Audio_AddPlaylist(const char* name);
bool Audio_AddTrackFromFile(int playlistId, const wchar_t* sourcePath);
bool Audio_InsertTrackCopy(int playlistId, const char* name, const char* relPath);
bool Audio_DeleteTracks(const int* ids, int count);
bool Audio_IsAudioExtension(const wchar_t* path);

// Runtime only — not persisted. Inactive = Clear audio (or cold start).
bool Audio_LayerActive();
void Audio_ClearLayer();
void Audio_ActivateLayer();

bool Audio_PlayTrack(int trackId);
int Audio_PlayingTrackId();
void Audio_Stop();
void Audio_Pause();
void Audio_Resume();
bool Audio_IsPlaying();
bool Audio_HasMedia();
double Audio_CurrentSeconds();
double Audio_DurationSeconds();
void Audio_SeekSeconds(double seconds);
void Audio_SetMuted(bool muted);
bool Audio_ResolveTrackPath(int trackId, char* outPath, int outChars);
void Audio_LastError(char* out, int outChars);
void Audio_Shutdown();

bool Audio_Autoplay();
void Audio_SetAutoplay(bool on);
bool Audio_Loop();
void Audio_SetLoop(bool on);
bool Audio_Shuffle();
void Audio_SetShuffle(bool on);
// Call from the UI timer. Advances or repeats when a track ends.
void Audio_TickAdvance();
