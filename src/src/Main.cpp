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
