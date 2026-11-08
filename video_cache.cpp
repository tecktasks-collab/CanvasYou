#include "video_cache.h"

#include <mfapi.h>
#include <mferror.h>
#include <share.h>
#include <stdarg.h>
#include <stdio.h>

#pragma comment(lib, "mfplat.lib")

#define CACHE_BLOCK_BYTES (4LL * 1024 * 1024)
#define CACHE_SLOTS_MAX 64
#define CACHE_BUDGET_BYTES (4LL * 1024 * 1024 * 1024)
#define CACHE_ENTRIES_MAX 128

// A video's RAM is a direct-mapped ring of 4 MB slots: file block b lives in
// slot b % slotCount. A file of 256 MB or less maps every block to its own
// slot, so it ends up fully in RAM. Longer files keep 256 MB ahead of the
// newest read, replacing blocks that playback has already passed.
struct CacheEntry
{
    bool used;
    bool failed;
    wchar_t path[MAX_PATH];
    LONGLONG size;
    FILETIME writeTime;
    LONGLONG totalBlocks;
    int slotCount;
    LONGLONG bytes;
    unsigned char* memory;
    LONGLONG tags[CACHE_SLOTS_MAX];
    volatile LONGLONG readHead;
    LONG users;
    int wantRank;
    DWORD lastUsed;
    DWORD fillStartMs;
    bool fullLogged;
    HANDLE file;
    SRWLOCK lock;
    volatile LONGLONG hitBytes;
    volatile LONGLONG missBytes;
};

static CacheEntry g_entries[CACHE_ENTRIES_MAX] = {};
static CRITICAL_SECTION g_tableLock;
static LONGLONG g_totalBytes = 0;
static HANDLE g_wake = nullptr;
static HANDLE g_thread = nullptr;
static volatile LONG g_stop = 0;
static bool g_started = false;

static void LogCache(const char* format, ...)
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

    char text[512] = {};
    va_list args;
    va_start(args, format);
    _vsnprintf_s(text, sizeof(text), _TRUNCATE, format, args);
    va_end(args);

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

static const wchar_t* FileNameOf(const wchar_t* path)
{
    const wchar_t* name = path;
    for (const wchar_t* p = path; *p != L'\0'; ++p)
    {
        if (*p == L'\\' || *p == L'/')
        {
            name = p + 1;
        }
    }
    return name;
}

static bool ReadAt(HANDLE file, LONGLONG offset, void* dest, DWORD bytes, DWORD* got)
{
    OVERLAPPED ov = {};
    ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
    *got = 0;
    return ReadFile(file, dest, bytes, got, &ov) == TRUE;
}

static HANDLE OpenVideoFile(const wchar_t* path, DWORD flags)
{
    return CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | flags,
        nullptr
    );
}

static void FreeEntry(CacheEntry* entry)
{
    if (entry->memory != nullptr)
    {
        VirtualFree(entry->memory, 0, MEM_RELEASE);
        entry->memory = nullptr;
        g_totalBytes -= entry->bytes;
    }
    if (entry->file != INVALID_HANDLE_VALUE && entry->file != nullptr)
    {
        CloseHandle(entry->file);
    }
    entry->file = INVALID_HANDLE_VALUE;
    entry->used = false;
    entry->bytes = 0;
    entry->slotCount = 0;
}

// Caller holds g_tableLock.
static bool EvictFor(LONGLONG bytes)
{
    while (g_totalBytes + bytes > CACHE_BUDGET_BYTES)
    {
        CacheEntry* victim = nullptr;
        for (int i = 0; i < CACHE_ENTRIES_MAX; ++i)
        {
            CacheEntry* entry = &g_entries[i];
            if (!entry->used || entry->memory == nullptr || entry->users > 0 || entry->wantRank >= 0)
            {
                continue;
            }
            if (victim == nullptr || (LONG)(entry->lastUsed - victim->lastUsed) < 0)
            {
                victim = entry;
            }
        }
        if (victim == nullptr)
        {
            return false;
        }
        LogCache("cache evict %ls (%lld MB)", FileNameOf(victim->path), victim->bytes >> 20);
        FreeEntry(victim);
    }
    return true;
}

