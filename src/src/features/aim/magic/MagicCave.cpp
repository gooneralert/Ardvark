#include "pch.h"
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "MagicCave.h"
#include "MagicMem.h"
#include "MagicState.h"
#include "core/memory/Memory.h"
#include <Windows.h>
#include <vector>

namespace Cheat {
namespace Features {
namespace MagicBullet {
namespace mb {

	bool region_is_padding(std::uintptr_t a, std::size_t n)
	{
	    std::vector<std::uint8_t> buf(n);
	    if (g_Memory.ReadRaw(a, buf.data(), n) != n)
	    {
	        return false;
	    }

	    for (auto b : buf)
	    {
	        if (b != 0xCC && b != 0x00 && b != 0x90)
	        {
	            return false;
	        }
	    }
	    return true;
	}
	bool read_val(std::uintptr_t a, void* d, std::size_t s) {
	    return g_Memory.ReadRaw(a, d, s) == s;
	}
	std::uintptr_t find_cave_in_module(std::uintptr_t module_base, std::size_t need,
	                                  std::uintptr_t min_offset,
	                                  std::uintptr_t ignore)
	{
	    if (!addr_ok(module_base) || !need)
	    {
	        return 0;
	    }

	    IMAGE_DOS_HEADER dos{};
	    if (!read_val(module_base, &dos, sizeof(dos)) ||
	        dos.e_magic != IMAGE_DOS_SIGNATURE)
	    {
	        return 0;
	    }

	    IMAGE_NT_HEADERS64 nt{};
	    const std::uintptr_t nt_addr =
	        module_base + static_cast<std::uintptr_t>(dos.e_lfanew);
	    if (!read_val(nt_addr, &nt, sizeof(nt)) ||
	        nt.Signature != IMAGE_NT_SIGNATURE)
	    {
	        return 0;
	    }

	    const std::uintptr_t section_base =
	        nt_addr + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
	        nt.FileHeader.SizeOfOptionalHeader;

	    for (WORD i = 0; i < nt.FileHeader.NumberOfSections; ++i)
	    {
	        IMAGE_SECTION_HEADER section{};
	        if (!read_val(section_base +
	                          static_cast<std::uintptr_t>(i) * sizeof(section),
	                      &section, sizeof(section)))
	        {
	            break;
	        }

	        if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
	        {
	            continue;
	        }

	        const std::uintptr_t section_off = section.VirtualAddress;
	        std::size_t section_size = section.Misc.VirtualSize
	            ? section.Misc.VirtualSize
	            : section.SizeOfRawData;
	        if (!section_off || section_size < need)
	        {
	            continue;
	        }

	        std::uintptr_t scan_off = section_off;
	        if (scan_off < min_offset)
	        {
	            scan_off = min_offset;
	        }

	        if (scan_off >= section_off + section_size)
	        {
	            continue;
	        }

	        const std::uintptr_t scan_start = module_base + scan_off;
	        const std::size_t scan_size = static_cast<std::size_t>(
	            section_off + section_size - scan_off);

	        std::vector<std::uint8_t> buf(scan_size);
	        if (!read_val(scan_start, buf.data(), buf.size()))
	        {
	            continue;
	        }

	        std::size_t run_start = 0;
	        std::size_t run_len = 0;
	        for (std::size_t j = 0; j < buf.size(); ++j)
	        {
	            const std::uint8_t b = buf[j];
	            if (b != 0x00 && b != 0xCC && b != 0x90)
	            {
	                run_len = 0;
	                run_start = j + 1;
	                continue;
	            }
	            ++run_len;
	            if (run_len < need)
	            {
	                continue;
	            }

	            std::uintptr_t cand = scan_start + run_start;
	            const std::uintptr_t aligned =
	                (cand + 0x0F) & ~static_cast<std::uintptr_t>(0x0F);
	            const std::size_t loss =
	                static_cast<std::size_t>(aligned - cand);
	            if (run_len < need + loss)
	            {
	                continue;
	            }

	            if (ignore && aligned == ignore)
	            {
	                continue;
	            }

	            if (g_hook.thunk && aligned == g_hook.thunk)
	            {
	                continue;
	            }

	            return aligned;
	        }
	    }
	    return 0;
	}
	// ищем пещеру в чужих dll, потом по процессу
	std::uintptr_t find_exec_cave(std::size_t need, std::uintptr_t,
	                             std::uintptr_t ignore)
	{
	    static const wchar_t* pref[] = {
	        L"winsta.dll",
	        L"win32u.dll",
	        L"uxtheme.dll",
	        L"dwmapi.dll",
	        L"msctf.dll",
	        L"TextInputFramework.dll",
	        L"CoreMessaging.dll",
	        L"user32.dll",
	    };

	    for (std::size_t mi = 0; mi < sizeof(pref) / sizeof(pref[0]); ++mi)
	    {
	        const wchar_t* name = pref[mi];
	        const std::uintptr_t mod = g_Memory.GetModuleBase(name);
	        if (!mod)
	        {
	            continue;
	        }

	        const std::uintptr_t min_off = (mi == 0) ? 0x2000u : 0x1000u;
	        const std::uintptr_t cave =
	            find_cave_in_module(mod, need, min_off, ignore);
	        if (!cave)
	        {
	            continue;
	        }

	        (void)name;
	        return cave;
	    }

	    MEMORY_BASIC_INFORMATION mbi{};
	    std::uintptr_t addr = 0;
	    std::uintptr_t fallback_rwx = 0;

	    while (VirtualQueryEx(g_Memory.GetHandle(),
	        reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)))
	    {
	        const auto base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
	        const auto size = static_cast<std::size_t>(mbi.RegionSize);
	        addr = base + size;
	        if (addr < base)
	        {
	            break;
	        }

	        if (mbi.State != MEM_COMMIT)
	        {
	            continue;
	        }

	        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
	        {
	            continue;
	        }

	        if (!is_executable_protect(mbi.Protect))
	        {
	            continue;
	        }

	        if (size < need)
	        {
	            continue;
	        }

	        if (g_hook.thunk && base <= g_hook.thunk && g_hook.thunk < addr)
	        {
	            continue;
	        }

	        for (std::size_t off = 0; off + need <= size; off += 0x10)
	        {
	            const std::uintptr_t cand = base + off;
	            if (ignore && cand == ignore)
	            {
	                continue;
	            }

	            if (region_is_padding(cand, need))
	            {
	                return cand;
	            }
	        }

	        if (!fallback_rwx && mbi.Type == MEM_PRIVATE &&
	            (mbi.Protect & 0xFF) == PAGE_EXECUTE_READWRITE &&
	            size >= need + 0x40)
	        {
	            const std::uintptr_t cand = base + size - need;
	            if (!ignore || cand != ignore)
	            {
	                fallback_rwx = cand;
	            }
	        }
	    }

	    return fallback_rwx;
	}
	std::uintptr_t alloc_exec_page() {
	    const std::uintptr_t p = g_Memory.Alloc(page_sz(), PAGE_EXECUTE_READWRITE);
	    if (!p) return 0;

	    const DWORD prot = query_protect(p);
	    if (!is_executable_protect(prot)) {
	        g_Memory.Free(p);
	        return 0;
	    }
	    return p;
	}

} // namespace mb
} // namespace MagicBullet
} // namespace Features
} // namespace Cheat
