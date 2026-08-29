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
#include <cmath>
#include <cstdarg>
#include "app/Settings.h"

namespace Cheat {
namespace Features {
namespace RaycastSilent {
namespace {

// ---- gelato shellcode-execution model -----------------------------------------
// The raycast hooking here is a port of booted-off-gelato's silentaim raycast
// hook. The stub is a tiny data-block-driven redirector that copies override
// vectors into the ray params and tail-jumps to the original native. It lives in
// a code cave inside d3d11.dll (a user-mode graphics DLL loaded on both Win10 and
// Win11) -- no system-DLL padding pages, no CFG SetProcessValidCallTargets, no
// VirtualAllocEx'd state page. That is what keeps it stable on Win10.

// Data block layout shared with the stub; offsets must not change.
constexpr std::uint64_t off_call_counter       = 0x00;
constexpr std::uint64_t off_last_origin        = 0x08;
constexpr std::uint64_t off_last_direction     = 0x14;
constexpr std::uint64_t off_override_direction = 0x28;
constexpr std::uint64_t off_new_direction      = 0x2C;
constexpr std::uint64_t off_override_origin    = 0x38;
constexpr std::uint64_t off_new_origin         = 0x3C;
constexpr std::size_t   data_block_bytes       = 0x50;
constexpr std::size_t   stub_slot_bytes        = 0x100;

struct RaycastTarget {
	const char* name;
	bool packed_ray;
};

// Same set gelato hooks. Descriptors resolve dynamically via the WorldRoot
// FunctionDescriptor scan (no hard-coded RVA).
constexpr RaycastTarget k_targets[] = {
	{ "Raycast",                     false },
	{ "FindPartOnRay",               true  },
	{ "FindPartOnRayWithIgnoreList", true  },
	{ "FindPartOnRayWithWhitelist",  true  },
	{ "findPartOnRay",               true  },
};
constexpr std::size_t k_target_count = sizeof(k_targets) / sizeof(k_targets[0]);

struct HookTarget {
	std::uintptr_t slot = 0;     // descriptor function-pointer address
	std::uintptr_t stub = 0;     // stub address inside the cave
	std::uintptr_t original = 0; // original native pointer
	bool packed = false;
};

struct Hook {
	std::uintptr_t data_block = 0;
	std::uintptr_t module_base = 0;
	std::vector<HookTarget> targets;
	bool installed = false;
	bool active = false;
	bool wallbang = false;
};

Hook g_hook{};
std::uintptr_t g_original = 0;
auto g_last_fail = std::chrono::steady_clock::time_point{};
std::uintptr_t g_last_base = 0;
auto g_last_slot_check = std::chrono::steady_clock::time_point{};

// ---- debug logging -------------------------------------------------------------
// Toggleable from the silent-aim tab ("raycast debug logs"). Every stage of the
// shellcode-execution chain prints into the jewsploit console with a [raycast]
// prefix: descriptor/vtable slot resolution, d3d11.dll code-cave search, cave
// protection, stub writes, slot patch verify, arm + stub call-counter readback.
bool g_debug = false;

void sync_debug()
{
	const bool want = g_Settings.aim.raycast_debug;
	if (want == g_debug)
		return;

	g_debug = want;
	if (want)
	{
		Cheat::Console::Log(Cheat::Console::Color::Yellow,
		                    "[raycast] debug ON");
		if (g_hook.installed)
		{
			Cheat::Console::Log(Cheat::Console::Color::Green,
			                    "[raycast] hook installed: %d target(s), "
			                    "data_block=%p roblox base=%p",
			                    (int)g_hook.targets.size(),
			                    (void*)g_hook.data_block,
			                    (void*)g_hook.module_base);
			for (const auto& t : g_hook.targets)
				Cheat::Console::Log(Cheat::Console::Color::Cyan,
				                    "[raycast]   slot=%p -> stub=%p "
				                    "(orig=%p, %s ray)",
				                    (void*)t.slot, (void*)t.stub,
				                    (void*)t.original,
				                    t.packed ? "packed" : "open");
		}
		else
		{
			Cheat::Console::Log(Cheat::Console::Color::Red,
			                    "[raycast] hook NOT installed "
			                    "(install will retry every 1.5s)");
		}
	}
	else
	{
		Cheat::Console::Log(Cheat::Console::Color::Yellow,
		                    "[raycast] debug OFF");
	}
}

void dbg(Cheat::Console::Color c, const char* fmt, ...)
{
	if (!g_debug)
		return;
	char buf[384];
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
	va_end(ap);
	Cheat::Console::Log(c, "[raycast] %s", buf);
}

// Same as dbg but at most once per second per call site (pass a static
// steady_clock::time_point) so per-frame failure paths can't spam the console.
void dbg_rate(std::chrono::steady_clock::time_point& last,
              Cheat::Console::Color c, const char* fmt, ...)
{
	if (!g_debug)
		return;
	const auto now = std::chrono::steady_clock::now();
	if (last.time_since_epoch().count() != 0 &&
		now - last < std::chrono::milliseconds(1000))
		return;
	last = now;
	char buf[384];
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
	va_end(ap);
	Cheat::Console::Log(c, "[raycast] %s", buf);
}

bool addr_ok(std::uintptr_t a)
{
	return a >= 0x10000ull && a < 0x00007FFFFFFFFFFFull;
}

bool w_mem(std::uintptr_t a, const void* d, std::size_t s)
{
	if (!addr_ok(a) || !d || !s || !g_Memory.GetHandle())
		return false;
	// Direct raw-syscall write bypasses user-mode WPM hooks that break
	// silent aim on Windows 10; falls back to WinAPI internally.
	return g_Memory.WriteRawDirect(a, d, s) == s;
}

bool read_val(std::uintptr_t a, void* d, std::size_t s)
{
	return g_Memory.ReadRawDirect(a, d, s) == s;
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

	// Scan every section (not just executable ones) for padding, exactly like
	// the gelato cave search -- the d3d11.dll padding we target is not in an
	// executable section.
	for (WORD i = 0; i < nt.FileHeader.NumberOfSections; ++i)
	{
		IMAGE_SECTION_HEADER sec{};
		if (!read_val(sec_base + (std::uintptr_t)i * sizeof(sec), &sec, sizeof(sec)))
			break;

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
			if (g_hook.data_block && aligned == g_hook.data_block)
				continue;
			return aligned;
		}
	}
	return 0;
}

void push_bytes(std::vector<std::uint8_t>& code, std::initializer_list<std::uint8_t> bytes)
{
	for (auto b : bytes)
		code.push_back(b);
}

void push64(std::vector<std::uint8_t>& code, std::uint64_t value)
{
	for (int i = 0; i < 8; i++)
		code.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}

// gelato's stub, verbatim. Register contract at the hooked descriptor native:
//   r11 = data-block base (clobbered, free)
//   r8  = ray origin (or packed ray origin+dir)
//   r9  = direction    (non-packed only)
// The stub bumps the call counter, records the last ray, copies the override
// origin/direction from the data block onto the params when armed, then
// tail-jumps to the original native. No stack use, no XMM, no call.
std::vector<std::uint8_t> build_stub(std::uint64_t data_block, std::uint64_t original, bool packed_ray)
{
	std::vector<std::uint8_t> code;
	code.reserve(160);

	push_bytes(code, { 0x49, 0xBB });
	push64(code, data_block);
	push_bytes(code, { 0x49, 0xFF, 0x03 });          // inc qword [r11] (call counter)

	push_bytes(code, { 0x41, 0x8B, 0x00 });
	push_bytes(code, { 0x41, 0x89, 0x43, 0x08 });
	push_bytes(code, { 0x41, 0x8B, 0x40, 0x04 });
	push_bytes(code, { 0x41, 0x89, 0x43, 0x0C });
	push_bytes(code, { 0x41, 0x8B, 0x40, 0x08 });
	push_bytes(code, { 0x41, 0x89, 0x43, 0x10 });    // origin -> last_origin

	if (packed_ray)
	{
		push_bytes(code, { 0x41, 0x8B, 0x40, 0x0C });
		push_bytes(code, { 0x41, 0x89, 0x43, 0x14 });
		push_bytes(code, { 0x41, 0x8B, 0x40, 0x10 });
		push_bytes(code, { 0x41, 0x89, 0x43, 0x18 });
		push_bytes(code, { 0x41, 0x8B, 0x40, 0x14 });
		push_bytes(code, { 0x41, 0x89, 0x43, 0x1C }); // packed dir -> last_direction

		push_bytes(code, { 0x41, 0x83, 0x7B, 0x28, 0x01 }); // cmp [r11+0x28],1
		push_bytes(code, { 0x75, 0x18 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x2C });
		push_bytes(code, { 0x41, 0x89, 0x40, 0x0C });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x30 });
		push_bytes(code, { 0x41, 0x89, 0x40, 0x10 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x34 });
		push_bytes(code, { 0x41, 0x89, 0x40, 0x14 }); // new_direction -> packed dir