// Caller holds g_tableLock.
static CacheEntry* FindOrCreate(const wchar_t* rawPath)
{
    wchar_t path[MAX_PATH] = {};
    const DWORD got = GetFullPathNameW(rawPath, MAX_PATH, path, nullptr);
    if (got == 0 || got >= MAX_PATH)
    {
        lstrcpynW(path, rawPath, MAX_PATH);
    }

    WIN32_FILE_ATTRIBUTE_DATA info = {};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &info) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return nullptr;
    }
    const LONGLONG size = (static_cast<LONGLONG>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    if (size <= 0)
    {
        return nullptr;
    }

    CacheEntry* freeSlot = nullptr;
    for (int i = 0; i < CACHE_ENTRIES_MAX; ++i)
    {
        CacheEntry* entry = &g_entries[i];
        if (!entry->used)
        {
            if (freeSlot == nullptr)
            {
                freeSlot = entry;
            }
            continue;
        }
        if (lstrcmpiW(entry->path, path) != 0)
        {
            continue;
        }
        const bool changed = entry->size != size ||
            CompareFileTime(&entry->writeTime, &info.ftLastWriteTime) != 0;
        if (!changed || entry->users > 0)
        {
            entry->lastUsed = GetTickCount();
            return entry;
        }
        FreeEntry(entry);
        freeSlot = entry;
        break;
    }

    if (freeSlot == nullptr)
    {
        for (int i = 0; i < CACHE_ENTRIES_MAX; ++i)
        {
            CacheEntry* entry = &g_entries[i];
            if (entry->users > 0 || entry->wantRank >= 0)
            {
                continue;
            }
            if (freeSlot == nullptr || (LONG)(entry->lastUsed - freeSlot->lastUsed) < 0)
            {
                freeSlot = entry;
            }
        }
        if (freeSlot == nullptr)
        {
            return nullptr;
        }
        FreeEntry(freeSlot);
    }

    CacheEntry* entry = freeSlot;
    ZeroMemory(entry, sizeof(*entry));
    entry->used = true;
    lstrcpynW(entry->path, path, MAX_PATH);
    entry->size = size;
    entry->writeTime = info.ftLastWriteTime;
    entry->totalBlocks = (size + CACHE_BLOCK_BYTES - 1) / CACHE_BLOCK_BYTES;
    entry->wantRank = -1;
    entry->lastUsed = GetTickCount();
    entry->file = INVALID_HANDLE_VALUE;
    InitializeSRWLock(&entry->lock);
    for (int i = 0; i < CACHE_SLOTS_MAX; ++i)
    {
        entry->tags[i] = -1;
    }

    const int slots = entry->totalBlocks < CACHE_SLOTS_MAX ? static_cast<int>(entry->totalBlocks) : CACHE_SLOTS_MAX;
    const LONGLONG bytes = entry->totalBlocks <= CACHE_SLOTS_MAX ? size : CACHE_SLOTS_MAX * CACHE_BLOCK_BYTES;
    entry->file = OpenVideoFile(path, FILE_FLAG_SEQUENTIAL_SCAN);
    if (entry->file != INVALID_HANDLE_VALUE && EvictFor(bytes))
    {
        entry->memory = static_cast<unsigned char*>(
            VirtualAlloc(nullptr, static_cast<SIZE_T>(bytes), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
        );
    }
    if (entry->memory != nullptr)
    {
        entry->slotCount = slots;
        entry->bytes = bytes;
        g_totalBytes += bytes;
    }
    else
    {
        LogCache("cache pass-through %ls (no room)", FileNameOf(path));
    }
    return entry;
}

static bool BlockCached(CacheEntry* entry, LONGLONG block)
{
    if (entry->slotCount <= 0)
    {
        return true;
    }
    AcquireSRWLockShared(&entry->lock);
    const bool hit = entry->tags[block % entry->slotCount] == block;
    ReleaseSRWLockShared(&entry->lock);
    return hit;
}

// Next block that should be in RAM: ahead of playback for open streams first,
// then the start of every wanted video in priority order.
static CacheEntry* PickWork(LONGLONG* outBlock)
{
    CacheEntry* best = nullptr;
    LONGLONG bestBlock = -1;
    int bestRank = 0x7FFFFFFF;

    EnterCriticalSection(&g_tableLock);
    for (int pass = 0; pass < 2 && best == nullptr; ++pass)
    {
        for (int i = 0; i < CACHE_ENTRIES_MAX; ++i)
        {
            CacheEntry* entry = &g_entries[i];
            if (!entry->used || entry->failed || entry->slotCount <= 0)
            {
                continue;
            }
            LONGLONG first = 0;
            if (pass == 0)
            {
                if (entry->users <= 0)
                {
                    continue;
                }
                first = entry->readHead / CACHE_BLOCK_BYTES;
            }
            else
            {
                if (entry->wantRank < 0 || entry->wantRank >= bestRank)
                {
                    continue;
                }
            }
            LONGLONG last = first + entry->slotCount;
            if (last > entry->totalBlocks)
            {
                last = entry->totalBlocks;
            }
            for (LONGLONG b = first; b < last; ++b)
            {
                if (!BlockCached(entry, b))
                {
                    best = entry;
                    bestBlock = b;
                    bestRank = entry->wantRank;
                    break;
                }
            }
            if (pass == 0 && best != nullptr)
            {
                break;
            }
        }
    }
    if (best != nullptr)
    {
        best->users += 1;
        if (best->fillStartMs == 0)
        {
            best->fillStartMs = GetTickCount();
        }
    }
    LeaveCriticalSection(&g_tableLock);

    *outBlock = bestBlock;
    return best;
}

static void ReleaseUser(CacheEntry* entry)
{
    EnterCriticalSection(&g_tableLock);
    entry->users -= 1;
    entry->lastUsed = GetTickCount();
    LeaveCriticalSection(&g_tableLock);
}

static void FillBlock(CacheEntry* entry, LONGLONG block)
{
    const int slot = static_cast<int>(block % entry->slotCount);
    AcquireSRWLockExclusive(&entry->lock);
    entry->tags[slot] = -1;
    ReleaseSRWLockExclusive(&entry->lock);

    const LONGLONG offset = block * CACHE_BLOCK_BYTES;
    LONGLONG want = entry->size - offset;
    if (want > CACHE_BLOCK_BYTES)
    {
        want = CACHE_BLOCK_BYTES;
    }
    DWORD got = 0;
    const bool ok = ReadAt(entry->file, offset, entry->memory + (slot * CACHE_BLOCK_BYTES), static_cast<DWORD>(want), &got) &&
        got == static_cast<DWORD>(want);

    AcquireSRWLockExclusive(&entry->lock);
    entry->tags[slot] = ok ? block : -1;
    ReleaseSRWLockExclusive(&entry->lock);

    if (!ok)
    {
        entry->failed = true;
        LogCache("cache read failed %ls block %lld", FileNameOf(entry->path), block);
        return;
    }

    const LONGLONG fullBlocks = entry->totalBlocks < entry->slotCount ? entry->totalBlocks : entry->slotCount;
    if (!entry->fullLogged && block == fullBlocks - 1)
    {
        bool all = true;
        for (LONGLONG b = 0; b < fullBlocks && all; ++b)
        {
            all = BlockCached(entry, b);
        }
        if (all)
        {
            entry->fullLogged = true;
            LogCache(
                "cache ready %ls: %lld MB in RAM (%s) in %lu ms, total %lld MB",
                FileNameOf(entry->path),
                entry->bytes >> 20,
                entry->totalBlocks <= CACHE_SLOTS_MAX ? "whole file" : "first 256 MB",
                GetTickCount() - entry->fillStartMs,
                g_totalBytes >> 20
            );
        }
    }
}

static DWORD WINAPI PrefetchThread(LPVOID)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    while (g_stop == 0)
    {
        LONGLONG block = -1;
        CacheEntry* entry = PickWork(&block);
        if (entry == nullptr)
        {
            WaitForSingleObject(g_wake, 100);
            continue;
        }
        FillBlock(entry, block);
        ReleaseUser(entry);
    }
    return 0;
}

