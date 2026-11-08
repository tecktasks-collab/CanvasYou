#include "audio.h"
#include "store.h"
#include "sqlite3.h"

// Playback runs on the same Media Foundation Media Engine that d3d_video.cpp
// uses for video, in audio-only mode. DirectShow/MCI are not used: this
// machine has no legacy MP3 decoder path for them (VFW_E_CANNOT_RENDER).
#include <mfapi.h>
#include <mfidl.h>
#include <mfmediaengine.h>
#include <mferror.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

static int g_splitTop = 220;
static int g_activePlaylistId = 0;
static bool g_layerActive = false;
static bool g_muted = false;
static int g_playingTrackId = 0;
static bool g_autoplay = false;
static bool g_loop = false;
static bool g_shuffle = false;

static bool g_mfStarted = false;
static IMFMediaEngine* g_engine = nullptr;
static IMFMediaEngineEx* g_engineEx = nullptr;
static bool g_hasMedia = false;
static volatile LONG g_wantPlay = 0;
static volatile LONG g_ended = 0;
static volatile LONG g_seekingFlag = 0;
static volatile LONG g_errorFlag = 0;
static double g_seekTarget = 0.0;
static DWORD g_seekStartedMs = 0;
static char g_lastError[256] = {};

static void AudioLogf(const char* format, ...)
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
    lstrcatA(path, "audio_debug.log");

    FILE* file = nullptr;
    if (fopen_s(&file, path, "a") != 0 || file == nullptr)
    {
        return;
    }

    char text[1024] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    fprintf(
        file,
        "%02u:%02u:%02u.%03u [tid %lu] %s\n",
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds,
        GetCurrentThreadId(),
        text
    );
    fclose(file);
}

static const char* EngineErrorName(DWORD_PTR code)
{
    switch (code)
    {
        case MF_MEDIA_ENGINE_ERR_ABORTED: return "aborted";
        case MF_MEDIA_ENGINE_ERR_NETWORK: return "network/file read";
        case MF_MEDIA_ENGINE_ERR_DECODE: return "decode";
        case MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED: return "format not supported";
        case MF_MEDIA_ENGINE_ERR_ENCRYPTED: return "encrypted";
        default: return "unknown";
    }
}

class AudioEngineNotify : public IMFMediaEngineNotify
{
public:
    long refs;

    AudioEngineNotify() : refs(1) {}

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

    STDMETHODIMP EventNotify(DWORD event, DWORD_PTR param1, DWORD param2)
    {
        switch (event)
        {
            case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
                AudioLogf("event LOADEDMETADATA duration=%.2f", g_engine != nullptr ? g_engine->GetDuration() : 0.0);
                break;
            case MF_MEDIA_ENGINE_EVENT_CANPLAY:
                AudioLogf("event CANPLAY wantPlay=%ld", g_wantPlay);
                if (g_engine != nullptr && g_wantPlay != 0)
                {
                    const HRESULT hr = g_engine->Play();
                    AudioLogf("  Play from CANPLAY hr=0x%08lX", static_cast<unsigned long>(hr));
                }
                break;
            case MF_MEDIA_ENGINE_EVENT_PLAYING:
                AudioLogf("event PLAYING");
                break;
            case MF_MEDIA_ENGINE_EVENT_PAUSE:
                AudioLogf("event PAUSE");
                break;
            case MF_MEDIA_ENGINE_EVENT_ENDED:
                AudioLogf("event ENDED");
                InterlockedExchange(&g_ended, 1);
                InterlockedExchange(&g_wantPlay, 0);
                break;
            case MF_MEDIA_ENGINE_EVENT_SEEKING:
                InterlockedExchange(&g_seekingFlag, 1);
                break;
            case MF_MEDIA_ENGINE_EVENT_SEEKED:
                InterlockedExchange(&g_seekingFlag, 0);
                break;
            case MF_MEDIA_ENGINE_EVENT_ERROR:
                sprintf_s(
                    g_lastError,
                    sizeof(g_lastError),
                    "Media Foundation error: %s (code %lu, hr 0x%08lX)",
                    EngineErrorName(param1),
                    static_cast<unsigned long>(param1),
                    static_cast<unsigned long>(param2)
                );
                AudioLogf("event ERROR %s", g_lastError);
                InterlockedExchange(&g_errorFlag, 1);
                InterlockedExchange(&g_wantPlay, 0);
                break;
            default:
                break;
        }
        return S_OK;
    }
};

