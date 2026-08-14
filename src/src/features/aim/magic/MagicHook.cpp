#include "pch.h"
#include "features/lua/vm/Reflect.h"
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "../MagicBullet.h"
#include "MagicState.h"
#include "MagicMem.h"
#include "MagicThunk.h"
#include "MagicCave.h"
#include "core/memory/Memory.h"
#include "core/console/Console.h"
#include <Windows.h>
#include <vector>
#include <chrono>
#include <cstddef>

namespace Cheat {
namespace Features {
namespace MagicBullet {
namespace mb {

	void report_inject(bool ok, std::uintptr_t orig = 0, std::uintptr_t stub = 0,
	                   const char* detail = nullptr) {
	    const std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
	    const std::uintptr_t slot = base ? (Reflect::RaycastSlot()) : 0;
	    Console::Clear();
	    Console::DumpWorld();
	    Console::DumpSilent(ok, orig, stub, g_hook.state, slot, detail);
	}

} // namespace mb

	bool Ready() { return mb::g_hook.installed; }
	bool Aiming() { return mb::g_hook.active; }
	bool Install()
	{
	    if (mb::g_hook.installed)
	    {
	        return true;
	    }

	    std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
	    if (!base)
	    {
	        return false;
	    }

	    // не долбим install каждые 2 мс если упало
	    auto now = std::chrono::steady_clock::now();
	    if (mb::g_lastFail.time_since_epoch().count() != 0 &&
	        now - mb::g_lastFail < std::chrono::milliseconds(1500))
	    {
	        return false;
	    }

	    std::uintptr_t slot = Reflect::RaycastSlot();
	    std::uintptr_t fn   = g_Memory.Read<std::uintptr_t>(slot);

	    if (!mb::addr_ok(fn))
	    {
	        mb::g_lastFail = now;
	        mb::report_inject(false, 0, 0, "bad handler");
	        return false;
	    }

	    if (!mb::g_hook.state)
	    {
	        mb::g_hook.state = g_Memory.Alloc(mb::page_sz(), PAGE_READWRITE);
	    }

	    if (!mb::g_hook.state)
	    {
	        mb::g_lastFail = now;
	        mb::report_inject(false, 0, 0, "state alloc");
	        return false;
	    }

	    // jmp-only оставлял раньше, щас полный stub
	    //auto thunk = mb::make_jmp_thunk(fn);
	    auto thunk = mb::make_hook_thunk(mb::g_hook.state, fn);

	    if (thunk.size() > 0x200)
	    {
	        mb::g_lastFail = now;
	        mb::report_inject(false, 0, 0, "stub too large");
	        return false;
	    }

	    bool owned = false;
	    std::uintptr_t stub = 0;
	    std::uintptr_t ignore_cave = 0;

	    // несколько попыток, иногда cave не пишется
	    for (int attempt = 0; attempt < 8 && !stub; ++attempt)
	    {
	        std::uintptr_t cand = mb::find_exec_cave(0x200, base, ignore_cave);
	        if (!cand)
	        {
	            break;
	        }

	        SetLastError(0);
	        DWORD old_prot = 0;
	        bool prot_ok =
	            mb::protect_remote(cand, thunk.size(), PAGE_EXECUTE_READWRITE, &old_prot);

	        SetLastError(0);
	        if (!mb::write_protected(cand, thunk.data(), thunk.size()))
	        {
	            if (prot_ok)
	            {
	                mb::protect_remote(cand, thunk.size(), old_prot, nullptr);
	            }
	            ignore_cave = cand;
	            continue;
	        }

	        stub = cand;
	        owned = false;
	    }

	    if (!stub)
	    {
	        stub = mb::alloc_exec_page();
	        owned = stub != 0;
	        if (stub)
	        {
	            SetLastError(0);
	            if (!mb::write_protected(stub, thunk.data(), thunk.size()))
	            {
	                g_Memory.Free(stub);
	                stub = 0;
	                owned = false;
	            }
	        }
	    }

	    if (!stub)
	    {
	        mb::g_lastFail = now;
	        mb::report_inject(false, 0, 0, "no host");
	        return false;
	    }

	    mb::RaycastState empty{};
	    SetLastError(0);
	    if (!mb::w_mem(mb::g_hook.state, &empty, sizeof(empty)))
	    {
	        mb::g_lastFail = now;
	        if (owned)
	        {
	            g_Memory.Free(stub);
	        }
	        mb::report_inject(false, 0, 0, "state write");
	        return false;
	    }

	    FlushInstructionCache(g_Memory.GetHandle(), (void*)stub, thunk.size());
	    mb::mark_cfg(stub);

	    DWORD prot = mb::query_protect(stub);
	    if (!mb::is_executable_protect(prot))
	    {
	        mb::g_lastFail = now;
	        if (owned)
	        {
	            g_Memory.Free(stub);
	        }
	        mb::report_inject(false, 0, 0, "stub not executable");
	        return false;
	    }

	    mb::protect_remote(slot, 8, PAGE_READWRITE, nullptr);
	    if (!mb::write_protected(slot, &stub, sizeof(stub)) ||
	        g_Memory.Read<std::uintptr_t>(slot) != stub)
	    {
	        mb::g_lastFail = now;
	        if (owned)
	        {
	            g_Memory.Free(stub);
	        }
	        mb::report_inject(false, 0, 0, "slot write");
	        return false;
	    }

	    mb::g_hook.module_base      = base;
	    mb::g_hook.originalFunction = fn;
	    mb::g_hook.thunk            = stub;
	    mb::g_hook.thunk_owned      = owned;
	    mb::g_hook.installed        = true;
	    mb::g_hook.active           = false;
	    mb::report_inject(true, fn, stub);
	    return true;
	}