bool VideoCache_Startup()
{
    if (g_started)
    {
        return true;
    }
    InitializeCriticalSection(&g_tableLock);
    for (int i = 0; i < CACHE_ENTRIES_MAX; ++i)
    {
        g_entries[i].file = INVALID_HANDLE_VALUE;
    }
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_stop = 0;
    g_thread = CreateThread(nullptr, 0, PrefetchThread, nullptr, 0, nullptr);
    g_started = g_wake != nullptr && g_thread != nullptr;
    return g_started;
}

void VideoCache_Shutdown()
{
    if (!g_started)
    {
        return;
    }
    InterlockedExchange(&g_stop, 1);
    SetEvent(g_wake);
    WaitForSingleObject(g_thread, 2000);
    CloseHandle(g_thread);
    g_thread = nullptr;
    EnterCriticalSection(&g_tableLock);
    for (int i = 0; i < CACHE_ENTRIES_MAX; ++i)
    {
        if (g_entries[i].used)
        {
            FreeEntry(&g_entries[i]);
        }
    }
    LeaveCriticalSection(&g_tableLock);
    CloseHandle(g_wake);
    g_wake = nullptr;
    DeleteCriticalSection(&g_tableLock);
    g_started = false;
}

void VideoCache_Want(const wchar_t (*paths)[MAX_PATH], int count)
{
    if (!g_started)
    {
        return;
    }
    EnterCriticalSection(&g_tableLock);
    for (int i = 0; i < CACHE_ENTRIES_MAX; ++i)
    {
        g_entries[i].wantRank = -1;
    }
    int wanted = 0;
    for (int i = 0; i < count; ++i)
    {
        if (paths[i][0] == L'\0' || wcsstr(paths[i], L"://") != nullptr)
        {
            continue;
        }
        CacheEntry* entry = FindOrCreate(paths[i]);
        if (entry != nullptr && entry->wantRank < 0)
        {
            entry->wantRank = i;
            wanted += 1;
        }
    }
    LogCache("cache want %d videos, %lld MB allocated", wanted, g_totalBytes >> 20);
    LeaveCriticalSection(&g_tableLock);
    SetEvent(g_wake);
}