static AudioEngineNotify* g_notify = nullptr;

static void JoinPath(char* out, int outChars, const char* folder, const char* name)
{
    lstrcpynA(out, folder, outChars);
    const int len = lstrlenA(out);
    if (len > 0 && out[len - 1] != '\\' && len + 1 < outChars)
    {
        out[len] = '\\';
        out[len + 1] = '\0';
    }
    lstrcatA(out, name);
}

static const wchar_t* WideFileName(const wchar_t* path)
{
    if (path == nullptr || path[0] == L'\0')
    {
        return L"";
    }
    const wchar_t* slash = wcsrchr(path, L'\\');
    const wchar_t* fwd = wcsrchr(path, L'/');
    if (fwd != nullptr && (slash == nullptr || fwd > slash))
    {
        slash = fwd;
    }
    return slash != nullptr ? slash + 1 : path;
}

bool Audio_IsAudioExtension(const wchar_t* path)
{
    if (path == nullptr)
    {
        return false;
    }
    const wchar_t* dot = wcsrchr(path, L'.');
    if (dot == nullptr)
    {
        return false;
    }
    return
        lstrcmpiW(dot, L".mp3") == 0 ||
        lstrcmpiW(dot, L".wav") == 0 ||
        lstrcmpiW(dot, L".m4a") == 0 ||
        lstrcmpiW(dot, L".aac") == 0 ||
        lstrcmpiW(dot, L".flac") == 0 ||
        lstrcmpiW(dot, L".wma") == 0 ||
        lstrcmpiW(dot, L".ogg") == 0;
}

static bool EnsureEngine()
{
    if (g_engine != nullptr)
    {
        return true;
    }

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        sprintf_s(g_lastError, sizeof(g_lastError), "MFStartup failed (hr 0x%08lX)", static_cast<unsigned long>(hr));
        AudioLogf("%s", g_lastError);
        return false;
    }
    g_mfStarted = true;

    IMFMediaEngineClassFactory* factory = nullptr;
    hr = CoCreateInstance(
        CLSID_MFMediaEngineClassFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)
    );
    if (FAILED(hr) || factory == nullptr)
    {
        sprintf_s(g_lastError, sizeof(g_lastError), "Media Engine class factory failed (hr 0x%08lX)", static_cast<unsigned long>(hr));
        AudioLogf("%s", g_lastError);
        return false;
    }

    g_notify = new AudioEngineNotify();
    IMFAttributes* attrs = nullptr;
    hr = MFCreateAttributes(&attrs, 1);
    if (FAILED(hr) || attrs == nullptr)
    {
        factory->Release();
        sprintf_s(g_lastError, sizeof(g_lastError), "MFCreateAttributes failed (hr 0x%08lX)", static_cast<unsigned long>(hr));
        AudioLogf("%s", g_lastError);
        return false;
    }
    attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, g_notify);

    hr = factory->CreateInstance(MF_MEDIA_ENGINE_AUDIOONLY, attrs, &g_engine);
    attrs->Release();
    factory->Release();
    if (FAILED(hr) || g_engine == nullptr)
    {
        g_engine = nullptr;
        sprintf_s(g_lastError, sizeof(g_lastError), "Media Engine create failed (hr 0x%08lX)", static_cast<unsigned long>(hr));
        AudioLogf("%s", g_lastError);
        return false;
    }

    g_engine->QueryInterface(IID_PPV_ARGS(&g_engineEx));
    g_engine->SetMuted(g_muted ? TRUE : FALSE);
    g_engine->SetVolume(1.0);
    AudioLogf("Media Engine ready (audio-only), engineEx=%s", g_engineEx != nullptr ? "yes" : "no");
    return true;
}

