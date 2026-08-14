#include "pch.h"
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "RaycastSilent.h"
#include "core/memory/Memory.h"
#include "features/lua/vm/Reflect.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/globals/Globals.h"
#include "core/roblox/math/Math.h"
#include "core/roblox/classes/Classes.h"
#include "core/console/Console.h"

#include <Windows.h>
#include <vector>
#include <cstring>
#include <chrono>
#include <cstddef>

#ifndef CFG_CALL_TARGET_VALID
#define CFG_CALL_TARGET_VALID 0x00000001
#endif

namespace Cheat {
namespace Features {
namespace RaycastSilent {
namespace {

// Raycast function slot is resolved dynamically via the WorldRoot
// function-descriptor scan (guide "find_first_func") — no hard-coded RVA.

#pragma pack(push, 4)
struct RaycastState {
	std::uint32_t active = 0;
	std::uint32_t reserved = 0;
	float target_x = 0.f;
	float target_y = 0.f;
	float target_z = 0.f;
	float scale = 1.15f;
	std::uint64_t calls = 0;
	float cam_x = 0.f;
	float cam_y = 0.f;
	float cam_z = 0.f;
};
#pragma pack(pop)

static_assert(offsetof(RaycastState, active) == 0x00);
static_assert(offsetof(RaycastState, target_x) == 0x08);
static_assert(offsetof(RaycastState, calls) == 0x18);
static_assert(offsetof(RaycastState, cam_x) == 0x20);

struct Hook {
	std::uintptr_t thunk = 0;
	std::uintptr_t state = 0;
	std::uintptr_t original = 0;
	std::uintptr_t module_base = 0;
	bool installed = false;
	bool active = false;
	bool wallbang = false;
};

Hook g_hook{};
auto g_last_fail = std::chrono::steady_clock::time_point{};
std::uintptr_t g_last_base = 0;
auto g_last_slot_check = std::chrono::steady_clock::time_point{};

bool addr_ok(std::uintptr_t a)
{
	return a >= 0x10000ull && a < 0x00007FFFFFFFFFFFull;
}

bool w_mem(std::uintptr_t a, const void* d, std::size_t s)
{
	if (!addr_ok(a) || !d || !s || !g_Memory.GetHandle())
		return false;
	return g_Memory.WriteRaw(a, d, s) == s;
}

bool read_val(std::uintptr_t a, void* d, std::size_t s)
{
	return g_Memory.ReadRaw(a, d, s) == s;
}

std::size_t page_sz()
{
	static std::size_t p = 0;
	if (!p)
	{
		SYSTEM_INFO i{};
		GetSystemInfo(&i);
		p = i.dwPageSize ? (std::size_t)i.dwPageSize : 0x1000u;
	}
	return p;
}

bool is_exec_prot(DWORD p)
{
	const DWORD x = p & 0xFF;
	return x == PAGE_EXECUTE || x == PAGE_EXECUTE_READ ||
		x == PAGE_EXECUTE_READWRITE || x == PAGE_EXECUTE_WRITECOPY;
}

bool region_exec(std::uintptr_t a)
{
	MEMORY_BASIC_INFORMATION mbi{};
	if (!VirtualQueryEx(g_Memory.GetHandle(), (void*)a, &mbi, sizeof(mbi)))
		return false;
	return mbi.State == MEM_COMMIT && is_exec_prot(mbi.Protect);
}

bool protect_remote(std::uintptr_t address, std::size_t size, DWORD protection, DWORD* old_protect = nullptr)
{
	if (!addr_ok(address) || !size || !g_Memory.GetHandle())
		return false;
	const std::uintptr_t mask = ~((std::uintptr_t)page_sz() - 1);
	const std::uintptr_t base = address & mask;
	const std::uintptr_t end = (address + size + page_sz() - 1) & mask;
	DWORD old = 0;
	if (!VirtualProtectEx(g_Memory.GetHandle(), (void*)base, (SIZE_T)(end - base), protection, &old))
		return false;
	if (old_protect)
		*old_protect = old;
	return true;
}

bool write_protected(std::uintptr_t address, const void* data, std::size_t size)
{
	if (!addr_ok(address) || !data || !size)
		return false;
	DWORD old = 0;
	const bool changed = protect_remote(address, size, PAGE_EXECUTE_READWRITE, &old);
	const bool wrote = w_mem(address, data, size);
	if (changed)
		protect_remote(address, size, old, nullptr);
	return wrote;
}

bool mark_cfg(std::uintptr_t t)
{
	static FARPROC proc = nullptr;
	if (!proc)
	{
		const char* mods[] = {
			"kernelbase.dll", "kernel32.dll",
			"api-ms-win-core-memory-l1-1-3.dll"
		};
		for (auto* m : mods)
		{
			HMODULE h = GetModuleHandleA(m);
			if (!h)
				h = LoadLibraryA(m);
			if (!h)
				continue;
			proc = GetProcAddress(h, "SetProcessValidCallTargets");
			if (proc)
				break;
		}
	}
	if (!proc || !g_Memory.GetHandle())
		return false;

	struct Info {
		ULONG_PTR Offset;
		ULONG Flags;
	} info{};
	info.Offset = t & (page_sz() - 1);
	info.Flags = CFG_CALL_TARGET_VALID;

	using Fn = BOOL(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, void*);
	return ((Fn)proc)(
		g_Memory.GetHandle(),
		(void*)(t & ~((std::uintptr_t)page_sz() - 1)),
		page_sz(), 1, &info) != 0;
}

void append_u64(std::vector<std::uint8_t>& c, std::uint64_t v)
{
	const auto* b = (const std::uint8_t*)&v;
	c.insert(c.end(), b, b + 8);
}

void patch_rel32(std::vector<std::uint8_t>& c, std::size_t o, std::size_t t)
{
	const std::int32_t v = (std::int32_t)((std::ptrdiff_t)t - (std::ptrdiff_t)(o + 4));
	std::memcpy(c.data() + o, &v, 4);
}

std::vector<std::uint8_t> make_hook_thunk(std::uintptr_t state, std::uintptr_t orig)
{
	std::vector<std::uint8_t> c;
	c.reserve(384);
	std::vector<std::size_t> inactive;

	auto je_inactive = [&]
	{
		c.insert(c.end(), { 0x0F, 0x84 });
		inactive.push_back(c.size());
		c.insert(c.end(), { 0, 0, 0, 0 });
	};
	auto jbe_inactive = [&]
	{
		c.insert(c.end(), { 0x0F, 0x86 });
		inactive.push_back(c.size());
		c.insert(c.end(), { 0, 0, 0, 0 });
	};

	c.insert(c.end(), { 0x48, 0x83, 0xEC, 0x68 });
	c.insert(c.end(), { 0x49, 0xBA });
	append_u64(c, state);
	c.insert(c.end(), { 0x41, 0x83, 0x3A, 0x00 });
	je_inactive();
	c.insert(c.end(), { 0x4D, 0x85, 0xC0 }); je_inactive();
	c.insert(c.end(), { 0x4D, 0x85, 0xC9 }); je_inactive();

	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x42, 0x08 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x00 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x40 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x4A, 0x0C });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x48, 0x04 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x4C, 0x24, 0x44 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x52, 0x10 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x50, 0x08 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x54, 0x24, 0x48 });