static void CacheRead(CacheEntry* entry, HANDLE file, LONGLONG offset, BYTE* dest, ULONG length, ULONG* outRead, HRESULT* outHr)
{
    ULONG total = 0;
    bool missed = false;
    HRESULT hr = S_OK;
    while (length > 0 && offset < entry->size)
    {
        const LONGLONG block = offset / CACHE_BLOCK_BYTES;
        const LONGLONG inBlock = offset - (block * CACHE_BLOCK_BYTES);
        LONGLONG chunk = CACHE_BLOCK_BYTES - inBlock;
        if (chunk > length)
        {
            chunk = length;
        }
        if (chunk > entry->size - offset)
        {
            chunk = entry->size - offset;
        }

        bool hit = false;
        if (entry->slotCount > 0)
        {
            const int slot = static_cast<int>(block % entry->slotCount);
            AcquireSRWLockShared(&entry->lock);
            if (entry->tags[slot] == block)
            {
                memcpy(dest, entry->memory + (slot * CACHE_BLOCK_BYTES) + inBlock, static_cast<size_t>(chunk));
                hit = true;
            }
            ReleaseSRWLockShared(&entry->lock);
        }
        if (hit)
        {
            InterlockedExchangeAdd64(&entry->hitBytes, chunk);
        }
        else
        {
            DWORD got = 0;
            if (!ReadAt(file, offset, dest, static_cast<DWORD>(chunk), &got))
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
                break;
            }
            if (got == 0)
            {
                break;
            }
            chunk = got;
            missed = true;
            InterlockedExchangeAdd64(&entry->missBytes, chunk);
        }
        dest += chunk;
        offset += chunk;
        length -= static_cast<ULONG>(chunk);
        total += static_cast<ULONG>(chunk);
    }
    InterlockedExchange64(&entry->readHead, offset);
    if (missed)
    {
        SetEvent(g_wake);
    }
    *outRead = total;
    *outHr = hr;
}