// file:///C:/dir/name%20x.mp3 — percent-encode everything outside the
// unreserved set so names with #, %, ?, & and spaces load reliably.
static void BuildFileUrl(const wchar_t* path, wchar_t* out, int outChars)
{
    if (out == nullptr || outChars < 16)
    {
        return;
    }
    char utf8[MAX_PATH * 4] = {};
    WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr, nullptr);

    int len = 0;
    for (const wchar_t* p = L"file:///"; *p != L'\0' && len < outChars - 1; ++p)
    {
        out[len++] = *p;
    }

    static const wchar_t hex[] = L"0123456789ABCDEF";
    for (const unsigned char* b = reinterpret_cast<const unsigned char*>(utf8); *b != 0 && len < outChars - 4; ++b)
    {
        const unsigned char c = *b;
        if (c == '\\')
        {
            out[len++] = L'/';
        }
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~' || c == '/' || c == ':')
        {
            out[len++] = static_cast<wchar_t>(c);
        }
        else
        {
            out[len++] = L'%';
            out[len++] = hex[c >> 4];
            out[len++] = hex[c & 15];
        }
    }
    out[len] = L'\0';
}

static void ResetPlaybackFlags()
{
    InterlockedExchange(&g_wantPlay, 0);
    InterlockedExchange(&g_ended, 0);
    InterlockedExchange(&g_seekingFlag, 0);
    InterlockedExchange(&g_errorFlag, 0);
    g_seekTarget = 0.0;
}

static void UnloadEngineSource()
{
    ResetPlaybackFlags();
    if (g_engine != nullptr && g_hasMedia)
    {
        g_engine->Pause();
        g_engine->SetSource(nullptr);
    }
    g_hasMedia = false;
    g_playingTrackId = 0;
}

bool Audio_EnsureSchema()
{
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return false;
    }

    char* error = nullptr;
    if (sqlite3_exec(
            db,
            "CREATE TABLE IF NOT EXISTS audio_playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT NOT NULL, "
            "sort_order INTEGER NOT NULL DEFAULT 0);"
            "CREATE TABLE IF NOT EXISTS audio_tracks ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "playlist_id INTEGER NOT NULL, "
            "name TEXT NOT NULL, "
            "path TEXT NOT NULL, "
            "sort_order INTEGER NOT NULL DEFAULT 0);",
            nullptr,
            nullptr,
            &error) != SQLITE_OK)
    {
        sqlite3_free(error);
        return false;
    }

    char root[MAX_PATH] = {};
    Store_Root(root, MAX_PATH);
    if (root[0] != '\0')
    {
        char media[MAX_PATH] = {};
        JoinPath(media, MAX_PATH, root, "media");
        CreateDirectoryA(media, nullptr);
        char audio[MAX_PATH] = {};
        JoinPath(audio, MAX_PATH, media, "audio");
        CreateDirectoryA(audio, nullptr);
    }

    Audio_LoadUiPrefs();
    static bool seeded = false;
    if (!seeded)
    {
        srand((unsigned)time(nullptr) ^ GetTickCount());
        seeded = true;
    }
    if (Audio_ListPlaylists(nullptr, 0) == 0)
    {
        Audio_AddPlaylist("Playlist 1");
    }
    if (g_activePlaylistId <= 0)
    {
        AudioPlaylist first = {};
        if (Audio_ListPlaylists(&first, 1) > 0)
        {
            g_activePlaylistId = first.id;
            Store_SetInt("audio.active_playlist", g_activePlaylistId);
        }
    }
    return true;
}

void Audio_LoadUiPrefs()
{
    g_splitTop = Store_GetInt("program.audio_split", 220);
    if (g_splitTop < 80)
    {
        g_splitTop = 80;
    }
    g_activePlaylistId = Store_GetInt("audio.active_playlist", 0);
    g_autoplay = true;
    g_loop = Store_GetInt("audio.loop", 0) != 0;
    g_shuffle = Store_GetInt("audio.shuffle", 0) != 0;
}

void Audio_SaveSplit(int topHeight)
{
    if (topHeight < 80)
    {
        topHeight = 80;
    }
    g_splitTop = topHeight;
    Store_SetInt("program.audio_split", g_splitTop);
}

int Audio_SplitTop()
{
    return g_splitTop;
}

int Audio_ActivePlaylistId()
{
    return g_activePlaylistId;
}

void Audio_SetActivePlaylistId(int id)
{
    g_activePlaylistId = id;
    Store_SetInt("audio.active_playlist", id);
}

int Audio_ListPlaylists(AudioPlaylist* out, int max)
{
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return 0;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT id, name FROM audio_playlists ORDER BY sort_order ASC, id ASC;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return 0;
    }

    int count = 0;
    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        if (out != nullptr && count < max)
        {
            out[count].id = sqlite3_column_int(statement, 0);
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
            lstrcpynA(out[count].name, name != nullptr ? name : "Playlist", 128);
        }
        count += 1;
    }
    sqlite3_finalize(statement);
    return count;
}