		push_bytes(code, { 0x41, 0x83, 0x7B, 0x38, 0x01 }); // cmp [r11+0x38],1
		push_bytes(code, { 0x75, 0x17 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x3C });
		push_bytes(code, { 0x41, 0x89, 0x00 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x40 });
		push_bytes(code, { 0x41, 0x89, 0x40, 0x04 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x44 });
		push_bytes(code, { 0x41, 0x89, 0x40, 0x08 }); // new_origin -> packed origin
	}
	else
	{
		push_bytes(code, { 0x41, 0x8B, 0x01 });
		push_bytes(code, { 0x41, 0x89, 0x43, 0x14 });
		push_bytes(code, { 0x41, 0x8B, 0x41, 0x04 });
		push_bytes(code, { 0x41, 0x89, 0x43, 0x18 });
		push_bytes(code, { 0x41, 0x8B, 0x41, 0x08 });
		push_bytes(code, { 0x41, 0x89, 0x43, 0x1C }); // [r9] dir -> last_direction

		push_bytes(code, { 0x41, 0x83, 0x7B, 0x28, 0x01 }); // cmp [r11+0x28],1
		push_bytes(code, { 0x75, 0x17 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x2C });
		push_bytes(code, { 0x41, 0x89, 0x01 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x30 });
		push_bytes(code, { 0x41, 0x89, 0x41, 0x04 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x34 });
		push_bytes(code, { 0x41, 0x89, 0x41, 0x08 }); // new_direction -> [r9]

		push_bytes(code, { 0x41, 0x83, 0x7B, 0x38, 0x01 }); // cmp [r11+0x38],1
		push_bytes(code, { 0x75, 0x17 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x3C });
		push_bytes(code, { 0x41, 0x89, 0x00 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x40 });
		push_bytes(code, { 0x41, 0x89, 0x40, 0x04 });
		push_bytes(code, { 0x41, 0x8B, 0x43, 0x44 });
		push_bytes(code, { 0x41, 0x89, 0x40, 0x08 }); // new_origin -> [r8]
	}

