#include "pch.h"
#include "core/memory/Memory.h"
#include "core/console/Console.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/globals/Globals.h"
#include "core/player/PlayerHandler.h"
#include "features/visuals/RaycastEngine.h"
#include "features/misc/PlayerAvatars.h"
#include "renderer/Renderer.h"
#include <thread>
#include <chrono>
#include <cstdio>
#include <exception>

// Log an unhandled exception (code + fault address) to %LOCALAPPDATA%\Ardvark\crash.txt
// so a silent external crash can be diagnosed. Returns EXCEPTION_EXECUTE_HANDLER to
// terminate normally (the process would die anyway), but we captured the details first.
static void CrashAppend(const char* msg)
{
	char dir[MAX_PATH]; dir[0] = 0;
	GetEnvironmentVariableA("LOCALAPPDATA", dir, MAX_PATH);
	if (dir[0] == 0)
		strcpy_s(dir, ".");

	char sub[MAX_PATH];
	snprintf(sub, sizeof(sub), "%s\\Ardvark", dir);
	CreateDirectoryA(sub, nullptr);

	char filepath[MAX_PATH];
	snprintf(filepath, sizeof(filepath), "%s\\crash.txt", sub);

	HANDLE h = CreateFileA(filepath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h != INVALID_HANDLE_VALUE)
	{
		SetFilePointer(h, 0, nullptr, FILE_END);
		DWORD w = 0;
		WriteFile(h, msg, (DWORD)strlen(msg), &w, nullptr);
		CloseHandle(h);
	}
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep)
{
	char line[256];
	const int n = snprintf(line, sizeof(line), "code=0x%08lX addr=0x%llX\n",
		(unsigned long)(ep ? ep->ExceptionRecord->ExceptionCode : 0),
		(unsigned long long)(ep ? (uintptr_t)ep->ExceptionRecord->ExceptionAddress : 0));
	CrashAppend(line);
	return EXCEPTION_EXECUTE_HANDLER;
}

static void TerminateLog()
{
	// unhandled C++ exception path (std::bad_alloc, length_error, ...)
	CrashAppend("terminate: unhandled C++ exception\n");
	abort();
}

static void HeartbeatThread()
{
	for (;;)
	{
		// heartbeat.txt is overwritten every second. If it stops updating, the
		// process froze (deadlock) rather than crashed.
		char dir[MAX_PATH]; dir[0] = 0;
		GetEnvironmentVariableA("LOCALAPPDATA", dir, MAX_PATH);
		if (dir[0] == 0)
			strcpy_s(dir, ".");
		char sub[MAX_PATH];
		snprintf(sub, sizeof(sub), "%s\\Ardvark", dir);
		CreateDirectoryA(sub, nullptr);
		char filepath[MAX_PATH];
		snprintf(filepath, sizeof(filepath), "%s\\heartbeat.txt", sub);
		HANDLE h = CreateFileA(filepath, GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h != INVALID_HANDLE_VALUE)
		{
			const char* t = "tick\n";
			DWORD w = 0;
			WriteFile(h, t, (DWORD)strlen(t), &w, nullptr);
			CloseHandle(h);
		}
		Sleep(1000);
	}
}

// dx11 + меню в отдельном потоке
void OverlayThread()
{
    if (!Cheat::Renderer::Initialize(GetModuleHandle(nullptr)))
        return;

    Cheat::Renderer::MainLoop();
    Cheat::Renderer::Shutdown();
}

static void ResetGlobals()
{
    Cheat::Globals::ClientBase = 0;
    Cheat::Globals::FrontDataModel = 0;
    Cheat::Globals::InstanceDataModel.address = 0;
    Cheat::Globals::Workspace = nullptr;
    Cheat::Globals::Players = nullptr;
}

// крутимся пока роблокс не поднимется
static void WaitForRoblox()
{
    if (g_Memory.IsAttached() && g_Memory.IsAlive())
        return;

    if (g_Memory.IsAttached())
        g_Memory.Detach();

    Cheat::Console::Log(Cheat::Console::Color::Yellow, "waiting for roblox");
    while (!g_Memory.Attach(L"RobloxPlayerBeta.exe"))
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

static void OnRobloxAttached(bool reattached)
{
    Cheat::Console::Clear();
    Cheat::Console::Log(
        Cheat::Console::Color::Green,
        reattached ? "reattached" : "attached");
    Cheat::Console::Ptr(Cheat::Console::Color::Cyan, "Module", g_Memory.GetModuleBase());
    Cheat::Console::Log(Cheat::Console::Color::Gray, "PID            %lu",
        (unsigned long)g_Memory.GetPID());
    if (reattached)
        Cheat::Console::DumpLastCrash();
}

int main()
{
    SetProcessDPIAware();
    SetUnhandledExceptionFilter(CrashFilter);
    std::set_terminate(TerminateLog);
    std::thread(HeartbeatThread).detach();

    { /* setup overlay */
        std::thread(OverlayThread).detach();
    }

    { /* attach */
        WaitForRoblox();
        OnRobloxAttached(false);
        Cheat::PlayerHandler::StartCacheThread(); // кэш игроков
    }

    // роблокс сдох, ждём и цепляемся заново
    while (true)
    {
        if (!g_Memory.IsAlive())
        {
            DWORD code = 0;
            if (!g_Memory.GetExitCode(&code))
                code = 0xFFFFFFFF;

            Cheat::PlayerHandler::StopCacheThread();
            Cheat::PlayerHandler::ClearCache();
            Cheat::Features::PlayerAvatars::Clear();
            Cheat::Features::RaycastEngine::Reset();
            ResetGlobals();

            Cheat::Console::Clear();
            Cheat::Console::DumpCrash(code);

            g_Memory.Detach();
            WaitForRoblox();
            OnRobloxAttached(true);
            Cheat::PlayerHandler::StartCacheThread();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    Cheat::PlayerHandler::StopCacheThread();
    return 0;
}