int Audio_ListTracks(int playlistId, AudioTrack* out, int max)
{
    sqlite3* db = Store_Db();
    if (db == nullptr || playlistId <= 0)
    {
        return 0;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT id, playlist_id, name, path FROM audio_tracks "
            "WHERE playlist_id = ?1 ORDER BY sort_order ASC, id ASC;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_int(statement, 1, playlistId);
    int count = 0;
    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        if (out != nullptr && count < max)
        {
            out[count].id = sqlite3_column_int(statement, 0);
            out[count].playlistId = sqlite3_column_int(statement, 1);
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
            lstrcpynA(out[count].name, name != nullptr ? name : "Track", 128);
            lstrcpynA(out[count].path, path != nullptr ? path : "", MAX_PATH);
        }
        count += 1;
    }
    sqlite3_finalize(statement);
    return count;
}

int Audio_AddPlaylist(const char* name)
{
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return 0;
    }

    const char* label = (name != nullptr && name[0] != '\0') ? name : "Playlist";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "INSERT INTO audio_playlists(name, sort_order) "
            "VALUES(?1, COALESCE((SELECT MAX(sort_order) + 1 FROM audio_playlists), 0));",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_text(statement, 1, label, -1, SQLITE_STATIC);
    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return 0;
    }
    sqlite3_finalize(statement);
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

static bool CopyAudioFile(const wchar_t* source, char* outRel, int outChars)
{
    if (source == nullptr || outRel == nullptr || outChars < 2)
    {
        return false;
    }

    char root[MAX_PATH] = {};
    Store_Root(root, MAX_PATH);
    if (root[0] == '\0')
    {
        return false;
    }

    char audioDir[MAX_PATH] = {};
    JoinPath(audioDir, MAX_PATH, root, "media\\audio");
    CreateDirectoryA(audioDir, nullptr);

    const wchar_t* base = WideFileName(source);
    char baseA[MAX_PATH] = {};
    WideCharToMultiByte(CP_ACP, 0, base, -1, baseA, MAX_PATH, nullptr, nullptr);
    if (baseA[0] == '\0')
    {
        lstrcpyA(baseA, "track.mp3");
    }

    char stem[MAX_PATH] = {};
    char ext[64] = {};
    char* dot = strrchr(baseA, '.');
    if (dot != nullptr && dot != baseA)
    {
        const int stemLen = static_cast<int>(dot - baseA);
        lstrcpynA(stem, baseA, stemLen + 1 < MAX_PATH ? stemLen + 1 : MAX_PATH);
        lstrcpynA(ext, dot, 64);
    }
    else
    {
        lstrcpynA(stem, baseA, MAX_PATH);
    }

    char candidate[MAX_PATH] = {};
    lstrcpynA(candidate, baseA, MAX_PATH);
    wchar_t destW[MAX_PATH] = {};
    for (int n = 0; n < 1000; ++n)
    {
        if (n > 0)
        {
            wsprintfA(candidate, "%s-%d%s", stem, n + 1, ext);
        }
        char destA[MAX_PATH] = {};
        JoinPath(destA, MAX_PATH, audioDir, candidate);
        MultiByteToWideChar(CP_ACP, 0, destA, -1, destW, MAX_PATH);
        if (GetFileAttributesW(destW) == INVALID_FILE_ATTRIBUTES)
        {
            if (!CopyFileW(source, destW, TRUE))
            {
                return false;
            }
            wsprintfA(outRel, "media\\audio\\%s", candidate);
            return true;
        }
    }
    return false;
}

