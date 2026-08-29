#include "pch.h"
#include "CallGate.h"
#include "Reflect.h"
#include "app/Settings.h"
#include "core/console/Console.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/globals/Globals.h"

#include <cstring>
#include <string>
#include <vector>

namespace Cheat {
namespace Features {
namespace CallGate {
namespace {

// раскладка страницы состояния в чужом процессе
constexpr std::uintptr_t st_pending = 0x00; // u32, 1 = команда ждёт
constexpr std::uintptr_t st_done    = 0x04; // u32, 1 = stub отработал
constexpr std::uintptr_t st_fn      = 0x08;
constexpr std::uintptr_t st_a0      = 0x10;
constexpr std::uintptr_t st_a1      = 0x18;
constexpr std::uintptr_t st_a2      = 0x20;
constexpr std::uintptr_t st_a3      = 0x28;
constexpr std::uintptr_t st_ret     = 0x30;
constexpr std::uintptr_t st_calls   = 0x38; // счётчик попаданий в слот
constexpr std::uintptr_t st_tid     = 0x40; // 0 = любой поток
constexpr std::uintptr_t st_scratch = 0x100;

constexpr std::size_t stub_bytes = 0x120;

// в stub'е orig лежит сразу за jmp, а начало опознаётся по mov r10, imm64.
// по этим двум фактам мы узнаём свой же stub, оставшийся от прошлого
// запуска чита, и достаём из него настоящий оригинал
constexpr std::size_t stub_orig_off = 0x1F;

struct Gate
{
	bool          installed = false;
	std::uintptr_t slot = 0;   // desc + RaycastBoundFn
	std::uintptr_t orig = 0;
	std::uintptr_t stub = 0;
	std::uintptr_t state = 0;
	bool          stub_is_cave = false;
	std::size_t   cand = 0;
	std::string   method;
};

// движок зовёт их из скриптов чаще всего; порядок = ожидаемая горячесть.
// ВАЖНО: create (Instance.new) нельзя исполнять внутри метода, который уже
// держит блокировку DataModel/аллокатора (FindFirstChild, GetChildren, Clone
// гуляют по children и держат read-lock) — create хочет write-lock и тот же
// поток встаёт в deadlock (наблюдали: 27k hits, done так и не выставился).
// Поэтому спереди чистые read-only геттеры (IsA, GetAttribute): они не берут
// детей и не аллоцируют, и create внутри них доезжает до конца.
const char* const k_candidates[] = {
	"IsA", "GetAttribute", "FindFirstChild", "GetChildren",
	"FindFirstChildOfClass", "WaitForChild", "GetDescendants", "Clone",
};

constexpr std::size_t k_candidate_count =
	sizeof(k_candidates) / sizeof(k_candidates[0]);

Gate g_gate;
int  g_last_fail = 0;

// слот, по которому не пришло ни одного вызова, второй раз не пробуем
bool g_cold[k_candidate_count]{};

std::size_t page_sz()
{
	static std::size_t v = []() -> std::size_t {
		SYSTEM_INFO si{};
		GetSystemInfo(&si);
		return si.dwPageSize ? si.dwPageSize : 0x1000;
	}();
	return v;
}

bool is_exec_protect(DWORD p)
{
	const DWORD b = p & 0xFF;
	return b == PAGE_EXECUTE || b == PAGE_EXECUTE_READ ||
	       b == PAGE_EXECUTE_READWRITE || b == PAGE_EXECUTE_WRITECOPY;
}

DWORD query_protect(std::uintptr_t a)
{
	MEMORY_BASIC_INFORMATION mbi{};
	if (!VirtualQueryEx(g_Memory.GetHandle(), (void*)a, &mbi, sizeof(mbi)))
		return 0;
	if (mbi.State != MEM_COMMIT)
		return 0;
	return mbi.Protect;
}

bool write_protected(std::uintptr_t addr, const void* data, std::size_t size)
{
	if (!addr || !data || !size)
		return false;

	DWORD old = 0;
	const bool changed = g_Memory.Protect(addr, size, PAGE_EXECUTE_READWRITE, &old);
	const bool wrote = g_Memory.WriteRaw(addr, data, size) == size;
	if (changed)
		g_Memory.Protect(addr, size, old, nullptr);
	return wrote;
}

// без этого CFG убивает процесс на первом же вызове нашего stub
bool mark_cfg(std::uintptr_t t)
{
	HMODULE h = GetModuleHandleA("kernelbase.dll");
	if (!h)
		h = GetModuleHandleA("kernel32.dll");
	if (!h)
		return false;

	FARPROC proc = GetProcAddress(h, "SetProcessValidCallTargets");
	if (!proc)
		return false;

	struct Info { ULONG_PTR Offset; ULONG Flags; } info{};
	info.Offset = t & (page_sz() - 1);
	info.Flags = CFG_CALL_TARGET_VALID;

	using Fn = BOOL(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, void*);
	return ((Fn)proc)(g_Memory.GetHandle(),
		(void*)(t & ~((std::uintptr_t)page_sz() - 1)),
		page_sz(), 1, &info) != 0;
}

bool module_range(const wchar_t* name, std::uintptr_t* out_base, std::size_t* out_size)
{
	const std::uintptr_t base = g_Memory.GetModuleBase(name);
	if (!base)
		return false;

	const auto lfanew = g_Memory.Read<std::int32_t>(base + 0x3C);
	if (lfanew <= 0 || lfanew > 0x1000)
		return false;

	const auto img = g_Memory.Read<std::uint32_t>(base + lfanew + 0x50);
	if (img < 0x1000 || img > 0x20000000u)
		return false;

	*out_base = base;
	*out_size = img;
	return true;
}

// пробегаем закоммиченные куски диапазона; want_exec отсекает регионы
// до чтения, иначе тянули бы десятки мегабайт .text впустую
template <typename Cb>
void walk_committed(std::uintptr_t from, std::uintptr_t to, bool want_exec, Cb cb)
{
	std::vector<std::uint8_t> buf;
	std::uintptr_t addr = from;
	MEMORY_BASIC_INFORMATION mbi{};

	while (addr < to &&
	       VirtualQueryEx(g_Memory.GetHandle(), (void*)addr, &mbi, sizeof(mbi)))
	{
		const auto rb = (std::uintptr_t)mbi.BaseAddress;
		const auto rs = (std::size_t)mbi.RegionSize;
		const std::uintptr_t next = rb + rs;
		if (next <= addr)
			break;

		const bool usable = mbi.State == MEM_COMMIT &&
			!(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
			is_exec_protect(mbi.Protect) == want_exec;
		if (usable)
		{
			const std::uintptr_t s = (rb > from) ? rb : from;
			const std::uintptr_t e = (next < to) ? next : to;
			if (e > s)
			{
				buf.resize((std::size_t)(e - s));
				if (g_Memory.ReadRaw(s, buf.data(), buf.size()) == buf.size())
					cb(s, buf);
			}
		}

		addr = next;
	}
}

// find_bound_desc (old .data scan + interned-name lookup) removed — the
// function slot is now resolved by scanning Instance's FunctionDescriptors
// (see install_impl -> Reflect::FindFunction).

// оригинальный путь: в .data ищем дескриптор, у которого +8 == указатель имени
// (интернированный RBX::Name*, значение берём из класс-листа выше), +0 — vtable
// модуля, +0x80 — исполняемая реализация (FunctionDescriptor::Function).
std::uintptr_t find_bound_desc(std::uintptr_t base, std::size_t size,
                               std::uint64_t name_ptr)
{
	if (!name_ptr)
		return 0;

	std::uintptr_t found = 0;
	walk_committed(base, base + size, false,
		[&](std::uintptr_t at, const std::vector<std::uint8_t>& b)
		{
			if (found || b.size() < 16)
				return;

			for (std::size_t i = 8; i + 8 <= b.size(); i += 8)
			{
				std::uint64_t v = 0;
				std::memcpy(&v, b.data() + i, 8);
				if (v != name_ptr)
					continue;

				const std::uintptr_t desc = at + i - 8;

				// у настоящего дескриптора в +0 vftable модуля,
				// а в +0x80 исполняемый указатель на реализацию
				const auto vt = g_Memory.Read<std::uint64_t>(desc);
				if (vt < base || vt >= base + size)
					continue;

				const auto fn = g_Memory.Read<std::uint64_t>(
					desc + ::FunctionDescriptor::Function);
				if (fn && is_exec_protect(query_protect((std::uintptr_t)fn)))
				{
					found = desc;
					return;
				}
			}
		});

	return found;
}

// в чужих системных dll меньше шансов, что античит сверит .text роблокса
std::uintptr_t find_exec_cave(std::size_t need)
{
	static const wchar_t* pref[] = {
		L"winsta.dll", L"win32u.dll", L"uxtheme.dll", L"dwmapi.dll",
		L"msctf.dll", L"TextInputFramework.dll", L"CoreMessaging.dll",
	};

	for (auto* name : pref)
	{
		std::uintptr_t mb = 0;
		std::size_t ms = 0;
		if (!module_range(name, &mb, &ms))
			continue;

		std::uintptr_t hit = 0;
		walk_committed(mb + 0x1000, mb + ms, true,
			[&](std::uintptr_t at, const std::vector<std::uint8_t>& b)
			{
				if (hit || b.size() < need)
					return;

				std::size_t run = 0;
				for (std::size_t i = 0; i < b.size(); ++i)
				{
					if (b[i] != 0xCC && b[i] != 0x00)
					{
						run = 0;
						continue;
					}

					if (++run < need + 0x10)
						continue;

					const std::uintptr_t start = at + i + 1 - run;
					hit = (start + 0x0F) & ~(std::uintptr_t)0x0F;
					return;
				}
			});

		if (hit)
			return hit;
	}

	return 0;
}

void emit(std::vector<std::uint8_t>& c, std::initializer_list<std::uint8_t> b)
{
	c.insert(c.end(), b);
}

void emit_u32(std::vector<std::uint8_t>& c, std::uint32_t v)
{
	const auto* b = (const std::uint8_t*)&v;
	c.insert(c.end(), b, b + 4);
}

void emit_u64(std::vector<std::uint8_t>& c, std::uint64_t v)
{
	const auto* b = (const std::uint8_t*)&v;
	c.insert(c.end(), b, b + 8);
}

// stub не знает сигнатуру перехваченного метода: он сохраняет все
// регистры аргументов, выполняет одну нашу команду и уходит jmp'ом в
// оригинал с нетронутым стеком — поэтому годится для любого слота
std::vector<std::uint8_t> build_stub(std::uintptr_t state, std::uintptr_t orig)
{
	std::vector<std::uint8_t> c;

	emit(c, { 0x49, 0xBA });                      // mov r10, state
	emit_u64(c, state);
	emit(c, { 0xF0, 0x49, 0xFF, 0x42, 0x38 });    // lock inc qword [r10+calls]
	emit(c, { 0x41, 0x83, 0x3A, 0x00 });          // cmp dword [r10], 0
	emit(c, { 0x0F, 0x85 });                      // jne slow
	const std::size_t fix_slow = c.size();
	emit_u32(c, 0);

	const std::size_t pass = c.size();
	emit(c, { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 }); // jmp qword [rip+0]
	emit_u64(c, orig);

	const std::size_t slow = c.size();

	emit(c, { 0x49, 0x8B, 0x42, 0x40 });          // mov rax, [r10+tid]
	emit(c, { 0x48, 0x85, 0xC0 });                // test rax, rax
	emit(c, { 0x74, 0x00 });                      // je take
	const std::size_t fix_take = c.size() - 1;
	emit(c, { 0x65, 0x4C, 0x8B, 0x1C, 0x25 });    // mov r11, gs:[0x48]
	emit_u32(c, 0x48);
	emit(c, { 0x4C, 0x39, 0xD8 });                // cmp rax, r11
	emit(c, { 0x0F, 0x85 });                      // jne pass
	const std::size_t fix_pass1 = c.size();
	emit_u32(c, 0);

	// забираем команду атомарно, иначе два потока выполнят её дважды
	const std::size_t take = c.size();
	emit(c, { 0xB8, 0x01, 0x00, 0x00, 0x00 });    // mov eax, 1
	emit(c, { 0x45, 0x31, 0xDB });                // xor r11d, r11d
	emit(c, { 0xF0, 0x45, 0x0F, 0xB1, 0x1A });    // lock cmpxchg [r10], r11d
	emit(c, { 0x0F, 0x85 });                      // jne pass
	const std::size_t fix_pass2 = c.size();
	emit_u32(c, 0);

	emit(c, { 0x48, 0x81, 0xEC });                // sub rsp, 0x88
	emit_u32(c, 0x88);
	emit(c, { 0x4C, 0x89, 0x94, 0x24 });          // mov [rsp+0x80], r10
	emit_u32(c, 0x80);
	emit(c, { 0x48, 0x89, 0x4C, 0x24, 0x20 });    // mov [rsp+0x20], rcx
	emit(c, { 0x48, 0x89, 0x54, 0x24, 0x28 });    // mov [rsp+0x28], rdx
	emit(c, { 0x4C, 0x89, 0x44, 0x24, 0x30 });    // mov [rsp+0x30], r8
	emit(c, { 0x4C, 0x89, 0x4C, 0x24, 0x38 });    // mov [rsp+0x38], r9
	emit(c, { 0x0F, 0x11, 0x44, 0x24, 0x40 });    // movups [rsp+0x40], xmm0
	emit(c, { 0x0F, 0x11, 0x4C, 0x24, 0x50 });    // movups [rsp+0x50], xmm1
	emit(c, { 0x0F, 0x11, 0x54, 0x24, 0x60 });    // movups [rsp+0x60], xmm2
	emit(c, { 0x0F, 0x11, 0x5C, 0x24, 0x70 });    // movups [rsp+0x70], xmm3

	emit(c, { 0x49, 0x8B, 0x42, 0x08 });          // mov rax, [r10+fn]
	emit(c, { 0x49, 0x8B, 0x4A, 0x10 });          // mov rcx, [r10+a0]
	emit(c, { 0x49, 0x8B, 0x52, 0x18 });          // mov rdx, [r10+a1]
	emit(c, { 0x4D, 0x8B, 0x42, 0x20 });          // mov r8,  [r10+a2]
	emit(c, { 0x4D, 0x8B, 0x4A, 0x28 });          // mov r9,  [r10+a3]
	emit(c, { 0xFF, 0xD0 });                      // call rax

	emit(c, { 0x4C, 0x8B, 0x94, 0x24 });          // mov r10, [rsp+0x80]
	emit_u32(c, 0x80);
	emit(c, { 0x49, 0x89, 0x42, 0x30 });          // mov [r10+ret], rax
	emit(c, { 0x41, 0xC7, 0x42, 0x04 });          // mov dword [r10+done], 1
	emit_u32(c, 1);

	emit(c, { 0x0F, 0x10, 0x44, 0x24, 0x40 });    // movups xmm0, [rsp+0x40]
	emit(c, { 0x0F, 0x10, 0x4C, 0x24, 0x50 });    // movups xmm1, [rsp+0x50]
	emit(c, { 0x0F, 0x10, 0x54, 0x24, 0x60 });    // movups xmm2, [rsp+0x60]
	emit(c, { 0x0F, 0x10, 0x5C, 0x24, 0x70 });    // movups xmm3, [rsp+0x70]
	emit(c, { 0x48, 0x8B, 0x4C, 0x24, 0x20 });    // mov rcx, [rsp+0x20]
	emit(c, { 0x48, 0x8B, 0x54, 0x24, 0x28 });    // mov rdx, [rsp+0x28]
	emit(c, { 0x4C, 0x8B, 0x44, 0x24, 0x30 });    // mov r8,  [rsp+0x30]
	emit(c, { 0x4C, 0x8B, 0x4C, 0x24, 0x38 });    // mov r9,  [rsp+0x38]
	emit(c, { 0x48, 0x81, 0xC4 });                // add rsp, 0x88
	emit_u32(c, 0x88);
	emit(c, { 0xE9 });                            // jmp pass
	const std::size_t fix_pass3 = c.size();
	emit_u32(c, 0);

	auto rel = [&](std::size_t at, std::size_t target)
	{
		const std::int32_t v = (std::int32_t)((std::ptrdiff_t)target -
		                                      (std::ptrdiff_t)(at + 4));
		std::memcpy(c.data() + at, &v, 4);
	};

	rel(fix_slow, slow);
	rel(fix_pass1, pass);
	rel(fix_pass2, pass);
	rel(fix_pass3, pass);
	c[fix_take] = (std::uint8_t)(take - (fix_take + 1));
	return c;
}

} // namespace

// hybrid mode off -> the gate is treated as not installed, lua-side users
// (Instance.new, gatecall, ...) get "no call gate" / install failure
bool Ready()
{
	return g_gate.installed && Cheat::g_Settings.misc.hybrid_mode;
}
int LastFail() { return g_last_fail; }
std::uint64_t SlotAddress() { return g_gate.slot; }
std::uint64_t Scratch() { return g_gate.state ? g_gate.state + st_scratch : 0; }

std::uint64_t Calls()
{
	if (!g_gate.state)
		return 0;
	return g_Memory.Read<std::uint64_t>(g_gate.state + st_calls);
}

// чит мог упасть, не сняв хук: тогда в слоте наш прошлый stub, а
// настоящий оригинал зашит внутри него
static std::uint64_t unwrap_stale(std::uint64_t fn, std::uintptr_t base,
                                  std::size_t size)
{
	std::uint8_t head[2]{};
	if (g_Memory.ReadRaw((std::uintptr_t)fn, head, sizeof(head)) != sizeof(head))
		return 0;

	if (head[0] != 0x49 || head[1] != 0xBA)
		return 0;

	std::uint8_t jmp[2]{};
	if (g_Memory.ReadRaw((std::uintptr_t)fn + stub_orig_off - 6, jmp, sizeof(jmp))
		!= sizeof(jmp))
		return 0;

	if (jmp[0] != 0xFF || jmp[1] != 0x25)
		return 0;

	const auto orig = g_Memory.Read<std::uint64_t>(
		(std::uintptr_t)fn + stub_orig_off);
	if (orig < base || orig >= base + size)
		return 0;

	return orig;
}

static bool install_impl(const char* method_name, std::size_t from)
{
	if (g_gate.installed)
		return true;

	g_last_fail = 0;

	std::uintptr_t base = 0;
	std::size_t size = 0;
	if (!module_range(L"RobloxPlayerBeta.exe", &base, &size))
	{
		g_last_fail = 1;
		return false;
	}

	const bool forced = method_name && method_name[0];

	const char* method = nullptr;
	std::uintptr_t slot = 0;
	std::uint64_t orig = 0;
	std::size_t picked = from;

	// Candidates are Instance methods. Resolve Instance's ClassDescriptor once
	// through the Creators map (guide "Instance.new"), then scan its function
	// list for each candidate and hook FunctionDescriptor::Function (+0x80).
	std::uintptr_t instance_desc = 0;

	// Два прохода: если все кандидаты разом «замёрзли» (ни одного вызова за
	// ожидание), чёрный список сбрасываем и пробуем ещё раз — иначе гейт
	// заклинивает навсегда и каждый Instance.new вечно сыпет fail (2).
	bool found_slot = false;
	for (int pass = 0; pass < 2 && !found_slot; ++pass)
	{
		if (pass == 1)
			std::memset(g_cold, 0, sizeof(g_cold));

		for (std::size_t i = from; i < k_candidate_count; ++i)
		{
			if (!forced && g_cold[i])
				continue;

			const char* try_name = forced ? method_name : k_candidates[i];

			if (!instance_desc)
			{
				// Prefer the Workspace (WorldRoot) descriptor — the same source
				// Reflect::RaycastSlot uses and that's proven to resolve. Roblox class
				// descriptors include inherited members, so the Instance methods
				// (IsA, GetChildren, ...) are listed here too.
				const auto ws = Cheat::Globals::Workspace;
				if (ws && g_Memory.IsValid(ws->address))
					instance_desc = g_Memory.Read<std::uint64_t>(
						ws->address + ::Instance::ClassDescriptor);
				// Fallback: Instance's own descriptor from the Creators map.
				if (!instance_desc)
					instance_desc = Reflect::ClassDescriptorByName(base, "Instance");
			}
			// Class-list gives us this method's interned name pointer; then scan .data
			// for the actual BoundFuncDesc the engine dispatches through (original path).
			const std::uintptr_t func_desc = instance_desc
				? Reflect::FindFunction(instance_desc, try_name) : 0;
			const std::uint64_t name_ptr = func_desc
				? g_Memory.Read<std::uint64_t>(
					(std::uintptr_t)func_desc + ::Descriptor::Name) : 0;
			const std::uintptr_t desc = name_ptr ? find_bound_desc(base, size, name_ptr) : 0;
			if (desc)
			{
				const std::uintptr_t s = desc + ::FunctionDescriptor::Function;
				auto fn = g_Memory.Read<std::uint64_t>(s);

				// указатель наружу модуля = слот уже подменён. если это наш
				// собственный stub, достаём из него оригинал и садимся заново,
				// иначе слот чужой и трогать его нельзя
				if (fn < base || fn >= base + size)
					fn = unwrap_stale(fn, base, size);

				if (fn)
				{
					method = try_name;
					slot = s;
					orig = fn;
					picked = i;
					found_slot = true;
					break;
				}
			}

			if (forced)
				break;
		}
	}

	if (!slot || !orig)
	{
		g_last_fail = 2;
		return false;
	}

	const std::uintptr_t state = g_Memory.Alloc(page_sz(), PAGE_READWRITE);
	if (!state)
	{
		g_last_fail = 4;
		return false;
	}

	std::vector<std::uint8_t> zero(page_sz(), 0);
	g_Memory.WriteRaw(state, zero.data(), zero.size());

	bool cave = true;
	std::uintptr_t stub = find_exec_cave(stub_bytes);
	if (!stub)
	{
		cave = false;
		stub = g_Memory.Alloc(page_sz(), PAGE_EXECUTE_READWRITE);
	}

	if (!stub)
	{
		g_Memory.Free(state);
		g_last_fail = 5;
		return false;
	}

	const auto code = build_stub(state, (std::uintptr_t)orig);
	if (code.size() > stub_bytes || !write_protected(stub, code.data(), code.size()))
	{
		if (!cave)
			g_Memory.Free(stub);
		g_Memory.Free(state);
		g_last_fail = 6;
		return false;
	}

	mark_cfg(stub);

	if (!g_Memory.Write<std::uint64_t>(slot, (std::uint64_t)stub))
	{
		if (!cave)
			g_Memory.Free(stub);
		g_Memory.Free(state);
		g_last_fail = 7;
		return false;
	}

	g_gate.installed = true;
	g_gate.slot = slot;
	g_gate.orig = (std::uintptr_t)orig;
	g_gate.stub = stub;
	g_gate.state = state;
	g_gate.stub_is_cave = cave;
	g_gate.cand = picked;
	g_gate.method = method;
	return true;
}

bool Install(const char* method_name)
{
	// hybrid mode disabled -> never install / re-install the gate
	if (!Cheat::g_Settings.misc.hybrid_mode)
	{
		g_last_fail = 13;
		return false;
	}

	const bool was = g_gate.installed;
	const bool ok = install_impl(method_name, 0);
	if (!was)
	{
		Console::DumpGate(ok, g_gate.method.c_str(), g_gate.slot, g_gate.orig,
			g_gate.stub, g_gate.state, g_gate.stub_is_cave, g_last_fail);
	}
	return ok;
}

void Remove()
{
	if (!g_gate.installed)
		return;

	// снимаем хук, но состояние и стаб НЕ фризим: движок может прямо сейчас
	// исполнять старый cave-stub, который читает state-страницу. освобождение
	// памяти под живым стабом = UAF-краш. лучше утечка, чем AV (как в kids).
	g_Memory.Write<std::uint64_t>(g_gate.slot, (std::uint64_t)g_gate.orig);

	g_gate = Gate{};
}

static bool invoke_once(std::uint64_t fn, std::uint64_t a0, std::uint64_t a1,
                        std::uint64_t a2, std::uint64_t a3,
                        std::uint64_t* out_ret, unsigned timeout_ms,
                        bool* out_cold)
{
	if (out_ret)
		*out_ret = 0;
	if (out_cold)
		*out_cold = false;

	if (!g_gate.installed || !fn)
	{
		g_last_fail = 10;
		return false;
	}

	const std::uintptr_t s = g_gate.state;
	const auto hits_before = g_Memory.Read<std::uint64_t>(s + st_calls);
	g_Memory.Write<std::uint32_t>(s + st_done, 0);
	g_Memory.Write<std::uint64_t>(s + st_ret, 0);
	g_Memory.Write<std::uint64_t>(s + st_fn, fn);
	g_Memory.Write<std::uint64_t>(s + st_a0, a0);
	g_Memory.Write<std::uint64_t>(s + st_a1, a1);
	g_Memory.Write<std::uint64_t>(s + st_a2, a2);
	g_Memory.Write<std::uint64_t>(s + st_a3, a3);

	// pending пишем последним, чтобы stub не подхватил недописанную команду
	if (!g_Memory.Write<std::uint32_t>(s + st_pending, 1))
	{
		g_last_fail = 11;
		return false;
	}

	const DWORD deadline = GetTickCount() + timeout_ms;
	while (GetTickCount() < deadline)
	{
		if (g_Memory.Read<std::uint32_t>(s + st_done) == 1)
		{
			if (out_ret)
				*out_ret = g_Memory.Read<std::uint64_t>(s + st_ret);
			g_last_fail = 0;
			return true;
		}

		if (!g_Memory.IsAlive())
			break;

		Sleep(1);
	}

	g_Memory.Write<std::uint32_t>(s + st_pending, 0);

	const auto hits_after = g_Memory.Read<std::uint64_t>(s + st_calls);
	if (out_cold)
		*out_cold = hits_after == hits_before;

	Console::GateTimeout(g_gate.method.c_str(), hits_after);
	g_last_fail = 12;
	return false;
}

// слот, по которому за всё ожидание не пришло ни одного вызова, бесполезен:
// молча переезжаем на следующего кандидата и пробуем ещё раз
bool Invoke(std::uint64_t fn, std::uint64_t a0, std::uint64_t a1,
            std::uint64_t a2, std::uint64_t a3,
            std::uint64_t* out_ret, unsigned timeout_ms)
{
	for (std::size_t tries = 0; tries < k_candidate_count; ++tries)
	{
		bool cold = false;
		if (invoke_once(fn, a0, a1, a2, a3, out_ret, timeout_ms, &cold))
			return true;

		if (!cold || !g_gate.installed)
			return false;

		g_cold[g_gate.cand] = true;
		Remove();
		// stub живёт в code cave: движок мог ещё исполнять старый код.
		// без паузы install_impl перезапишет страницу под ногами — AV.
		Sleep(15);
		if (!install_impl(nullptr, 0))
			return false;

		Console::DumpGate(true, g_gate.method.c_str(), g_gate.slot, g_gate.orig,
			g_gate.stub, g_gate.state, g_gate.stub_is_cave, 0);
	}

	return false;
}

} // namespace CallGate
} // namespace Features
} // namespace Cheat