	c.insert(c.end(), { 0x0F, 0x28, 0xD8 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xDB });
	c.insert(c.end(), { 0x0F, 0x28, 0xE1 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xDC });
	c.insert(c.end(), { 0x0F, 0x28, 0xE2 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xDC });
	c.insert(c.end(), { 0xF3, 0x0F, 0x51, 0xDB });
	c.insert(c.end(), { 0x0F, 0x57, 0xED });
	c.insert(c.end(), { 0x0F, 0x2E, 0xDD });
	jbe_inactive();

	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x21 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x69, 0x04 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xED });
	c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x69, 0x08 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xED });
	c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x51, 0xE4 });
	c.insert(c.end(), { 0x0F, 0x57, 0xED });
	c.insert(c.end(), { 0x0F, 0x2E, 0xE5 });
	jbe_inactive();

	c.insert(c.end(), { 0x41, 0x8B, 0x42, 0x04 });
	c.insert(c.end(), { 0xA8, 0x01 });
	c.insert(c.end(), { 0x0F, 0x85 });
	const std::size_t wallbang_jmp = c.size();
	c.insert(c.end(), { 0, 0, 0, 0 });

	const std::size_t dir_only_off = c.size();
	c.insert(c.end(), { 0x0F, 0x28, 0xEC });
	c.insert(c.end(), { 0xF3, 0x0F, 0x5E, 0xEB });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xC5 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xCD });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xD5 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x11, 0x01 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x11, 0x49, 0x04 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x11, 0x51, 0x08 });
	c.insert(c.end(), { 0x49, 0xFF, 0x42, 0x18 });
	c.push_back(0xE9);
	const std::size_t to_call = c.size();
	c.insert(c.end(), { 0, 0, 0, 0 });

	const std::size_t wallbang_off = c.size();
	patch_rel32(c, wallbang_jmp, wallbang_off);

	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x20 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x62, 0x20 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x68, 0x04 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x6A, 0x24 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xED });
	c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x68, 0x08 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x6A, 0x28 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xED });
	c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
	c.insert(c.end(), { 0xB8, 0x00, 0x00, 0x10, 0x41 });
	c.insert(c.end(), { 0x66, 0x0F, 0x6E, 0xE8 });
	c.insert(c.end(), { 0x0F, 0x2F, 0xE5 });
	c.insert(c.end(), { 0x0F, 0x82 });
	const std::size_t cam_jb = c.size();
	c.insert(c.end(), { 0, 0, 0, 0 });
	patch_rel32(c, cam_jb, dir_only_off);

	c.insert(c.end(), { 0xF3, 0x0F, 0x5E, 0xC3 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x5E, 0xCB });
	c.insert(c.end(), { 0xF3, 0x0F, 0x5E, 0xD3 });

	c.insert(c.end(), { 0x0F, 0x28, 0xE0 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, 0x08 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
	c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x6C, 0x24, 0x50 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x64, 0x24, 0x40 });

	c.insert(c.end(), { 0x0F, 0x28, 0xE1 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, 0x0C });
	c.insert(c.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
	c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x6C, 0x24, 0x54 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x64, 0x24, 0x44 });

	c.insert(c.end(), { 0x0F, 0x28, 0xE2 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
	c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, 0x10 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
	c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x6C, 0x24, 0x58 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
	c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x64, 0x24, 0x48 });

	c.insert(c.end(), { 0x4C, 0x8D, 0x44, 0x24, 0x50 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x4C, 0x24, 0x40 });
	c.insert(c.end(), { 0x49, 0xFF, 0x42, 0x18 });

	const std::size_t call_off = c.size();
	patch_rel32(c, to_call, call_off);
	const std::size_t inactive_off = c.size();
	for (auto o : inactive)
		patch_rel32(c, o, inactive_off);

	c.insert(c.end(), { 0x48, 0x8B, 0x84, 0x24, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x48, 0x89, 0x44, 0x24, 0x20 });
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, orig);
	c.insert(c.end(), { 0xFF, 0xD0 });
	c.insert(c.end(), { 0x48, 0x83, 0xC4, 0x68 });
	c.push_back(0xC3);
	return c;
}