	push_bytes(code, { 0x48, 0xB8 });  // mov rax, original
	push64(code, original);
	push_bytes(code, { 0xFF, 0xE0 });  // jmp rax
	return code;
}

std::uintptr_t find_dll_cave(std::size_t need, std::uintptr_t ignore)
{
	// Gelato digs its raycast shellcode into d3d11.dll (a user-mode graphics
	// DLL that Roblox loads on both Win10 and Win11) instead of the kernel
	// bridge DLLs whose padding pages are handled differently between Win10
	// and Win11 -- the Win10 crash source.
	static const wchar_t* pref[] = {
		L"d3d11.dll",
	};
	for (std::size_t i = 0; i < sizeof(pref) / sizeof(pref[0]); ++i)
	{
		const std::uintptr_t mod = g_Memory.GetModuleBase(pref[i]);
		if (!mod)
		{
			dbg(Cheat::Console::Color::Red,
			    "cave: %ls is NOT loaded in the target", pref[i]);
			continue;
		}
		dbg(Cheat::Console::Color::Cyan,
		    "cave: scanning %ls @ %p for %zu bytes of 0x00/0xCC/0x90 padding",
		    pref[i], (void*)mod, need);
		const std::uintptr_t cave = find_cave_in_module(mod, need, 0x1000u, ignore);
		if (cave)
			return cave;
	}
	dbg(Cheat::Console::Color::Red,
	    "cave: no usable %zu-byte padding run found (shellcode cannot be placed)",
	    need);
	return 0;
}

std::uintptr_t module_base()
{
	return g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
}

// Resolve a WorldRoot method's descriptor function-pointer slot by name
// (same reflection source Reflect::RaycastSlot uses).
std::uintptr_t find_slot(const char* name)
{
	const auto ws = Cheat::Globals::Workspace;
	if (!ws || !g_Memory.IsValid(ws->address))
		return 0;
	const auto class_desc = g_Memory.Read<std::uint64_t>(ws->address + ::Instance::ClassDescriptor);
	if (!class_desc || !g_Memory.IsValid((std::uintptr_t)class_desc))
		return 0;
	const auto func_desc = Reflect::FindFunction((std::uintptr_t)class_desc, name);
	if (!func_desc)
		return 0;
	return (std::uintptr_t)func_desc + ::FunctionDescriptor::Function;
}

// Disarm the redirect so the next ray passes through untouched.
void clear_overrides()
{
	if (!g_hook.data_block || !g_Memory.GetHandle())
		return;
	std::uint32_t zero = 0;
	w_mem(g_hook.data_block + off_override_direction, &zero, sizeof(zero));
	w_mem(g_hook.data_block + off_override_origin, &zero, sizeof(zero));
}

void deactivate()
{
	clear_overrides();
	g_hook.active = false;
	g_hook.wallbang = false;
}

bool restore_slots()
{
	if (!g_hook.module_base || !g_Memory.GetHandle() || !g_Memory.IsAlive())
		return false;
	if (module_base() != g_hook.module_base)
		return false;

	int restored_n = 0;
	for (const auto& t : g_hook.targets)
	{
		if (!t.slot || !t.original)
			continue;
		if (g_Memory.Read<std::uintptr_t>(t.slot) != t.stub)
			continue;
		write_protected(t.slot, &t.original, sizeof(t.original));
		++restored_n;
	}
	dbg(Cheat::Console::Color::Cyan,
	    "restore: %d/%d slot(s) put back to original natives",
	    restored_n, (int)g_hook.targets.size());
	return true;
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

	dbg(Cheat::Console::Color::Yellow,
	    "install: attempting (roblox base=%p, process alive)", (void*)base);

	// Resolve every target slot + original native first (fail fast).
	struct Resolved {
		std::uintptr_t slot;
		std::uintptr_t orig;
		bool packed;
		const char* name;
	};
	std::vector<Resolved> res;
	res.reserve(k_target_count);
	for (std::size_t i = 0; i < k_target_count; ++i)
	{
		const std::uintptr_t slot = find_slot(k_targets[i].name);
		if (!slot)
		{
			dbg(Cheat::Console::Color::Red,
			    "resolve: '%s' descriptor NOT found (no function slot)",
			    k_targets[i].name);
			continue;
		}
		const std::uintptr_t orig = g_Memory.Read<std::uintptr_t>(slot);
		if (!addr_ok(orig) || !region_exec(orig))
		{
			dbg(Cheat::Console::Color::Red,
			    "resolve: '%s' slot=%p but native %p is invalid/not executable",
			    k_targets[i].name, (void*)slot, (void*)orig);
			continue;
		}
		dbg(Cheat::Console::Color::Green,
		    "resolve: '%s' slot=%p -> native=%p (%s ray)",
		    k_targets[i].name, (void*)slot, (void*)orig,
		    k_targets[i].packed_ray ? "packed" : "open");
		res.push_back({ slot, orig, k_targets[i].packed_ray, k_targets[i].name });
	}
	if (res.empty())
	{
		dbg(Cheat::Console::Color::Red,
		    "install FAILED: no raycast descriptors resolved "
		    "(Workspace missing or reflection offsets stale)");
		g_last_fail = now;
		return false;
	}

	// One contiguous cave in d3d11.dll: data block + per-target stub slots.
	const std::size_t need = data_block_bytes + stub_slot_bytes * res.size();
	const std::uintptr_t cave = find_dll_cave(need, 0);
	if (!cave)
	{
		g_last_fail = now;
		return false;
	}
	dbg(Cheat::Console::Color::Green,
	    "cave: found at %p (data block + %d stub slots)", (void*)cave, (int)res.size());

	DWORD old = 0;
	if (!protect_remote(cave, need, PAGE_EXECUTE_READWRITE, &old))
	{
		dbg(Cheat::Console::Color::Red,
		    "install FAILED: VirtualProtectEx(RWX) on cave %p denied", (void*)cave);
		g_last_fail = now;
		return false;
	}
	dbg(Cheat::Console::Color::Cyan,
	    "cave: protection flipped to RWX (old=0x%lX)", (unsigned long)old);

	std::uint8_t zero_block[data_block_bytes]{};
	if (!w_mem(cave, zero_block, sizeof(zero_block)))
	{
		if (old)
			protect_remote(cave, need, old, nullptr);
		dbg(Cheat::Console::Color::Red,
		    "install FAILED: could not zero the data block @ %p "
		    "(raw write denied -- shellcode would run with garbage state)",
		    (void*)cave);
		g_last_fail = now;
		return false;
	}

	std::vector<HookTarget> installed_av;
	installed_av.reserve(res.size());
	for (std::size_t i = 0; i < res.size(); ++i)
	{
		const std::uintptr_t stub = cave + data_block_bytes + i * stub_slot_bytes;
		auto bytes = build_stub(cave, res[i].orig, res[i].packed);
		if (bytes.size() > stub_slot_bytes)
		{
			dbg(Cheat::Console::Color::Red,
			    "stub: '%s' generated %zu bytes, slot only holds %d -- skipped",
			    res[i].name, bytes.size(), (int)stub_slot_bytes);
			continue;
		}
		if (!write_protected(stub, bytes.data(), bytes.size()))
		{
			dbg(Cheat::Console::Color::Red,
			    "stub: write FAILED @ %p for '%s'", (void*)stub, res[i].name);
			continue;
		}
		if (!write_protected(res[i].slot, &stub, sizeof(stub)))
		{
			dbg(Cheat::Console::Color::Red,
			    "hook: slot patch FAILED @ %p for '%s' (stub written but never called)",
			    (void*)res[i].slot, res[i].name);
			continue;
		}
		if (g_Memory.Read<std::uintptr_t>(res[i].slot) != stub)
		{
			dbg(Cheat::Console::Color::Red,
			    "hook: slot verify mismatch @ %p for '%s' "
			    "(something reverted the patch)",
			    (void*)res[i].slot, res[i].name);
			continue;
		}
		dbg(Cheat::Console::Color::Green,
		    "hook: '%s' slot=%p -> stub=%p OK",
		    res[i].name, (void*)res[i].slot, (void*)stub);
		installed_av.push_back({ res[i].slot, stub, res[i].orig, res[i].packed });
	}

	if (installed_av.empty())
	{
		if (old)
			protect_remote(cave, need, old, nullptr);
		w_mem(cave, zero_block, sizeof(zero_block));
		dbg(Cheat::Console::Color::Red,
		    "install FAILED: no targets were hooked, cave restored");
		g_last_fail = now;
		return false;
	}

	FlushInstructionCache(g_Memory.GetHandle(), (void*)cave, need);

	g_hook.data_block = cave;
	g_hook.module_base = base;
	g_hook.targets = std::move(installed_av);
	g_original = g_hook.targets[0].original;
	g_hook.installed = true;
	g_hook.active = false;
	g_hook.wallbang = false;

	dbg(Cheat::Console::Color::Green,
	    "install OK: %d target(s) hooked, shellcode live in d3d11.dll "
	    "(data_block=%p, call counter at +0x00)",
	    (int)g_hook.targets.size(), (void*)g_hook.data_block);
	return true;
}

} // namespace

