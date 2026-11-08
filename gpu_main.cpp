#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include "init.h"
#include "d3d_video.h"
#include "gpu_protocol.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")

#define WM_GPU_MESSAGE (WM_APP + 40)
// A video frame is shown on the first tick after it is due, so the tick has to
// be well under one display refresh. WM_TIMER cannot do this: it is rounded up
// to the ~15.6 ms system clock, which made a 16 ms timer fire every ~31 ms.
#define GPU_TICK_MS 4
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static HANDLE g_pipeThread = nullptr;
static HWND g_hwnd = nullptr;
static HANDLE g_readyEvent = nullptr;
static bool g_running = true;

static bool PassedInitializer(const char* cmdLine)
{
    if (cmdLine == nullptr || strstr(cmdLine, APP_INIT_ARG) == nullptr)
    {
        return false;
    }

    HANDLE gate = OpenMutexA(SYNCHRONIZE, FALSE, APP_INIT_MUTEX_NAME);
    if (gate == nullptr)
    {
        return false;
    }

    CloseHandle(gate);
    return true;
}

static void SignalGpuReady()
{
    if (g_readyEvent == nullptr)
    {
        g_readyEvent = CreateEventA(nullptr, TRUE, FALSE, GPU_READY_EVENT_NAME);
    }
    if (g_readyEvent != nullptr)
    {
        SetEvent(g_readyEvent);
    }
}

static DWORD WINAPI PipeThread(LPVOID)
{
    while (g_running)
    {
        HANDLE pipe = CreateNamedPipeA(
            GPU_PIPE_NAME,
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            sizeof(GpuMessage),
            sizeof(GpuMessage),
            0,
            nullptr
        );
        if (pipe == INVALID_HANDLE_VALUE)
        {
            Sleep(50);
            continue;
        }

        g_pipe = pipe;
        SignalGpuReady();

        if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED)
        {
            GpuMessage message = {};
            DWORD read = 0;
            while (g_running && ReadFile(pipe, &message, sizeof(message), &read, nullptr))
            {
                if (read != sizeof(message))
                {
                    continue;
                }

                GpuMessage* copy = new GpuMessage(message);
                if (!PostMessage(g_hwnd, WM_GPU_MESSAGE, 0, reinterpret_cast<LPARAM>(copy)))
                {
                    delete copy;
                }
                if (message.cmd == GPU_CMD_QUIT)
                {
                    break;
                }
            }
        }

        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }

    return 0;
}

static LRESULT CALLBACK GpuProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_GPU_MESSAGE:
        {
            GpuMessage* message = reinterpret_cast<GpuMessage*>(lParam);
            if (message != nullptr)
            {
                if (message->cmd == GPU_CMD_QUIT)
                {
                    DestroyWindow(hwnd);
                }
                else
                {
                    D3DVideo_Apply(message);
                }
                delete message;
            }
            return 0;
        }

        case WM_CLOSE:
            // Keep GPU alive for the session; only Initializer/QUIT should exit.
            return 0;

        case WM_DESTROY:
            g_running = false;
            if (g_pipe != INVALID_HANDLE_VALUE)
            {
                CancelIoEx(g_pipe, nullptr);
            }
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR cmdLine, int)
{
    if (!PassedInitializer(cmdLine))
    {
        MessageBoxA(
            nullptr,
            "Gpu.exe does not start on its own.\nRun Initializer.exe.",
            "GPU",
            MB_OK | MB_ICONINFORMATION
        );
        return 0;
    }

    g_readyEvent = CreateEventA(nullptr, TRUE, FALSE, GPU_READY_EVENT_NAME);

    if (!D3DVideo_Startup())
    {
        MessageBoxA(nullptr, "Direct3D / Media Foundation failed to start.", "GPU", MB_OK | MB_ICONERROR);
        if (g_readyEvent != nullptr)
        {
            CloseHandle(g_readyEvent);
            g_readyEvent = nullptr;
        }
        return 0;
    }

    WNDCLASSA wc = {};
    wc.lpfnWndProc = GpuProc;
    wc.hInstance = instance;
    wc.lpszClassName = "PresentationGpuHost";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);

    // Hidden tool window: message/timer host only — stay off the taskbar.
    g_hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        "PresentationGpuHost",
        "GPU",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (g_hwnd == nullptr)
    {
        D3DVideo_Shutdown();
        if (g_readyEvent != nullptr)
        {
            CloseHandle(g_readyEvent);
            g_readyEvent = nullptr;
        }
        return 0;
    }

    ShowWindow(g_hwnd, SW_HIDE);

    // High-resolution timers need Windows 10 1803+. Older systems get a normal
    // waitable timer with the system clock raised to 1 ms for this process.
    bool raisedClock = false;
    HANDLE tick = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (tick == nullptr)
    {
        raisedClock = timeBeginPeriod(1) == TIMERR_NOERROR;
        tick = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
    LARGE_INTEGER due = {};
    due.QuadPart = -10000LL * GPU_TICK_MS;
    if (tick != nullptr)
    {
        SetWaitableTimer(tick, &due, GPU_TICK_MS, nullptr, nullptr, FALSE);
    }
    g_pipeThread = CreateThread(nullptr, 0, PipeThread, nullptr, 0, nullptr);

    HANDLE waits[2] = {};
    DWORD waitCount = 0;
    HANDLE vblank = D3DVideo_VBlankEvent();
    if (vblank != nullptr)
    {
        waits[waitCount++] = vblank;
    }
    if (tick != nullptr)
    {
        waits[waitCount++] = tick;
    }

    MSG msg = {};
    bool quit = false;
    while (!quit)
    {
        const DWORD waited = waitCount > 0
            ? MsgWaitForMultipleObjectsEx(waitCount, waits, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE)
            : MsgWaitForMultipleObjectsEx(0, nullptr, GPU_TICK_MS, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (waited < WAIT_OBJECT_0 + waitCount)
        {
            const bool fromVBlank = waits[waited - WAIT_OBJECT_0] == vblank;
            if (fromVBlank || !D3DVideo_PacedByVBlank())
            {
                D3DVideo_Tick();
            }
        }
        else if (waited == WAIT_TIMEOUT)
        {
            D3DVideo_Tick();
        }
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                quit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (tick != nullptr)
    {
        CancelWaitableTimer(tick);
        CloseHandle(tick);
    }
    if (raisedClock)
    {
        timeEndPeriod(1);
    }
    g_running = false;
    if (g_pipeThread != nullptr)
    {
        WaitForSingleObject(g_pipeThread, 1000);
        CloseHandle(g_pipeThread);
    }

    D3DVideo_Shutdown();
    if (g_readyEvent != nullptr)
    {
        CloseHandle(g_readyEvent);
        g_readyEvent = nullptr;
    }
    return 0;
}
