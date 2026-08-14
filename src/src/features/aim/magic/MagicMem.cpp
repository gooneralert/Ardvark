#include "pch.h"
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "MagicMem.h"
#include "MagicState.h"
#include "core/memory/Memory.h"
#include <Windows.h>

#ifndef CFG_CALL_TARGET_VALID
#define CFG_CALL_TARGET_VALID 0x00000001
#endif

namespace Cheat {
namespace Features {
namespace MagicBullet {
namespace mb {

	bool addr_ok(std::uintptr_t a)
	{
	    return a >= 0x10000ull && a < 0x00007FFFFFFFFFFFull;
	}
	bool w_mem(std::uintptr_t a, const void* d, std::size_t s)
	{
	    if (!addr_ok(a) || !d || !s || !g_Memory.GetHandle())
	    {
	        return false;
	    }
	    return g_Memory.WriteRaw(a, d, s) == s;
	}
	std::size_t page_sz()
	{
	    static std::size_t p = 0;
	    if (!p)
	    {
	        SYSTEM_INFO i{};
	        GetSystemInfo(&i);
	        p = (std::size_t)i.dwPageSize;
	        if (!p) p = 0x1000u;
	    }
	    return p;
	}
	DWORD query_protect(std::uintptr_t a)
	{
	    MEMORY_BASIC_INFORMATION mbi{};
	    if (!VirtualQueryEx(g_Memory.GetHandle(),
	            reinterpret_cast<void*>(a), &mbi, sizeof(mbi)))
	    {
	        return 0;
	    }
	    return mbi.Protect;
	}
	bool is_executable_protect(DWORD p)
	{
	    DWORD x = p & 0xFF;
	    if (x == PAGE_EXECUTE)
	    {
	        return true;
	    }

	    if (x == PAGE_EXECUTE_READ)
	    {
	        return true;
	    }

	    if (x == PAGE_EXECUTE_READWRITE)
	    {
	        return true;
	    }

	    if (x == PAGE_EXECUTE_WRITECOPY)
	    {
	        return true;
	    }

	    return false;
	}
	bool protect_remote(std::uintptr_t address, std::size_t size, DWORD protection,
	                    DWORD* old_protect)
	{
	    if (!addr_ok(address) || !size || !g_Memory.GetHandle())
	    {
	        return false;
	    }

	    std::uintptr_t page_mask = ~((std::uintptr_t)page_sz() - 1);
	    std::uintptr_t base = address & page_mask;
	    std::uintptr_t end =
	        (address + size + page_sz() - 1) & page_mask;
	    std::size_t span = (std::size_t)(end - base);

	    using NtProtectFn = LONG(WINAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
	    static NtProtectFn nt_protect = nullptr;
	    if (!nt_protect)
	    {
	        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	        if (ntdll)
	        {
	            nt_protect = (NtProtectFn)GetProcAddress(ntdll, "NtProtectVirtualMemory");
	        }
	    }

	    auto try_one = [&](DWORD req) -> bool
	    {
	        DWORD old = 0;
	        if (VirtualProtectEx(g_Memory.GetHandle(),
	                (void*)base, span, req, &old))
	        {
	            if (old_protect)
	            {
	                *old_protect = old;
	            }
	            return true;
	        }

	        if (!nt_protect)
	        {
	            return false;
	        }

	        PVOID  nt_base = (void*)base;
	        SIZE_T nt_size = span;
	        ULONG  nt_old  = 0;
	        LONG st = nt_protect(
	            g_Memory.GetHandle(), &nt_base, &nt_size, req, &nt_old);
	        if (st >= 0)
	        {
	            if (old_protect)
	            {
	                *old_protect = (DWORD)nt_old;
	            }
	            return true;
	        }

	        return false;
	    };

	    if (try_one(protection))
	    {
	        return true;
	    }

	    if (protection == PAGE_EXECUTE_READWRITE &&
	        try_one(PAGE_EXECUTE_WRITECOPY))
	    {
	        return true;
	    }

	    if (protection == PAGE_READWRITE && try_one(PAGE_WRITECOPY))
	    {
	        return true;
	    }

	    return false;
	}
	bool write_protected(std::uintptr_t address, const void* data, std::size_t size)
	{
	    if (!addr_ok(address) || !data || !size)
	    {
	        return false;
	    }

	    DWORD old = 0;
	    bool changed =
	        protect_remote(address, size, PAGE_EXECUTE_READWRITE, &old);
	    bool wrote = w_mem(address, data, size);
	    if (changed)
	    {
	        protect_remote(address, size, old, nullptr);
	    }
	    return wrote;
	}
	// иначе CFG орёт на наш stub
	bool mark_cfg(std::uintptr_t t)
	{
	    auto resolve = []() -> FARPROC
	    {
	        const char* mods[] = {
	            "kernelbase.dll", "kernel32.dll",
	            "api-ms-win-core-memory-l1-1-3.dll"
	        };
	        for (auto* m : mods)
	        {
	            HMODULE h = GetModuleHandleA(m);
	            if (!h)
	            {
	                h = LoadLibraryA(m);
	            }

	            if (!h)
	            {
	                continue;
	            }

	            FARPROC p = GetProcAddress(h, "SetProcessValidCallTargets");
	            if (p)
	            {
	                return p;
	            }
	        }
	        return nullptr;
	    };

	    FARPROC proc = resolve();
	    if (!proc)
	    {
	        return false;
	    }

	    struct Info {
	        ULONG_PTR Offset;
	        ULONG     Flags;
	    } info{};
	    info.Offset = t & (page_sz() - 1);
	    info.Flags  = CFG_CALL_TARGET_VALID;

	    using Fn = BOOL(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, void*);
	    SetLastError(0);
	    BOOL ok = ((Fn)proc)(
	        g_Memory.GetHandle(),
	        (void*)(t & ~((std::uintptr_t)page_sz() - 1)),
	        page_sz(), 1, &info);
	    return ok != 0;
	}

} // namespace mb
} // namespace MagicBullet
} // namespace Features
} // namespace Cheat