	void Remove()
	{
	    if (mb::g_hook.installed && mb::addr_ok(mb::g_hook.originalFunction) && mb::g_hook.module_base)
	    {
	        std::uintptr_t slot =
	            Reflect::RaycastSlot();
	        mb::write_protected(slot, &mb::g_hook.originalFunction,
	                        sizeof(mb::g_hook.originalFunction));
	    }

	    if (mb::g_hook.thunk && !mb::g_hook.thunk_owned)
	    {
	        std::vector<std::uint8_t> pad(0x200, 0xCC);
	        mb::write_protected(mb::g_hook.thunk, pad.data(), pad.size());
	    }

	    if (mb::g_hook.thunk && mb::g_hook.thunk_owned)
	    {
	        g_Memory.Free(mb::g_hook.thunk);
	    }

	    if (mb::g_hook.state)
	    {
	        g_Memory.Free(mb::g_hook.state);
	    }

	    mb::g_hook = {};
	    mb::g_wallbang = false;
	}

	void Ensure(bool want)
	{
	    std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
	    static std::uintptr_t s_last_base = 0;

	    if (mb::g_hook.installed && base && s_last_base && base != s_last_base)
	    {
	        Remove();
	        Console::Clear();
	        Console::DumpWorld();
	        Console::Log(Console::Color::Orange, "Magic rescan  module");
	    }

	    if (base)
	    {
	        s_last_base = base;
	    }

	    if (want)
	    {
	        if (!base)
	        {
	            return;
	        }

	        // слот могли перетереть, переставить без Remove каждый кадр
	        if (mb::g_hook.installed && mb::g_hook.thunk)
	        {
	            std::uintptr_t slot = Reflect::RaycastSlot();
	            std::uintptr_t cur = g_Memory.Read<std::uintptr_t>(slot);
	            if (cur != mb::g_hook.thunk)
	            {
	                mb::g_hook.installed = false;
	            }
	        }

	        if (!mb::g_hook.installed)
	        {
	            Install();
	        }
	    }

	    else if (mb::g_hook.installed)
	    {
	        Remove();
	        Console::Clear();
	        Console::DumpWorld();
	        Console::Log(Console::Color::Yellow, "Magic removed");
	    }
	}

	void SetActive(bool on, const Vector3& world_target)
	{
	    if (!on)
	    {
	        if (mb::g_hook.active && mb::g_hook.state)
	        {
	            std::uint32_t v = 0;
	            mb::w_mem(mb::g_hook.state, &v, sizeof(v));
	            mb::g_hook.active = false;
	        }
	        mb::g_wallbang = false;
	        return;
	    }

	    if (!mb::g_hook.installed)
	    {
	        return;
	    }

	    float pos[3]{ world_target.x, world_target.y, world_target.z };
	    // magic = всегда wallbang; scale мелкий, origin у самой цели
	    std::uint32_t flags = 1u;
	    float scale = 1.15f;
	    std::uint32_t one = 1;
	    mb::w_mem(mb::g_hook.state + offsetof(mb::RaycastState, reserved), &flags, sizeof(flags));
	    mb::w_mem(mb::g_hook.state + offsetof(mb::RaycastState, target_x), pos, sizeof(pos));
	    mb::w_mem(mb::g_hook.state + offsetof(mb::RaycastState, scale), &scale, sizeof(scale));
	    mb::w_mem(mb::g_hook.state + offsetof(mb::RaycastState, active), &one, sizeof(one));
	    mb::g_hook.active = true;
	    mb::g_wallbang = true;
	}

} // namespace MagicBullet
} // namespace Features
} // namespace Cheat