bool Audio_AddTrackFromFile(int playlistId, const wchar_t* sourcePath)
{
    if (!Audio_IsAudioExtension(sourcePath) || playlistId <= 0)
    {
        return false;
    }

    char rel[MAX_PATH] = {};
    if (!CopyAudioFile(sourcePath, rel, MAX_PATH))
    {
        return false;
    }

    char title[128] = {};
    const wchar_t* base = WideFileName(sourcePath);
    WideCharToMultiByte(CP_ACP, 0, base, -1, title, 128, nullptr, nullptr);
    char* dot = strrchr(title, '.');
    if (dot != nullptr)
    {
        *dot = '\0';
    }
    if (title[0] == '\0')
    {
        lstrcpyA(title, "Track");
    }

    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "INSERT INTO audio_tracks(playlist_id, name, path, sort_order) "
            "VALUES(?1, ?2, ?3, COALESCE((SELECT MAX(sort_order) + 1 FROM audio_tracks WHERE playlist_id = ?1), 0));",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_int(statement, 1, playlistId);
    sqlite3_bind_text(statement, 2, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, rel, -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool Audio_InsertTrackCopy(int playlistId, const char* name, const char* relPath)
{
    if (playlistId <= 0 || relPath == nullptr || relPath[0] == '\0')
    {
        return false;
    }

    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return false;
    }

    const char* label = (name != nullptr && name[0] != '\0') ? name : "Track";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "INSERT INTO audio_tracks(playlist_id, name, path, sort_order) "
            "VALUES(?1, ?2, ?3, COALESCE((SELECT MAX(sort_order) + 1 FROM audio_tracks WHERE playlist_id = ?1), 0));",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_int(statement, 1, playlistId);
    sqlite3_bind_text(statement, 2, label, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, relPath, -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool Audio_DeleteTracks(const int* ids, int count)
{
    sqlite3* db = Store_Db();
    if (db == nullptr || ids == nullptr || count <= 0)
    {
        return false;
    }

    bool ok = true;
    for (int i = 0; i < count; ++i)
    {
        if (ids[i] == g_playingTrackId)
        {
            Audio_ClearLayer();
        }
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM audio_tracks WHERE id = ?1;", -1, &statement, nullptr) != SQLITE_OK)
        {
            ok = false;
            continue;
        }
        sqlite3_bind_int(statement, 1, ids[i]);
        if (sqlite3_step(statement) != SQLITE_DONE)
        {
            ok = false;
        }
        sqlite3_finalize(statement);
    }
    return ok;
}

bool Audio_LayerActive()
{
    return g_layerActive;
}

void Audio_ClearLayer()
{
    AudioLogf("ClearLayer");
    UnloadEngineSource();
    g_layerActive = false;
}

void Audio_ActivateLayer()
{
    g_layerActive = true;
}

void Audio_Stop()
{
    if (g_engine == nullptr || !g_hasMedia)
    {
        return;
    }
    InterlockedExchange(&g_wantPlay, 0);
    g_engine->Pause();
    g_engine->SetCurrentTime(0.0);
}

void Audio_Pause()
{
    if (g_engine == nullptr || !g_hasMedia)
    {
        return;
    }
    InterlockedExchange(&g_wantPlay, 0);
    const HRESULT hr = g_engine->Pause();
    AudioLogf("Pause hr=0x%08lX", static_cast<unsigned long>(hr));
}

void Audio_Resume()
{
    if (g_engine == nullptr || !g_hasMedia)
    {
        return;
    }
    InterlockedExchange(&g_ended, 0);
    InterlockedExchange(&g_wantPlay, 1);
    const HRESULT hr = g_engine->Play();
    AudioLogf("Resume hr=0x%08lX", static_cast<unsigned long>(hr));
    if (SUCCEEDED(hr))
    {
        g_layerActive = true;
    }
}

bool Audio_IsPlaying()
{
    if (g_engine == nullptr || !g_hasMedia || g_wantPlay == 0 || g_ended != 0)
    {
        return false;
    }
    return g_engine->IsPaused() != TRUE;
}

bool Audio_HasMedia()
{
    return g_engine != nullptr && g_hasMedia && g_layerActive;
}

double Audio_CurrentSeconds()
{
    if (g_engine == nullptr || !g_hasMedia)
    {
        return 0.0;
    }
    if (g_seekingFlag != 0)
    {
        if ((GetTickCount() - g_seekStartedMs) > 500)
        {
            InterlockedExchange(&g_seekingFlag, 0);
        }
        else
        {
            return g_seekTarget;
        }
    }
    const double current = g_engine->GetCurrentTime();
    if (current != current || current < 0.0)
    {
        return 0.0;
    }
    return current;
}

double Audio_DurationSeconds()
{
    if (g_engine == nullptr || !g_hasMedia)
    {
        return 0.0;
    }
    const double duration = g_engine->GetDuration();
    // NaN before metadata, +inf for live streams.
    if (duration != duration || duration <= 0.0 || duration > 1.0e9)
    {
        return 0.0;
    }
    return duration;
}

void Audio_SeekSeconds(double seconds)
{
    if (g_engine == nullptr || !g_hasMedia)
    {
        return;
    }
    if (seconds < 0.0)
    {
        seconds = 0.0;
    }
    const double duration = Audio_DurationSeconds();
    if (duration > 0.0 && seconds > duration)
    {
        seconds = duration;
    }
    InterlockedExchange(&g_ended, 0);
    InterlockedExchange(&g_seekingFlag, 1);
    g_seekTarget = seconds;
    g_seekStartedMs = GetTickCount();
    g_engine->SetCurrentTime(seconds);
}

void Audio_SetMuted(bool muted)
{
    g_muted = muted;
    AudioLogf("SetMuted %d", muted ? 1 : 0);
    if (g_engine != nullptr)
    {
        g_engine->SetMuted(muted ? TRUE : FALSE);
    }
}

void Audio_LastError(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    lstrcpynA(out, g_lastError, outChars);
}

bool Audio_Autoplay()
{
    return g_autoplay;
}

void Audio_SetAutoplay(bool on)
{
    // Autoplay is always on for the track rail.
    (void)on;
    g_autoplay = true;
}

bool Audio_Loop()
{
    return g_loop;
}

void Audio_SetLoop(bool on)
{
    g_loop = on;
    Store_SetInt("audio.loop", on ? 1 : 0);
}

bool Audio_Shuffle()
{
    return g_shuffle;
}

void Audio_SetShuffle(bool on)
{
    g_shuffle = on;
    Store_SetInt("audio.shuffle", on ? 1 : 0);
}

static int PickNextTrackId(int playlistId, int currentId)
{
    AudioTrack tracks[AUDIO_TRACK_MAX] = {};
    const int count = Audio_ListTracks(playlistId, tracks, AUDIO_TRACK_MAX);
    if (count <= 0)
    {
        return 0;
    }

    int currentIndex = -1;
    for (int i = 0; i < count; ++i)
    {
        if (tracks[i].id == currentId)
        {
            currentIndex = i;
            break;
        }
    }

    if (g_shuffle)
    {
        if (count == 1)
        {
            return g_loop ? tracks[0].id : 0;
        }
        int pick = rand() % count;
        if (tracks[pick].id == currentId)
        {
            pick = (pick + 1) % count;
        }
        return tracks[pick].id;
    }

    if (currentIndex < 0)
    {
        return tracks[0].id;
    }
    if (currentIndex + 1 < count)
    {
        return tracks[currentIndex + 1].id;
    }
    return g_loop ? tracks[0].id : 0;
}

void Audio_TickAdvance()
{
    if (InterlockedCompareExchange(&g_ended, 0, 1) != 1)
    {
        return;
    }

    const int currentId = g_playingTrackId;
    const int playlistId = g_activePlaylistId;
    if (currentId <= 0 || playlistId <= 0)
    {
        return;
    }

    const int nextId = PickNextTrackId(playlistId, currentId);
    if (nextId > 0)
    {
        Audio_PlayTrack(nextId);
    }
    else if (g_loop)
    {
        Audio_PlayTrack(currentId);
    }
}

void Audio_Shutdown()
{
    AudioLogf("Shutdown");
    if (g_engine != nullptr)
    {
        InterlockedExchange(&g_wantPlay, 0);
        g_engine->Shutdown();
    }
    if (g_engineEx != nullptr)
    {
        g_engineEx->Release();
        g_engineEx = nullptr;
    }
    if (g_engine != nullptr)
    {
        g_engine->Release();
        g_engine = nullptr;
    }
    if (g_notify != nullptr)
    {
        g_notify->Release();
        g_notify = nullptr;
    }
    g_hasMedia = false;
    g_playingTrackId = 0;
    g_layerActive = false;
    if (g_mfStarted)
    {
        MFShutdown();
        g_mfStarted = false;
    }
}

bool Audio_ResolveTrackPath(int trackId, char* outPath, int outChars)
{
    if (outPath == nullptr || outChars < 2 || trackId <= 0)
    {
        return false;
    }
    outPath[0] = '\0';

    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT path FROM audio_tracks WHERE id = ?1 LIMIT 1;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return false;
    }
    sqlite3_bind_int(statement, 1, trackId);
    bool ok = false;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        const char* rel = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        char root[MAX_PATH] = {};
        Store_Root(root, MAX_PATH);
        if (rel != nullptr && rel[0] != '\0' && root[0] != '\0')
        {
            JoinPath(outPath, outChars, root, rel);
            ok = (GetFileAttributesA(outPath) != INVALID_FILE_ATTRIBUTES);
        }
    }
    sqlite3_finalize(statement);
    return ok;
}

bool Audio_PlayTrack(int trackId)
{
    g_lastError[0] = '\0';

    char pathA[MAX_PATH] = {};
    if (!Audio_ResolveTrackPath(trackId, pathA, MAX_PATH))
    {
        sprintf_s(g_lastError, sizeof(g_lastError), "Track file not found:\n%s", pathA);
        AudioLogf("PlayTrack %d: file missing (%s)", trackId, pathA);
        return false;
    }
    AudioLogf("PlayTrack %d: %s", trackId, pathA);

    if (!EnsureEngine())
    {
        return false;
    }

    wchar_t pathW[MAX_PATH] = {};
    MultiByteToWideChar(CP_ACP, 0, pathA, -1, pathW, MAX_PATH);

    UnloadEngineSource();

    wchar_t url[MAX_PATH * 4] = {};
    BuildFileUrl(pathW, url, MAX_PATH * 4);
    BSTR source = SysAllocString(url);
    if (source == nullptr)
    {
        lstrcpynA(g_lastError, "Out of memory building media URL.", sizeof(g_lastError));
        return false;
    }

    // Prefer a byte stream on the actual file (no URL parsing involved);
    // fall back to the URL form the video engine uses.
    HRESULT hr = E_FAIL;
    IMFByteStream* stream = nullptr;
    const HRESULT fileHr = MFCreateFile(
        MF_ACCESSMODE_READ,
        MF_OPENMODE_FAIL_IF_NOT_EXIST,
        MF_FILEFLAGS_NONE,
        pathW,
        &stream
    );
    AudioLogf("  MFCreateFile hr=0x%08lX", static_cast<unsigned long>(fileHr));
    if (SUCCEEDED(fileHr) && stream != nullptr && g_engineEx != nullptr)
    {
        hr = g_engineEx->SetSourceFromByteStream(stream, source);
        AudioLogf("  SetSourceFromByteStream hr=0x%08lX", static_cast<unsigned long>(hr));
    }
    if (stream != nullptr)
    {
        stream->Release();
    }
    if (FAILED(hr))
    {
        hr = g_engine->SetSource(source);
        AudioLogf("  SetSource(%ls) hr=0x%08lX", url, static_cast<unsigned long>(hr));
    }
    SysFreeString(source);
    if (FAILED(hr))
    {
        sprintf_s(g_lastError, sizeof(g_lastError), "SetSource failed (hr 0x%08lX)", static_cast<unsigned long>(hr));
        return false;
    }

    g_hasMedia = true;
    InterlockedExchange(&g_wantPlay, 1);
    hr = g_engine->Load();
    AudioLogf("  Load hr=0x%08lX", static_cast<unsigned long>(hr));
    g_engine->SetMuted(g_muted ? TRUE : FALSE);
    g_engine->SetVolume(1.0);

    // Wait briefly for metadata (or a decode/format error) so a bad file is
    // reported right away instead of silently doing nothing.
    const DWORD started = GetTickCount();
    while ((GetTickCount() - started) < 2000)
    {
        if (g_errorFlag != 0)
        {
            break;
        }
        if (g_engine->GetReadyState() >= MF_MEDIA_ENGINE_READY_HAVE_METADATA)
        {
            break;
        }
        Sleep(10);
    }

    if (g_errorFlag != 0)
    {
        AudioLogf("  load failed: %s", g_lastError);
        UnloadEngineSource();
        return false;
    }

    hr = g_engine->Play();
    AudioLogf(
        "  Play hr=0x%08lX readyState=%lu duration=%.2f waited=%lums",
        static_cast<unsigned long>(hr),
        static_cast<unsigned long>(g_engine->GetReadyState()),
        Audio_DurationSeconds(),
        static_cast<unsigned long>(GetTickCount() - started)
    );

    g_playingTrackId = trackId;
    g_layerActive = true;
    return true;
}

int Audio_PlayingTrackId()
{
    return g_playingTrackId;
}
