#include "pch.h"
#define NOMINMAX
#include "Console.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/player/PlayerHandler.h"
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <mutex>

namespace Cheat::Console {
namespace {

std::mutex g_mu;
bool g_vt = false;
bool g_have_last_crash = false;
unsigned long g_last_crash_code = 0;

void ensure_console()
{
    static bool once = false;
    if (once)
        return;
    once = true;

    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE)
        return;

    DWORD mode = 0;
    if (GetConsoleMode(h, &mode))
    {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        g_vt = SetConsoleMode(h, mode) != 0;
    }
}

void stamp(char* out, size_t n)
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::snprintf(out, n, "[%02u:%02u:%02u]",
        (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond);
}

WORD attr(Color c)
{
    return (WORD)c;
}

void write_colored(Color color, const char* text)
{
    ensure_console();
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        GetConsoleScreenBufferInfo(h, &csbi);
        SetConsoleTextAttribute(h, attr(color));
        std::fputs(text, stdout);
        SetConsoleTextAttribute(h, csbi.wAttributes);
    }

    else
    {
        std::fputs(text, stdout);
    }
}

void log_locked(Color color, const char* body)
{
    char t[16]{};
    stamp(t, sizeof(t));
    write_colored(Color::Dim, t);
    write_colored(Color::Dim, ": ");
    write_colored(color, body);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

enum class CrashSide {
    User,
    Cheat,
    Both,
    Info
};

struct CrashInfo {
    const char* name;
    CrashSide side;
    const char* fix;
    const char* dev;
};

// разбор exit code, что сказать юзеру / деву
CrashInfo classify_crash(unsigned long code)
{
    if (code == 0)
    {
        return {
            "Clean Exit",
            CrashSide::Info,
            "clean exit.",
            nullptr
        };
    }

    else if (code == 0xC000013A)
    {
        return {
            "Process Terminated",
            CrashSide::User,
            "killed from taskmgr / another app. reopen.",
            nullptr
        };
    }

    else if (code == 0xC0000135)
    {
        return {
            "DLL Not Found",
            CrashSide::User,
            "reinstall roblox + vcredist.",
            nullptr
        };
    }

    else if (code == 0xC0000141)
    {
        return {
            "Invalid Address",
            CrashSide::Cheat,
            "cfg/bad call. try off raycast silent / lua.",
            "raycast stub = in-module cave or xrw+cfg. luavm: call-rax;ret in-module."
        };
    }

    else if (code == 0xC0000142)
    {
        return {
            "DLL Init Failed",
            CrashSide::User,
            "dll init fail. update gpu, vcredist, reinstall roblox.",
            nullptr
        };
    }

    else if (code == 0xC000007B)
    {
        return {
            "Bad Image",
            CrashSide::User,
            "bad image. delete %localappdata%\\Roblox + reinstall.",
            nullptr
        };
    }

    else if (code == 0xC0000017 || code == 0xC000009A)
    {
        return {
            "Out Of Memory",
            CrashSide::User,
            "oom. close apps, lower gfx, reboot.",
            nullptr
        };
    }

    else if (code == 0xC0000006)
    {
        return {
            "In-Page Error",
            CrashSide::User,
            "disk/ram read fail. check disk + memdiag.",
            nullptr
        };
    }

    else if (code == 0xC0000185)
    {
        return {
            "IO Device Error",
            CrashSide::User,
            "io device error. check disk, reinstall roblox.",
            nullptr
        };
    }

    else if (code == 0xE06D7363)
    {
        return {
            "C++ Exception",
            CrashSide::User,
            "c++ exception. reinstall roblox usually fixes.",
            "if only with features on - note which."
        };
    }

    else if (code == 0xC0000005)
    {
        return {
            "Access Violation",
            CrashSide::Both,
            "av. update gpu, disable overlays, reinstall if keeps happening.",
            "often cheat: bad r/w or silent. off silent/fly/speed."
        };
    }

    else if (code == 0xC0000374)
    {
        return {
            "Heap Corruption",
            CrashSide::Cheat,
            "heap corrupt. features off + still? reinstall roblox.",
            "bad write (fly/speed/silent). bisect features."
        };
    }

    else if (code == 0xC0000409)
    {
        return {
            "Stack Buffer Overrun",
            CrashSide::Cheat,
            "stack smash. features off? reinstall roblox.",
            "usually silent inject. off silent, check stub."
        };
    }

    else if (code == 0xC00000FD)
    {
        return {
            "Stack Overflow",
            CrashSide::Cheat,
            "stack overflow. reboot; clean still crash -> reinstall.",
            "maybe hook recursion. check silent path."
        };
    }

    else if (code == 0xC000001D)
    {
        return {
            "Illegal Instruction",
            CrashSide::Both,
            "illegal insn. reinstall roblox; update windows.",
            "bad patched bytes in silent stub?"
        };
    }

    else if (code == 0xC0000096)
    {
        return {
            "Privileged Instruction",
            CrashSide::Cheat,
            "priv insn. av inject? add exclusions.",
            "bad patch / wrong rva."
        };
    }

    else if (code == 0xC000041D)
    {
        return {
            "Fatal Callback Exception",
            CrashSide::Both,
            "fatal callback. reboot, gpu drivers, overlays off, reinstall.",
            "hooks during window/input callbacks."
        };
    }

    if ((code & 0xF0000000u) == 0xC0000000u)
    {
        return {
            "NTSTATUS Crash",
            CrashSide::Both,
            "unknown ntstatus. reinstall roblox + disable overlays.",
            "log code + features on."
        };
    }

    return {
        "Unknown Exit",
        CrashSide::Both,
        "weird exit. reopen; reinstall if repeats.",
        "save exit code + features."
    };
}

const char* side_text(CrashSide side)
{
    if (side == CrashSide::User)
    {
        return "user (fixable)";
    }

    else if (side == CrashSide::Cheat)
    {
        return "cheat (dev)";
    }

    else if (side == CrashSide::Both)
    {
        return "user + cheat";
    }

    else if (side == CrashSide::Info)
    {
        return "info";
    }

    return "unknown";
}

void dump_crash_locked(unsigned long exit_code)
{
    CrashInfo info = classify_crash(exit_code);
    long long signed_code = (long long)(std::int32_t)exit_code;

    log_locked(Color::White, "jewsploit crash");
    {
        char body[160]{};
        std::snprintf(body, sizeof(body), "Exit code      0x%08lX (%lld)",
            exit_code, signed_code);
        log_locked(Color::Yellow, body);
    }
    {
        char body[160]{};
        std::snprintf(body, sizeof(body), "Name           %s", info.name);
        log_locked(Color::Orange, body);
    }
    {
        char body[160]{};
        std::snprintf(body, sizeof(body), "Side           %s", side_text(info.side));

        Color c = Color::Magenta;
        if (info.side == CrashSide::Cheat)
            c = Color::Red;

        else if (info.side == CrashSide::User)
            c = Color::Green;

        else if (info.side == CrashSide::Info)
            c = Color::Gray;

        log_locked(c, body);
    }
    if (info.fix && info.fix[0])
    {
        char body[640]{};
        std::snprintf(body, sizeof(body), "Fix            %s", info.fix);
        log_locked(Color::Lime, body);
    }
    if (info.dev && info.dev[0])
    {
        char body[640]{};
        std::snprintf(body, sizeof(body), "Dev            %s", info.dev);
        log_locked(Color::Cyan, body);
    }
}

} // namespace