class ReadResult : public IUnknown
{
public:
    explicit ReadResult(ULONG bytes) : refs(1), bytes(bytes) {}

    STDMETHODIMP QueryInterface(REFIID id, void** out)
    {
        if (out == nullptr)
        {
            return E_POINTER;
        }
        if (id == IID_IUnknown)
        {
            *out = static_cast<IUnknown*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&refs); }
    STDMETHODIMP_(ULONG) Release()
    {
        const LONG value = InterlockedDecrement(&refs);
        if (value == 0)
        {
            delete this;
        }
        return value;
    }

    LONG refs;
    ULONG bytes;
};

class CachedByteStream : public IMFByteStream
{
public:
    CachedByteStream(CacheEntry* entry, HANDLE file)
        : refs(1), entry(entry), file(file), size(entry->size), position(0), closed(false)
    {
        InitializeCriticalSection(&lock);
    }

    ~CachedByteStream()
    {
        Close();
        DeleteCriticalSection(&lock);
    }

    STDMETHODIMP QueryInterface(REFIID id, void** out)
    {
        if (out == nullptr)
        {
            return E_POINTER;
        }
        if (id == IID_IUnknown || id == __uuidof(IMFByteStream))
        {
            *out = static_cast<IMFByteStream*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&refs); }
    STDMETHODIMP_(ULONG) Release()
    {
        const LONG value = InterlockedDecrement(&refs);
        if (value == 0)
        {
            delete this;
        }
        return value;
    }