std::uintptr_t find_cave_in_module(std::uintptr_t mod, std::size_t need, std::uintptr_t min_off, std::uintptr_t ignore)
{
	IMAGE_DOS_HEADER dos{};
	if (!read_val(mod, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE)
		return 0;

	IMAGE_NT_HEADERS64 nt{};
	const std::uintptr_t nt_addr = mod + (std::uintptr_t)dos.e_lfanew;
	if (!read_val(nt_addr, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE)
		return 0;

	const std::uintptr_t sec_base = nt_addr + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
		nt.FileHeader.SizeOfOptionalHeader;

	for (WORD i = 0; i < nt.FileHeader.NumberOfSections; ++i)
	{
		IMAGE_SECTION_HEADER sec{};
		if (!read_val(sec_base + (std::uintptr_t)i * sizeof(sec), &sec, sizeof(sec)))
			break;
		if ((sec.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
			continue;

		const std::uintptr_t off = sec.VirtualAddress;
		const std::size_t size = sec.Misc.VirtualSize ? sec.Misc.VirtualSize : sec.SizeOfRawData;
		if (!off || size < need)
			continue;

		std::uintptr_t scan_off = off < min_off ? min_off : off;
		if (scan_off >= off + size)
			continue;

		const std::uintptr_t start = mod + scan_off;
		const std::size_t scan = (std::size_t)(off + size - scan_off);
		std::vector<std::uint8_t> buf(scan);
		if (!read_val(start, buf.data(), buf.size()))
			continue;

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
				continue;

			const std::uintptr_t cand = start + run_start;
			const std::uintptr_t aligned = (cand + 0x0F) & ~std::uintptr_t(0x0F);
			const std::size_t loss = (std::size_t)(aligned - cand);
			if (run_len < need + loss)
				continue;
			if (ignore && aligned == ignore)
				continue;
			if (g_hook.thunk && aligned == g_hook.thunk)
				continue;
			return aligned;
		}
	}
	return 0;
}

std::uintptr_t find_dll_cave(std::size_t need, std::uintptr_t ignore)
{
	static const wchar_t* pref[] = {
		L"winsta.dll", L"win32u.dll", L"user32.dll",
	};

	for (std::size_t i = 0; i < sizeof(pref) / sizeof(pref[0]); ++i)
	{
		const std::uintptr_t mod = g_Memory.GetModuleBase(pref[i]);
		if (!mod)
			continue;
		const std::uintptr_t cave = find_cave_in_module(mod, need, i == 0 ? 0x2000u : 0x1000u, ignore);
		if (cave)
			return cave;
	}
	return 0;
}

std::uintptr_t module_base()
{
	return g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
}

std::uintptr_t slot_addr(std::uintptr_t /*base*/)
{
	// Resolved dynamically via the WorldRoot function-descriptor scan.
	return Reflect::RaycastSlot();
}

void deactivate_state()
{
	if (!g_hook.state || !g_Memory.GetHandle())
		return;
	std::uint32_t z = 0;
	w_mem(g_hook.state, &z, sizeof(z));
	g_hook.active = false;
	g_hook.wallbang = false;
}

void free_state()
{
	if (g_hook.state)
	{
		g_Memory.Free(g_hook.state);
		g_hook.state = 0;
	}
}

bool restore_slot()
{
	if (!g_hook.module_base || !addr_ok(g_hook.original) || !g_hook.thunk)
		return false;
	if (!g_Memory.GetHandle() || !g_Memory.IsAlive())
		return false;

	const std::uintptr_t live = module_base();
	if (!live || live != g_hook.module_base)
		return false;

	const std::uintptr_t slot = slot_addr(g_hook.module_base);
	if (g_Memory.Read<std::uintptr_t>(slot) != g_hook.thunk)
		return false;

	return write_protected(slot, &g_hook.original, sizeof(g_hook.original));
}

bool install()
{
	if (g_hook.installed)
		return true;

	const std::uintptr_t base = module_base();
	if (!base || !g_Memory.GetHandle() || !g_Memory.IsAlive())
		return false;

	const auto now = std::chrono::steady_clock::now();
	if (g_last_fail.time_since_epoch().count() != 0 &&
		now - g_last_fail < std::chrono::milliseconds(1500))
		return false;

	const std::uintptr_t slot = slot_addr(base);
	const std::uintptr_t fn = g_Memory.Read<std::uintptr_t>(slot);
	if (!addr_ok(fn) || !region_exec(fn))
	{
		g_last_fail = now;
		return false;
	}

	if (g_hook.thunk && g_hook.state && g_hook.original == fn)
	{
		mark_cfg(g_hook.thunk);
		if (write_protected(slot, &g_hook.thunk, sizeof(g_hook.thunk)) &&
			g_Memory.Read<std::uintptr_t>(slot) == g_hook.thunk)
		{
			g_hook.module_base = base;
			g_hook.installed = true;
			deactivate_state();
			return true;
		}
	}

	if (!g_hook.state)
		g_hook.state = g_Memory.Alloc(page_sz(), PAGE_READWRITE);
	if (!g_hook.state)
	{
		g_last_fail = now;
		return false;
	}

	auto thunk = make_hook_thunk(g_hook.state, fn);
	if (thunk.size() > 0x200)
	{
		g_last_fail = now;
		return false;
	}

	std::uintptr_t stub = 0;
	std::uintptr_t ignore = 0;
	for (int attempt = 0; attempt < 3 && !stub; ++attempt)
	{
		const std::uintptr_t cand = find_dll_cave(0x200, ignore);
		if (!cand)
			break;
		if (!write_protected(cand, thunk.data(), thunk.size()))
		{
			ignore = cand;
			continue;
		}
		stub = cand;
	}

	if (!stub)
	{
		g_last_fail = now;
		return false;
	}

	RaycastState empty{};
	empty.scale = 1.15f;
	if (!w_mem(g_hook.state, &empty, sizeof(empty)))
	{
		g_last_fail = now;
		return false;
	}

	FlushInstructionCache(g_Memory.GetHandle(), (void*)stub, thunk.size());
	mark_cfg(stub);

	if (!region_exec(stub))
	{
		g_last_fail = now;
		return false;
	}

	if (!write_protected(slot, &stub, sizeof(stub)) ||
		g_Memory.Read<std::uintptr_t>(slot) != stub)
	{
		g_last_fail = now;
		return false;
	}

	g_hook.module_base = base;
	g_hook.original = fn;
	g_hook.thunk = stub;
	g_hook.installed = true;
	g_hook.active = false;
	g_hook.wallbang = false;
	return true;
}

}

bool Ready() { return g_hook.installed; }
bool Aiming() { return g_hook.active; }
bool WallbangMode() { return g_hook.wallbang; }
std::uintptr_t OriginalHandler() { return g_hook.original; }

bool Install()
{
	return install();
}

void Remove()
{
	deactivate_state();
	restore_slot();
	g_hook.installed = false;

	if (!g_Memory.GetHandle() || !g_Memory.IsAlive())
	{
		free_state();
		g_hook = {};
	}
}

void Ensure(bool want)
{
	const std::uintptr_t base = module_base();
	const bool alive = g_Memory.GetHandle() && g_Memory.IsAlive();

	if (g_hook.installed && (!alive || !base))
	{
		Remove();
		g_last_base = 0;
		return;
	}

	if (g_hook.installed && base && g_last_base && base != g_last_base)
		Remove();

	if (base)
		g_last_base = base;

	if (!want)
	{
		if (g_hook.installed)
			Remove();
		return;
	}

	if (!base || !alive)
		return;

	const auto now = std::chrono::steady_clock::now();
	if (g_hook.installed && g_hook.thunk)
	{
		if (g_last_slot_check.time_since_epoch().count() == 0 ||
			now - g_last_slot_check >= std::chrono::milliseconds(250))
		{
			g_last_slot_check = now;
			if (g_Memory.Read<std::uintptr_t>(slot_addr(base)) != g_hook.thunk)
			{
				g_hook.installed = false;
				g_hook.module_base = 0;
			}
		}
	}

	if (!g_hook.installed)
		install();
}

void SetActive(bool on, const Vector3& world_target, bool wallbang)
{
	if (!on)
	{
		deactivate_state();
		return;
	}

	if (!g_hook.installed || !g_hook.state)
		return;

#pragma pack(push, 4)
	struct Payload {
		std::uint32_t reserved;
		float target_x;
		float target_y;
		float target_z;
		float scale;
		std::uint64_t calls;
		float cam_x;
		float cam_y;
		float cam_z;
	};
#pragma pack(pop)

	Payload p{};
	p.reserved = wallbang ? 1u : 0u;
	p.target_x = world_target.x;
	p.target_y = world_target.y;
	p.target_z = world_target.z;
	p.scale = 1.15f;

	if (Cheat::Globals::Workspace)
	{
		auto c = Cheat::Globals::Workspace->GetCurrentCamera();
		if (c && g_Memory.IsValid(c->address))
		{
			Camera cam_obj(c->address);
			Vector3 cp = cam_obj.GetPosition();
			p.cam_x = cp.x;
			p.cam_y = cp.y;
			p.cam_z = cp.z;
		}
	}

	w_mem(g_hook.state + offsetof(RaycastState, reserved), &p, sizeof(p));
	std::uint32_t one = 1;
	w_mem(g_hook.state + offsetof(RaycastState, active), &one, sizeof(one));
	g_hook.active = true;
	g_hook.wallbang = wallbang;
}

}
}
}