void Clear()
{
    std::lock_guard<std::mutex> lock(g_mu);
    ensure_console();
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE)
        return;

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (!GetConsoleScreenBufferInfo(h, &csbi))
        return;

    const DWORD cells = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
    DWORD written = 0;
    COORD home{ 0, 0 };
    FillConsoleOutputCharacterA(h, ' ', cells, home, &written);
    FillConsoleOutputAttribute(h, csbi.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(h, home);
}

void Log(Color color, const char* fmt, ...)
{
    if (!fmt)
        return;

    char body[640]{};
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    std::lock_guard<std::mutex> lock(g_mu);
    log_locked(color, body);
}

void Log(const char* fmt, ...)
{
    if (!fmt)
        return;

    char body[640]{};
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    std::lock_guard<std::mutex> lock(g_mu);
    log_locked(Color::White, body);
}

void Ptr(Color color, const char* name, std::uint64_t addr)
{
    Log(color, "%-14s 0x%llX", name ? name : "?", (unsigned long long)addr);
}

void Ptr(const char* name, std::uint64_t addr)
{
    Ptr(Color::Cyan, name, addr);
}

void DumpCrash(unsigned long exit_code)
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_have_last_crash = true;
    g_last_crash_code = exit_code;
    dump_crash_locked(exit_code);
}

void DumpLastCrash()
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_have_last_crash)
        return;
    dump_crash_locked(g_last_crash_code);
}