    STDMETHODIMP GetCapabilities(DWORD* caps)
    {
        if (caps == nullptr)
        {
            return E_POINTER;
        }
        *caps = MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE;
        return S_OK;
    }
    STDMETHODIMP GetLength(QWORD* length)
    {
        if (length == nullptr)
        {
            return E_POINTER;
        }
        *length = static_cast<QWORD>(size);
        return S_OK;
    }
    STDMETHODIMP SetLength(QWORD) { return E_NOTIMPL; }
    STDMETHODIMP GetCurrentPosition(QWORD* out)
    {
        if (out == nullptr)
        {
            return E_POINTER;
        }
        EnterCriticalSection(&lock);
        *out = static_cast<QWORD>(position);
        LeaveCriticalSection(&lock);
        return S_OK;
    }
    STDMETHODIMP SetCurrentPosition(QWORD value)
    {
        EnterCriticalSection(&lock);
        position = static_cast<LONGLONG>(value) > size ? size : static_cast<LONGLONG>(value);
        LeaveCriticalSection(&lock);
        return S_OK;
    }
    STDMETHODIMP IsEndOfStream(BOOL* end)
    {
        if (end == nullptr)
        {
            return E_POINTER;
        }
        EnterCriticalSection(&lock);
        *end = position >= size ? TRUE : FALSE;
        LeaveCriticalSection(&lock);
        return S_OK;
    }
    STDMETHODIMP Read(BYTE* buffer, ULONG length, ULONG* read)
    {
        if (buffer == nullptr || read == nullptr)
        {
            return E_POINTER;
        }
        EnterCriticalSection(&lock);
        if (closed)
        {
            LeaveCriticalSection(&lock);
            return MF_E_SHUTDOWN;
        }
        HRESULT hr = S_OK;
        CacheRead(entry, file, position, buffer, length, read, &hr);
        position += *read;
        LeaveCriticalSection(&lock);
        return hr;
    }
    STDMETHODIMP BeginRead(BYTE* buffer, ULONG length, IMFAsyncCallback* callback, IUnknown* state)
    {
        if (callback == nullptr || buffer == nullptr)
        {
            return E_POINTER;
        }
        ULONG got = 0;
        const HRESULT readHr = Read(buffer, length, &got);
        ReadResult* result = new ReadResult(got);
        IMFAsyncResult* async = nullptr;
        HRESULT hr = MFCreateAsyncResult(result, callback, state, &async);
        result->Release();
        if (FAILED(hr))
        {
            return hr;
        }
        async->SetStatus(readHr);
        hr = MFInvokeCallback(async);
        async->Release();
        return hr;
    }
    STDMETHODIMP EndRead(IMFAsyncResult* async, ULONG* read)
    {
        if (async == nullptr || read == nullptr)
        {
            return E_POINTER;
        }
        *read = 0;
        IUnknown* object = nullptr;
        if (SUCCEEDED(async->GetObject(&object)) && object != nullptr)
        {
            *read = static_cast<ReadResult*>(object)->bytes;
            object->Release();
        }
        return async->GetStatus();
    }
    STDMETHODIMP Write(const BYTE*, ULONG, ULONG*) { return E_NOTIMPL; }
    STDMETHODIMP BeginWrite(const BYTE*, ULONG, IMFAsyncCallback*, IUnknown*) { return E_NOTIMPL; }
    STDMETHODIMP EndWrite(IMFAsyncResult*, ULONG*) { return E_NOTIMPL; }
    STDMETHODIMP Seek(MFBYTESTREAM_SEEK_ORIGIN origin, LONGLONG offset, DWORD, QWORD* current)
    {
        EnterCriticalSection(&lock);
        LONGLONG next = origin == msoCurrent ? position + offset : offset;
        if (next < 0)
        {
            next = 0;
        }
        if (next > size)
        {
            next = size;
        }
        position = next;
        if (current != nullptr)
        {
            *current = static_cast<QWORD>(position);
        }
        LeaveCriticalSection(&lock);
        return S_OK;
    }
    STDMETHODIMP Flush() { return S_OK; }
    STDMETHODIMP Close()
    {
        EnterCriticalSection(&lock);
        if (!closed)
        {
            closed = true;
            if (file != INVALID_HANDLE_VALUE)
            {
                CloseHandle(file);
                file = INVALID_HANDLE_VALUE;
            }
            LogCache(
                "cache stream closed %ls: %lld MB from RAM, %lld MB from disk",
                FileNameOf(entry->path),
                entry->hitBytes >> 20,
                entry->missBytes >> 20
            );
            ReleaseUser(entry);
        }
        LeaveCriticalSection(&lock);
        return S_OK;
    }

private:
    LONG refs;
    CacheEntry* entry;
    HANDLE file;
    LONGLONG size;
    LONGLONG position;
    bool closed;
    CRITICAL_SECTION lock;
};

HRESULT VideoCache_OpenStream(const wchar_t* path, IMFByteStream** out)
{
    if (out == nullptr)
    {
        return E_POINTER;
    }
    *out = nullptr;
    if (!g_started || path == nullptr || path[0] == L'\0' || wcsstr(path, L"://") != nullptr)
    {
        return E_INVALIDARG;
    }

    EnterCriticalSection(&g_tableLock);
    CacheEntry* entry = FindOrCreate(path);
    if (entry == nullptr)
    {
        LeaveCriticalSection(&g_tableLock);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
    HANDLE file = OpenVideoFile(entry->path, FILE_FLAG_RANDOM_ACCESS);
    if (file == INVALID_HANDLE_VALUE)
    {
        LeaveCriticalSection(&g_tableLock);
        return HRESULT_FROM_WIN32(GetLastError());
    }
    entry->users += 1;
    entry->lastUsed = GetTickCount();
    LeaveCriticalSection(&g_tableLock);
    SetEvent(g_wake);

    *out = new CachedByteStream(entry, file);
    return S_OK;
}
