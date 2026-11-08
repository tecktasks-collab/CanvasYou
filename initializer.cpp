#include <windows.h>
#include <string.h>
#include <d3d11.h>
#include "init.h"
#include "store.h"
#include "boot.h"
#include "gpu_protocol.h"
#include "session_recorder.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "d3d11.lib")

#include <gdiplus.h>

static bool ProbeFullGraphics()
{
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
    {
        return false;
    }

    Gdiplus::GdiplusShutdown(token);
    return true;
}

static void FolderOfThisExe(char* folder, int folderChars)
{
    folder[0] = '\0';
    if (GetModuleFileNameA(nullptr, folder, folderChars) == 0)
    {
        return;
    }

    char* slash = strrchr(folder, '\\');
    if (slash != nullptr)
    {
        slash[1] = '\0';
    }
}

static bool ProbeDirect3D()
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    const HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &device,
        nullptr,
        &context
    );
    if (FAILED(hr))
    {
        return false;
    }

    context->Release();
    device->Release();
    return true;
}

static bool StartChild(const char* exeName, const char* args, PROCESS_INFORMATION* outProcess, bool wait)
{
    char folder[MAX_PATH] = {};
    FolderOfThisExe(folder, MAX_PATH);

    char exe[MAX_PATH] = {};
    lstrcpynA(exe, folder, MAX_PATH);
    lstrcatA(exe, exeName);

    char command[MAX_PATH + 64] = {};
    wsprintfA(command, "\"%s\" %s", exe, args);

    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};

    if (!CreateProcessA(exe, command, nullptr, nullptr, FALSE, 0, nullptr, folder, &startup, &process))
    {
        char text[300] = {};
        wsprintfA(text, "Could not start %s.", exeName);
        MessageBoxA(nullptr, text, "Initializer", MB_OK | MB_ICONERROR);
        return false;
    }

    CloseHandle(process.hThread);
    if (wait)
    {
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hProcess);
        if (outProcess != nullptr)
        {
            ZeroMemory(outProcess, sizeof(*outProcess));
        }
    }
    else if (outProcess != nullptr)
    {
        *outProcess = process;
    }
    else
    {
        CloseHandle(process.hProcess);
    }

    return true;
}

static bool ProcessStillRunning(HANDLE process)
{
    if (process == nullptr || process == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    DWORD code = 0;
    if (!GetExitCodeProcess(process, &code))
    {
        return false;
    }
    return code == STILL_ACTIVE;
}

// Returns true if Gpu signaled ready (pipe listening).
static bool WaitForGpuReady(HANDLE gpuProcess, DWORD timeoutMs)
{
    if (!ProcessStillRunning(gpuProcess))
    {
        MessageBoxA(
            nullptr,
            "Gpu.exe exited immediately after launch.\n"
            "Video playback will not work.",
            "Initializer",
            MB_OK | MB_ICONERROR
        );
        return false;
    }

    HANDLE ready = CreateEventA(nullptr, TRUE, FALSE, GPU_READY_EVENT_NAME);
    if (ready == nullptr)
    {
        Sleep(750);
        return ProcessStillRunning(gpuProcess);
    }

    HANDLE waits[2] = { ready, gpuProcess };
    const DWORD result = WaitForMultipleObjects(2, waits, FALSE, timeoutMs);
    CloseHandle(ready);

    if (result == WAIT_OBJECT_0)
    {
        return true;
    }

    if (result == WAIT_OBJECT_0 + 1)
    {
        MessageBoxA(
            nullptr,
            "Gpu.exe started but exited before it was ready.\n"
            "Video playback will not work.\n\n"
            "Check that Direct3D / Media Foundation can start.",
            "Initializer",
            MB_OK | MB_ICONERROR
        );
        return false;
    }

    if (ProcessStillRunning(gpuProcess))
    {
        MessageBoxA(
            nullptr,
            "Timed out waiting for Gpu.exe to become ready.\n"
            "Continuing anyway — video may still connect.",
            "Initializer",
            MB_OK | MB_ICONWARNING
        );
        return true;
    }

    MessageBoxA(
        nullptr,
        "Gpu.exe did not stay running.\nVideo playback will not work.",
        "Initializer",
        MB_OK | MB_ICONERROR
    );
    return false;
}

static void StopGpu(PROCESS_INFORMATION* gpu)
{
    if (gpu == nullptr || gpu->hProcess == nullptr)
    {
        return;
    }
    if (ProcessStillRunning(gpu->hProcess))
    {
        TerminateProcess(gpu->hProcess, 0);
    }
    CloseHandle(gpu->hProcess);
    gpu->hProcess = nullptr;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // Physical-pixel coordinates for the session recorder's screen captures.
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
    {
        SetProcessDPIAware();
    }

    HANDLE gate = CreateMutexA(nullptr, TRUE, APP_INIT_MUTEX_NAME);
    if (gate == nullptr)
    {
        MessageBoxA(nullptr, "Could not create the initializer process gate.", "Initializer", MB_OK | MB_ICONERROR);
        return 0;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(gate);
        MessageBoxA(nullptr, "Initializer is already running.", "Initializer", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // Records from the preferences saved in the store. No launch notice.
    Recorder_Start();

    if (Store_Open())
    {
        BootTicket ticket = {};
        Boot_Defaults(&ticket);
        Store_Root(ticket.dataRoot, MAX_PATH);
        ticket.leftWidth = Store_GetInt("program.left_width", 240);
        ticket.rightWidth = Store_GetInt("program.right_width", 240);
        Store_GetString("last.document", ticket.document, MAX_PATH, "");
        ticket.screenCount = Store_GetInt("screens.count", 0);
        if (ticket.screenCount < 0)
        {
            ticket.screenCount = 0;
        }
        if (ticket.screenCount > BOOT_MAX_SCREENS)
        {
            ticket.screenCount = BOOT_MAX_SCREENS;
        }
        for (int i = 0; i < ticket.screenCount; ++i)
        {
            char key[32] = {};
            wsprintfA(key, "screens.%d.device", i);
            Store_GetString(key, ticket.screens[i], 64, "");
        }
        Boot_Write(&ticket);
        Store_Close();
    }

    PROCESS_INFORMATION gpu = {};
    const bool gdiOk = ProbeFullGraphics();
    const bool d3dOk = gdiOk && ProbeDirect3D();

    if (d3dOk)
    {
        // Gpu must start with Initializer — before Window — and stay up for the session.
        if (!StartChild("Gpu.exe", APP_INIT_ARG, &gpu, false))
        {
            MessageBoxA(
                nullptr,
                "Could not start Gpu.exe.\nLaunching Window without GPU video support.",
                "Initializer",
                MB_OK | MB_ICONWARNING
            );
            StartChild("Window.exe", APP_INIT_ARG " --full", nullptr, true);
        }
        else
        {
            WaitForGpuReady(gpu.hProcess, 10000);
            StartChild("Window.exe", APP_INIT_ARG " --full", nullptr, true);
            StopGpu(&gpu);
        }
    }
    else if (gdiOk)
    {
        MessageBoxA(
            nullptr,
            "Direct3D is not available.\n"
            "Starting without Gpu.exe — video playback will be unavailable.",
            "Initializer",
            MB_OK | MB_ICONWARNING
        );
        StartChild("Window.exe", APP_INIT_ARG " --full", nullptr, true);
    }
    else
    {
        StartChild("Basic.exe", "", nullptr, true);
    }

    Recorder_Stop();
    CloseHandle(gate);
    return 0;
}