bool Ready() { return g_hook.installed; }
bool Aiming() { return g_hook.active; }
bool WallbangMode() { return g_hook.wallbang; }
std::uintptr_t OriginalHandler() { return g_original; }

bool Install()
{
	return install();
}

void Remove()
{
	dbg(Cheat::Console::Color::Yellow, "Remove: unhooking and restoring slots");
	deactivate();
	restore_slots();
	g_hook.installed = false;

	if (!g_Memory.GetHandle() || !g_Memory.IsAlive())
	{
		g_hook = {};
		g_original = 0;
		g_last_base = 0;
	}
}

void Ensure(bool want)
{
	sync_debug();

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
	if (g_hook.installed && g_hook.data_block)
	{
		if (g_last_slot_check.time_since_epoch().count() == 0 ||
			now - g_last_slot_check >= std::chrono::milliseconds(250))
		{
			g_last_slot_check = now;
			bool intact = !g_hook.targets.empty();
			for (const auto& t : g_hook.targets)
			{
				if (g_Memory.Read<std::uintptr_t>(t.slot) != t.stub)
				{
					intact = false;
					break;
				}
			}
			if (!intact)
			{
				dbg(Cheat::Console::Color::Red,
				    "slot check: a hooked slot was stomped/reverted "
				    "-- marking for reinstall");
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
		deactivate();
		return;
	}

	sync_debug();

	if (!g_hook.installed || !g_hook.data_block)
	{
		static auto s_last = std::chrono::steady_clock::time_point{};
		dbg_rate(s_last, Cheat::Console::Color::Red,
		         "SetActive: hook not installed, silent ray dropped "
		         "(install keeps retrying every 1.5s)");
		return;
	}

	Vector3 cam{};
	bool have_cam = false;
	if (Cheat::Globals::Workspace)
	{
		auto c = Cheat::Globals::Workspace->GetCurrentCamera();
		if (c && g_Memory.IsValid(c->address))
		{
			Camera cam_obj(c->address);
			cam = cam_obj.GetPosition();
			have_cam = true;
		}
	}
	if (!have_cam)
	{
		static auto s_last = std::chrono::steady_clock::time_point{};
		dbg_rate(s_last, Cheat::Console::Color::Red,
		         "SetActive: no camera position readable -- cannot arm");
		deactivate();
		return;
	}

	const float dx = world_target.x - cam.x;
	const float dy = world_target.y - cam.y;
	const float dz = world_target.z - cam.z;
	const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
	if (len <= 0.001f)
	{
		static auto s_last = std::chrono::steady_clock::time_point{};
		dbg_rate(s_last, Cheat::Console::Color::Red,
		         "SetActive: zero-length ray (target == camera) -- cannot arm");
		deactivate();
		return;
	}

	// Mirror the gelato silentaim behaviour: origin anchored at the camera
	// when aiming directly, or pulled just behind the target for wallbang
	// (magic-bullet) hits. Direction magnitude = ray extent.
	float org[3]{};
	float dir[3]{};
	if (wallbang)
	{
		constexpr float k_scale = 1.15f;
		const float ux = dx / len, uy = dy / len, uz = dz / len;
		org[0] = world_target.x - ux * k_scale;
		org[1] = world_target.y - uy * k_scale;
		org[2] = world_target.z - uz * k_scale;
		dir[0] = ux * (k_scale * 2.0f);
		dir[1] = uy * (k_scale * 2.0f);
		dir[2] = uz * (k_scale * 2.0f);
	}
	else
	{
		constexpr float k_range = 10000.0f;
		const float ux = dx / len, uy = dy / len, uz = dz / len;
		org[0] = cam.x;
		org[1] = cam.y;
		org[2] = cam.z;
		dir[0] = ux * k_range;
		dir[1] = uy * k_range;
		dir[2] = uz * k_range;
	}

	const std::uint32_t one = 1;
	w_mem(g_hook.data_block + off_new_origin, org, sizeof(org));
	w_mem(g_hook.data_block + off_new_direction, dir, sizeof(dir));
	w_mem(g_hook.data_block + off_override_origin, &one, sizeof(one));
	w_mem(g_hook.data_block + off_override_direction, &one, sizeof(one));

	// debug: verify the override block actually landed and watch the stub's
	// own call counter -- that counter only moves when the shellcode in the
	// d3d11.dll cave really executes inside Roblox.
	if (g_debug)
	{
		const bool first_arm = !g_hook.active;

		std::uint32_t ovr_dir_rb = 0, ovr_org_rb = 0;
		read_val(g_hook.data_block + off_override_direction, &ovr_dir_rb, sizeof(ovr_dir_rb));
		read_val(g_hook.data_block + off_override_origin, &ovr_org_rb, sizeof(ovr_org_rb));

		if (first_arm)
		{
			dbg(Cheat::Console::Color::Green,
			    "ARMED origin=(%.1f,%.1f,%.1f) dir=(%.1f,%.1f,%.1f) wallbang=%d",
			    org[0], org[1], org[2], dir[0], dir[1], dir[2],
			    (int)wallbang);
			dbg(ovr_dir_rb == 1 && ovr_org_rb == 1
			        ? Cheat::Console::Color::Green
			        : Cheat::Console::Color::Red,
			    "override flag readback: dir=%u origin=%u %s",
			    ovr_dir_rb, ovr_org_rb,
			    ovr_dir_rb == 1 && ovr_org_rb == 1
			        ? "(writes to the data block work)"
			        : "(READBACK MISMATCH -- writes are not landing!)");
		}

		static auto s_last_stub_check = std::chrono::steady_clock::time_point{};
		static std::uint64_t s_last_calls = 0;
		const auto now = std::chrono::steady_clock::now();
		if (s_last_stub_check.time_since_epoch().count() == 0 ||
			now - s_last_stub_check >= std::chrono::milliseconds(2000))
		{
			s_last_stub_check = now;
			std::uint64_t calls = 0;
			if (read_val(g_hook.data_block + off_call_counter, &calls, sizeof(calls)))
			{
				if (calls != s_last_calls)
					dbg(Cheat::Console::Color::Green,
					    "shellcode EXECUTING: call counter %llu -> %llu "
					    "(hooked raycast natives are running the stub)",
					    (unsigned long long)s_last_calls,
					    (unsigned long long)calls);
				else if (first_arm)
					dbg(Cheat::Console::Color::Red,
					    "shellcode NOT executing yet: call counter stuck at %llu "
					    "(Roblox hasn't raycast through the hooked slot)",
					    (unsigned long long)calls);
				s_last_calls = calls;
			}
			else
			{
				dbg(Cheat::Console::Color::Red,
				    "cannot read stub call counter @ %p (data block unreadable)",
				    (void*)(g_hook.data_block + off_call_counter));
			}
		}
	}

	g_hook.active = true;
	g_hook.wallbang = wallbang;
}

} // namespace RaycastSilent
} // namespace Features
} // namespace Cheat