// дамп мира, dm/ws/players/камера
void DumpWorld()
{
    uintptr_t base = g_Memory.GetModuleBase();
    DWORD pid = g_Memory.GetPID();

    std::uint64_t ve = 0;
    if (base)
        ve = g_Memory.Read<std::uint64_t>(base + ::VisualEngine::Pointer);

    std::uint64_t front = Globals::FrontDataModel;
    std::uint64_t dm = Globals::InstanceDataModel.address;
    std::uint64_t ws = Globals::Workspace ? Globals::Workspace->address : 0;
    std::uint64_t pl = Globals::Players ? Globals::Players->address : 0;

    std::uint64_t local_player = 0;
    std::uint64_t local_char = 0;
    std::uint64_t camera = 0;
    std::int64_t place_id = 0;
    std::int64_t game_id = 0;
    std::int64_t user_id = 0;
    std::uint64_t world = 0;

    if (dm)
    {
        place_id = g_Memory.Read<std::int64_t>(dm + ::DataModel::PlaceId);
        game_id = g_Memory.Read<std::int64_t>(dm + ::DataModel::GameId);
    }
    if (pl)
    {
        local_player = g_Memory.Read<std::uint64_t>(pl + ::Player::LocalPlayer);
        if (g_Memory.IsValid(local_player))
        {
            local_char = g_Memory.Read<std::uint64_t>(
                local_player + ::Player::ModelInstance);
            user_id = g_Memory.Read<std::int64_t>(
                local_player + ::Player::UserId);
        }
    }
    if (ws)
    {
        camera = g_Memory.Read<std::uint64_t>(ws + ::Workspace::CurrentCamera);
        world = g_Memory.Read<std::uint64_t>(ws + ::Workspace::World);
    }

    std::size_t players = PlayerHandler::GetPlayerCount();

    Log(Color::White, "jewsploit status");
    Log(Color::Gray, "PID            %lu", (unsigned long)pid);
    Ptr(Color::Cyan, "Module", base);
    Ptr(Color::Yellow, "Front DM", front);
    Ptr(Color::Green, "Data Model", dm);
    Ptr(Color::Magenta, "VisualEngine", ve);
    Ptr(Color::Blue, "Workspace", ws);
    Ptr(Color::Teal, "World", world);
    Ptr(Color::Sky, "Players", pl);
    Ptr(Color::Lime, "LocalPlayer", local_player);
    Ptr(Color::Purple, "Character", local_char);
    Ptr(Color::Pink, "Camera", camera);
    Log(Color::Orange, "PlaceId        %lld", (long long)place_id);
    Log(Color::Yellow, "GameId         %lld", (long long)game_id);
    Log(Color::Lime, "UserId         %lld", (long long)user_id);
    Log(Color::White, "Cached         %zu players", players);

    bool have_crash = false;
    unsigned long crash_code = 0;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        have_crash = g_have_last_crash;
        crash_code = g_last_crash_code;
    }
    if (have_crash)
    {
        CrashInfo info = classify_crash(crash_code);
        Log(Color::Red, "Last crash     0x%08lX  %s  [%s]",
            crash_code, info.name, side_text(info.side));
    }
}

void DumpSilent(bool ok, std::uint64_t handler, std::uint64_t stub,
                std::uint64_t state, std::uint64_t slot, const char* detail)
{
    if (ok)
    {
        Log(Color::Green, "Silent inject  ok");
        Ptr(Color::Magenta, "Handler", handler);
        Ptr(Color::Yellow, "Stub", stub);
        Ptr(Color::Cyan, "State", state);
        Ptr(Color::Blue, "Slot", slot);
        Log(Color::Gray, "Raycast slot resolved dynamically (function-descriptor scan)");
    }

    else
    {
        Log(Color::Red, "Silent inject  fail%s%s",
            detail && detail[0] ? " " : "",
            detail ? detail : "");
        if (handler)
            Ptr(Color::Magenta, "Handler", handler);
        if (stub)
            Ptr(Color::Yellow, "Stub", stub);
        if (slot)
            Ptr(Color::Blue, "Slot", slot);
    }
}

void DumpGate(bool ok, const char* method, std::uint64_t slot,
              std::uint64_t handler, std::uint64_t stub,
              std::uint64_t state, bool cave, int fail)
{
    if (!ok)
    {
        Log(Color::Red, "Gate install   fail (%d)", fail);
        return;
    }

    Log(Color::Green, "Gate install   ok");
    Log(Color::White, "Method         %s", method ? method : "?");
    Ptr(Color::Blue, "Slot", slot);
    Ptr(Color::Magenta, "Handler", handler);
    Ptr(Color::Yellow, "Stub", stub);
    Ptr(Color::Cyan, "State", state);
    Log(Color::Gray, "Stub place     %s", cave ? "cave" : "rwx");
}

// счётчик не растёт = слот холодный, надо брать другой метод
void GateTimeout(const char* method, std::uint64_t calls)
{
    Log(Color::Red, "Gate timeout   %s, hits %llu",
        method ? method : "?", (unsigned long long)calls);
}

}

