#include "pch.h"
#include "LuaGc.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaGc {
namespace {

#if 0 // ===== LuaGc implementation disabled (setgc/getgc) — migrating to external offsets; keep for later =====
constexpr int k_gco_chain_max = 80000;
constexpr int k_page_chain_max = 200000;
constexpr int k_page_blocks_max = 20000;
constexpr int k_sizeclass_count = 48;
constexpr int k_strt_chain_max = 100000;
constexpr std::size_t k_snapshot_max = 1u << 20;
constexpr int k_result_max = 100000;
constexpr int k_result_default = 20000;
constexpr int k_keys_per_key_default = 400;
constexpr int k_hits_max = 4096;
constexpr std::chrono::milliseconds k_snapshot_ttl{ 1500 };
constexpr std::chrono::milliseconds k_strt_ttl{ 3000 };

bool LooksLikeLuaState(std::uint64_t L)
{
	if (!g_Memory.IsValid(L) || (L & 0xF))
		return false;

	const std::uint64_t top = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Top);
	const std::uint64_t base = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Base);
	const std::uint64_t G = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Global);

	if (!g_Memory.IsValid(top) || !g_Memory.IsValid(base) || !g_Memory.IsValid(G))
		return false;

	if ((top | base) & 0xF)
		return false;

	if (top < base || (top - base) > 0x40000)
		return false;

	const std::uint64_t tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
	const std::uint8_t st = g_Memory.Read<std::uint8_t>(G + Offsets::LuauGlobal::gcstate);

	if (tb < 1024 || tb > (512ull << 20))
		return false;

	if (st > 4)
		return false;

	return true;
}

std::uint64_t FindScriptContext()
{
	if (!g_Memory.IsAttached())
		return 0;

	const auto& dm = Globals::InstanceDataModel;
	if (!g_Memory.IsValid(dm.address))
		return 0;

	for (const auto& c : dm.GetChildren())
	{
		if (!g_Memory.IsValid(c.address))
			continue;

		if (c.GetClassName() == "ScriptContext")
			return c.address;
	}

	return 0;
}

struct GameVm
{
	std::uint64_t L = 0;
	std::uint64_t G = 0;
	std::uint64_t tb = 0;
	std::uint32_t wrap = 0;
};

void CollectGameVms(std::vector<GameVm>& out)
{
	out.clear();
	const std::uint64_t sc = FindScriptContext();
	if (!sc)
		return;

	const uintptr_t wraps[3] = {
		Offsets::ScriptContext::VmWrapper,
		Offsets::ScriptContext::VmWrapperBig,
		Offsets::ScriptContext::VmWrapper2,
	};
	const uintptr_t loffs[2] = {
		Offsets::ScriptContext::LuaState,
		Offsets::ScriptContext::LuaStateAlt,
	};

	std::unordered_set<std::uint64_t> seen_g;

	for (int wi = 0; wi < 3; ++wi)
	{
		const std::uint64_t w = g_Memory.Read<std::uint64_t>(sc + wraps[wi]);
		if (!g_Memory.IsValid(w))
			continue;

		for (int li = 0; li < 2; ++li)
		{
			const std::uint64_t L = g_Memory.Read<std::uint64_t>(w + loffs[li]);
			if (!LooksLikeLuaState(L))
				continue;

			const std::uint64_t G = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Global);
			if (!g_Memory.IsValid(G))
				continue;

			if (!seen_g.insert(G).second)
				continue;

			GameVm vm{};
			vm.L = L;
			vm.G = G;
			vm.tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
			vm.wrap = (std::uint32_t)wraps[wi];
			out.push_back(vm);
		}
	}
}

// SC wraps -> L с большим totalbytes (setgc)
std::uint64_t ResolveGameLuaState()
{
	std::vector<GameVm> vms;
	CollectGameVms(vms);

	std::uint64_t best = 0;
	std::uint64_t best_tb = 0;
	for (const auto& vm : vms)
	{
		if (vm.tb > best_tb)
		{
			best_tb = vm.tb;
			best = vm.L;
		}
	}
	return best;
}

std::uint64_t GameGlobal()
{
	const std::uint64_t L = ResolveGameLuaState();
	if (!L)
		return 0;

	const std::uint64_t G = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Global);
	if (!g_Memory.IsValid(G))
		return 0;

	return G;
}

int SetGcByKey(lua_State* L);

bool IsGcOpt(const char* opt)
{
	static const char* const opts[] = {
		"stop", "restart", "count", "isrunning",
		"pause", "setpause", "stepmul", "setstepmul",
		"collect", "step",
	};

	for (const char* o : opts)
	{
		if (std::strcmp(opt, o) == 0)
			return true;
	}

	return false;
}

// setgc("stop") — GC, setgc("ShootCooldown", 0) — запись по ключу во всех таблицах
int l_setgc(lua_State* L)
{
	if (lua_istable(L, 1))
		return SetGcByKey(L);

	const char* opt = luaL_checkstring(L, 1);
	if (!opt)
		return 0;

	if (!IsGcOpt(opt))
		return SetGcByKey(L);

	const std::uint64_t G = GameGlobal();
	if (!G)
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	// stop
	if (std::strcmp(opt, "stop") == 0)
	{
		const std::int64_t m1 = -1;
		g_Memory.Write<std::int64_t>(G + Offsets::LuauGlobal::GCthreshold, m1);
		lua_pushboolean(L, 1);
		return 1;
	}

	// restart
	if (std::strcmp(opt, "restart") == 0)
	{
		const std::uint64_t tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
		g_Memory.Write<std::uint64_t>(G + Offsets::LuauGlobal::GCthreshold, tb);
		lua_pushboolean(L, 1);
		return 1;
	}

	// count -> KB как lua_gc COUNT
	if (std::strcmp(opt, "count") == 0)
	{
		const std::uint64_t tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
		lua_pushnumber(L, static_cast<lua_Number>(tb >> 10));
		return 1;
	}

	// isrunning
	if (std::strcmp(opt, "isrunning") == 0)
	{
		const std::int64_t thr = g_Memory.Read<std::int64_t>(G + Offsets::LuauGlobal::GCthreshold);
		lua_pushboolean(L, thr != -1);
		return 1;
	}

	// pause
	if (std::strcmp(opt, "pause") == 0 || std::strcmp(opt, "setpause") == 0)
	{
		const int old = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::gcpause);
		if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
		{
			int v = static_cast<int>(luaL_checkinteger(L, 2));
			if (v < 0) v = 0;
			if (v > 1000) v = 1000;
			g_Memory.Write<std::int32_t>(G + Offsets::LuauGlobal::gcpause, v);
		}

		lua_pushinteger(L, old);
		return 1;
	}

	// stepmul
	if (std::strcmp(opt, "stepmul") == 0 || std::strcmp(opt, "setstepmul") == 0)
	{
		const int old = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::gcstepmul);
		if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
		{
			int v = static_cast<int>(luaL_checkinteger(L, 2));
			if (v < 0) v = 0;
			if (v > 10000) v = 10000;
			g_Memory.Write<std::int32_t>(G + Offsets::LuauGlobal::gcstepmul, v);
		}

		lua_pushinteger(L, old);
		return 1;
	}

	// collect/step — только через lua_gc в процессе, снаружи нет
	if (std::strcmp(opt, "collect") == 0 || std::strcmp(opt, "step") == 0)
	{
		lua_pushnil(L);
		lua_pushstring(L, "collect/step need ingame call");
		return 2;
	}

	lua_pushnil(L);
	lua_pushstring(L, "unknown setgc opt");
	return 2;
}

const char* TtName(int tt)
{
	if (tt == 0) return "nil";
	if (tt == 1) return "bool";
	if (tt == 2) return "lightud";
	if (tt == 3) return "number";
	if (tt == 4) return "vector?";
	if (tt == 5) return "vector";
	if (tt == 6) return "string";
	if (tt == 7) return "table";
	if (tt == 8) return "fn?";
	if (tt == 9) return "userdata";
	if (tt == 10) return "thread";
	if (tt == 11) return "buffer";
	return "other";
}

std::uint64_t GcoNext(std::uint64_t obj, int tt)
{
	// gclist: table @+40, остальное чаще @+8 (propagatemark)
	if (tt == 7)
		return g_Memory.Read<std::uint64_t>(obj + 40);

	return g_Memory.Read<std::uint64_t>(obj + 8);
}

template <class F>
void WalkGcoChain(std::uint64_t head, F&& fn)
{
	std::uint64_t cur = head;
	for (int i = 0; i < k_gco_chain_max; ++i)
	{
		if (!cur || (cur & 0x7))
			break;

		const int tt = (int)g_Memory.Read<std::uint8_t>(cur + 1);
		if (tt <= 0 || tt > 15)
			break;

		if (!fn(cur, tt))
			break;

		const std::uint64_t nxt = GcoNext(cur, tt);
		if (nxt == cur)
			break;
		cur = nxt;
	}
}

int WalkGcoList(std::uint64_t head, int* by_tt, int by_n, int& total, std::uint64_t* samples, int* sample_tt, int& nsample, int sample_cap)
{
	int n = 0;
	WalkGcoChain(head, [&](std::uint64_t cur, int tt)
	{
		if (tt >= by_n)
			return false;

		++by_tt[tt];
		++total;
		++n;

		if (nsample < sample_cap)
		{
			samples[nsample] = cur;
			sample_tt[nsample] = tt;
			++nsample;
		}

		return true;
	});
	return n;
}

// luaH_dummynode в образе: пустая таблица показывает node сюда, кандидатом её не считаем
std::uint64_t EmptyNode()
{
	return (std::uint64_t)g_Memory.GetModuleBase() + Offsets::LuauGlobal::dummynode;
}

struct PageBuf
{
	std::vector<unsigned char> blocks;
	std::uint64_t base = 0;
	int block = 0;
	int count = 0;
};

bool ReadPage(std::uint64_t p, int min_block, PageBuf& out)
{
	unsigned char hdr[64];
	if (g_Memory.ReadRaw((uintptr_t)p, hdr, sizeof(hdr)) != sizeof(hdr))
		return false;

	std::int32_t page_size = 0;
	std::int32_t block_size = 0;
	std::memcpy(&page_size, hdr + 32, 4);
	std::memcpy(&block_size, hdr + 36, 4);

	if (block_size < min_block || block_size > 512)
		return false;
	if (page_size < 128 || page_size > 65536)
		return false;

	const int count = (page_size - 64) / block_size;
	if (count <= 0 || count > k_page_blocks_max)
		return false;

	const std::size_t bytes = (std::size_t)count * (std::size_t)block_size;
	out.blocks.resize(bytes);
	if (g_Memory.ReadRaw((uintptr_t)(p + 64), out.blocks.data(), bytes) != bytes)
		return false;

	out.base = p + 64;
	out.block = block_size;
	out.count = count;
	return true;
}

// после unlink next может смотреть назад — без visited цепь крутится до упора лимита
template <class F>
void WalkPageChain(std::uint64_t head, std::uint64_t sentinel, uintptr_t next_off, F&& fn)
{
	std::unordered_set<std::uint64_t> visited;
	std::uint64_t p = head;

	for (int i = 0; i < k_page_chain_max; ++i)
	{
		if (!p || p == sentinel || (p & 0x7))
			break;
		if (!visited.insert(p).second)
			break;
		if (!fn(p))
			break;

		p = g_Memory.Read<std::uint64_t>(p + (std::uint64_t)next_off);
	}
}

template <class F>
void WalkAllPages(std::uint64_t G, F&& fn)
{
	WalkPageChain(g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gcopages),
		G + Offsets::LuauGlobal::gcopages_end, Offsets::LuauGlobal::page_next_free, fn);

	// all pages — next @+8, тут и полные page после unlink с freelist
	WalkPageChain(g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gcopages_large),
		0, Offsets::LuauGlobal::page_next_all, fn);

	for (int sc = 0; sc < k_sizeclass_count; ++sc)
	{
		WalkPageChain(g_Memory.Read<std::uint64_t>(
			G + Offsets::LuauGlobal::gcopages_sizeclass + (std::uint64_t)sc * 8ull),
			0, Offsets::LuauGlobal::page_next_free, fn);
	}
}

bool BlockIsTable(const unsigned char* b, std::uint64_t empty_node)
{
	if (b[1] != 7)
		return false;

	// без hash — мусор с page walk, ammo всё равно с ключами
	const std::uint8_t lsz = b[3];
	if (lsz == 0 || lsz > 18)
		return false;

	std::int32_t sizearray = 0;
	std::memcpy(&sizearray, b + 8, 4);
	if (sizearray < 0 || sizearray > 1000000)
		return false;

	std::uint64_t node = 0;
	std::memcpy(&node, b + 24, 8);
	return node && (node & 0x7) == 0 && node != empty_node;
}

bool BlockIsStr(const unsigned char* b, int block, const char* s, std::size_t len)
{
	if (b[1] != 6)
		return false;

	std::uint32_t slen = 0;
	std::memcpy(&slen, b + 20, 4);
	if (slen != (std::uint32_t)len)
		return false;

	if ((std::size_t)block < 24 + len)
		return false;

	return std::memcmp(b + 24, s, len) == 0;
}

// один ReadRaw вместо пяти — заголовок Table целиком лежит в первых 40 байтах
bool ReadTableHdr(std::uint64_t o, std::uint8_t& lsz, std::uint64_t& node)
{
	if (!o || (o & 0x7))
		return false;

	unsigned char b[40];
	if (g_Memory.ReadRaw((uintptr_t)o, b, sizeof(b)) != sizeof(b))
		return false;

	if (!BlockIsTable(b, EmptyNode()))
		return false;

	lsz = b[3];
	std::memcpy(&node, b + 24, 8);
	return true;
}

bool LooksLikeTable(std::uint64_t o)
{
	std::uint8_t lsz = 0;
	std::uint64_t node = 0;
	return ReadTableHdr(o, lsz, node);
}

int CountPages(std::uint64_t G)
{
	int n = 0;
	WalkAllPages(G, [&](std::uint64_t)
	{
		++n;
		return true;
	});
	return n;
}

// getgc_info() — старый дамп stats
int l_getgc_info(lua_State* L)
{
	const std::uint64_t gameL = ResolveGameLuaState();
	const std::uint64_t G = GameGlobal();
	if (!G || !gameL)
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	int by_tt[16] = {};
	int total = 0;
	std::uint64_t samples[24] = {};
	int sample_tt[24] = {};
	int nsample = 0;

	const std::uint64_t gray = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gray);
	const std::uint64_t grayagain = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::grayagain);
	const std::uint64_t weak = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::weak);

	const int n_gray = WalkGcoList(gray, by_tt, 16, total, samples, sample_tt, nsample, 24);
	const int n_ga = WalkGcoList(grayagain, by_tt, 16, total, samples, sample_tt, nsample, 24);
	const int n_weak = WalkGcoList(weak, by_tt, 16, total, samples, sample_tt, nsample, 24);
	const int pages = CountPages(G);

	const std::uint64_t tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
	const std::int64_t thr = g_Memory.Read<std::int64_t>(G + Offsets::LuauGlobal::GCthreshold);
	const int st = (int)g_Memory.Read<std::uint8_t>(G + Offsets::LuauGlobal::gcstate);
	const int pause = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::gcpause);
	const int mul = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::gcstepmul);

	lua_createtable(L, 0, 16);

	lua_pushnumber(L, (lua_Number)(tb >> 10));
	lua_setfield(L, -2, "kb");

	lua_pushinteger(L, st);
	lua_setfield(L, -2, "gcstate");

	lua_pushboolean(L, thr != -1);
	lua_setfield(L, -2, "isrunning");

	lua_pushinteger(L, pause);
	lua_setfield(L, -2, "pause");

	lua_pushinteger(L, mul);
	lua_setfield(L, -2, "stepmul");

	lua_pushinteger(L, pages);
	lua_setfield(L, -2, "pages");

	lua_pushinteger(L, n_gray);
	lua_setfield(L, -2, "gray");

	lua_pushinteger(L, n_ga);
	lua_setfield(L, -2, "grayagain");

	lua_pushinteger(L, n_weak);
	lua_setfield(L, -2, "weak");

	lua_pushinteger(L, total);
	lua_setfield(L, -2, "total");

	char buf[32];
	std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)gameL);
	lua_pushstring(L, buf);
	lua_setfield(L, -2, "L");

	std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)G);
	lua_pushstring(L, buf);
	lua_setfield(L, -2, "G");

	lua_createtable(L, 0, 16);
	for (int t = 0; t < 16; ++t)
	{
		if (by_tt[t] <= 0)
			continue;
		lua_pushinteger(L, by_tt[t]);
		lua_setfield(L, -2, TtName(t));
	}
	lua_setfield(L, -2, "by_type");

	lua_createtable(L, nsample, 0);
	for (int i = 0; i < nsample; ++i)
	{
		lua_createtable(L, 0, 3);
		lua_pushinteger(L, sample_tt[i]);
		lua_setfield(L, -2, "tt");
		lua_pushstring(L, TtName(sample_tt[i]));
		lua_setfield(L, -2, "name");
		std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)samples[i]);
		lua_pushstring(L, buf);
		lua_setfield(L, -2, "addr");
		lua_rawseti(L, -2, i + 1);
	}
	lua_setfield(L, -2, "samples");

	return 1;
}

// ---- game table proxy (getgc true) ----

// findgc("_tag", "xxx") — не плодим 40k проксей
const char* g_fkey = nullptr;
const char* g_fval = nullptr;
// findgc(nil, "jewsploit_ammo_test") / режим value-only
bool g_fval_only = false;
// interned TString* из strt — быстрее чем memcmp каждого ключа
std::uint64_t g_fkey_ts = 0;
std::uint64_t g_fval_ts = 0;
// TString.hash @+16 (наш layout: next@+8 len@+20 data@+24)
unsigned int g_fkey_hash = 0;

// proxy write: tbl+key -> node, ключ хранится целиком, иначе коллизия PcId = запись не туда
struct PcEnt
{
	std::uint64_t node = 0;
	std::uint64_t ts = 0;
	std::string key;
};

std::unordered_map<std::uint64_t, PcEnt> g_pc;

unsigned int LuauStrHash(const char* str, std::size_t len);

std::uint64_t PcId(std::uint64_t tbl, const char* key, std::size_t klen)
{
	return tbl ^ ((std::uint64_t)LuauStrHash(key, klen) * 0x9E3779B97F4A7C15ull) ^ ((std::uint64_t)klen << 17);
}

bool TsEq(std::uint64_t ts, const char* key, std::size_t klen)
{
	if (!g_Memory.IsValid(ts) || (ts & 7))
		return false;

	if (g_Memory.Read<std::uint8_t>(ts + 1) != 6)
		return false;

	const std::uint32_t len = g_Memory.Read<std::uint32_t>(ts + 20);
	if (len != (std::uint32_t)klen || len > 512)
		return false;

	char buf[512];
	if (g_Memory.ReadRaw((uintptr_t)(ts + 24), buf, len) != len)
		return false;

	return std::memcmp(buf, key, klen) == 0;
}

// luau short hash (BytecodeBuilder::getStringHash / luaS_hash len<32)
unsigned int LuauStrHash(const char* str, std::size_t len)
{
	unsigned int h = (unsigned int)len;
	for (std::size_t i = len; i > 0; --i)
		h ^= (h << 5) + (h >> 2) + (unsigned char)str[i - 1];
	return h;
}

std::uint64_t StrtChainFind(
	std::uint64_t hash_base,
	int bucket,
	const char* needle,
	std::size_t nlen)
{
	std::uint64_t ts = g_Memory.Read<std::uint64_t>(
		hash_base + (std::uint64_t)bucket * 8ull);

	for (int k = 0; k < k_strt_chain_max && ts; ++k)
	{
		if (!g_Memory.IsValid(ts) || (ts & 7))
			break;

		if (TsEq(ts, needle, nlen))
			return ts;

		const std::uint64_t nxt = g_Memory.Read<std::uint64_t>(ts + 8);
		if (nxt == ts)
			break;
		ts = nxt;
	}

	return 0;
}

std::uint64_t FindStrtFirst(std::uint64_t G, const char* needle)
{
	if (!needle || !G)
		return 0;

	const int size = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::strt_size);
	const std::uint64_t hash = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::strt_hash);
	if (size <= 0 || size > (1 << 22) || !g_Memory.IsValid(hash))
		return 0;

	const std::size_t nlen = std::strlen(needle);

	// сначала один bucket — miss = полный скан (seed/longstr хз)
	if ((size & (size - 1)) == 0)
	{
		const int buck = (int)(LuauStrHash(needle, nlen) & (unsigned int)(size - 1));
		const std::uint64_t hit = StrtChainFind(hash, buck, needle, nlen);
		if (hit)
			return hit;
	}

	for (int i = 0; i < size; ++i)
	{
		const std::uint64_t hit = StrtChainFind(hash, i, needle, nlen);
		if (hit)
			return hit;
	}

	return 0;
}

struct StrtEnt
{
	std::uint64_t ts = 0;
	unsigned int hash = 0;
	std::chrono::steady_clock::time_point at{};
};

std::mutex g_strt_mx;
std::unordered_map<std::uint64_t, std::unordered_map<std::string, StrtEnt>> g_strt;

// промах FindStrtFirst = линейный скан всей strt, а lock-поток дёргает это раз в 2с
std::uint64_t FindStrtCached(std::uint64_t G, const char* needle, unsigned int* out_hash)
{
	if (!needle || !*needle || !G)
		return 0;

	const auto now = std::chrono::steady_clock::now();
	StrtEnt cached{};
	bool have = false;

	{
		std::lock_guard<std::mutex> g(g_strt_mx);
		auto vit = g_strt.find(G);
		if (vit != g_strt.end())
		{
			auto it = vit->second.find(needle);
			if (it != vit->second.end() && now - it->second.at < k_strt_ttl)
			{
				cached = it->second;
				have = true;
			}
		}
	}

	if (have)
	{
		if (!cached.ts || TsEq(cached.ts, needle, std::strlen(needle)))
		{
			if (out_hash)
				*out_hash = cached.hash;
			return cached.ts;
		}
	}

	const std::uint64_t ts = FindStrtFirst(G, needle);
	const unsigned int h = ts ? g_Memory.Read<unsigned int>(ts + 16) : 0;

	{
		std::lock_guard<std::mutex> g(g_strt_mx);
		if (g_strt.size() > 8)
			g_strt.clear();

		auto& m = g_strt[G];
		if (m.size() > 512)
			m.clear();

		StrtEnt e{};
		e.ts = ts;
		e.hash = h;
		e.at = now;
		m[needle] = e;
	}

	if (out_hash)
		*out_hash = h;
	return ts;
}

bool TableValStrEq(std::uint64_t val_node, const char* s, std::size_t len)
{
	const int tt = g_Memory.Read<std::int32_t>(val_node + 12) & 0xF;
	if (tt != 6)
		return false;

	const std::uint64_t ts = g_Memory.Read<std::uint64_t>(val_node);
	if (g_fval_ts)
		return ts == g_fval_ts;

	return TsEq(ts, s, len);
}

// любая string-value == needle (для ammo без точного _tag)
bool TableHasStrVal(std::uint64_t tbl, const char* needle, std::size_t nlen)
{
	std::uint8_t lsz = 0;
	std::uint64_t node = 0;
	if (!ReadTableHdr(tbl, lsz, node))
		return false;

	const int n = 1 << lsz;
	unsigned char buf[32 * 64];
	int off = 0;

	while (off < n)
	{
		int chunk = n - off;
		if (chunk > 64)
			chunk = 64;

		const std::size_t bytes = (std::size_t)chunk * 32ull;
		if (g_Memory.ReadRaw((uintptr_t)(node + (std::uint64_t)off * 32ull), buf, bytes) != bytes)
			return false;

		for (int i = 0; i < chunk; ++i)
		{
			const unsigned char* nd = buf + (std::size_t)i * 32ull;
			std::int32_t vtt = 0;
			std::memcpy(&vtt, nd + 12, 4);
			if ((vtt & 0xF) != 6)
				continue;

			std::uint64_t ts = 0;
			std::memcpy(&ts, nd, 8);

			if (g_fval_ts)
			{
				if (ts == g_fval_ts)
					return true;
				continue;
			}

			if (TsEq(ts, needle, nlen))
				return true;
		}

		off += chunk;
	}

	return false;
}

// node val @0, key ptr @16, key tt:4|next:28 @28; TValue tt @12
bool TableFindStr(std::uint64_t tbl, const char* key, std::uint64_t key_ts, unsigned int key_hash, std::uint64_t& val_node)
{
	std::uint8_t lsz = 0;
	std::uint64_t node = 0;
	if (!ReadTableHdr(tbl, lsz, node))
		return false;

	const int n = 1 << lsz;
	const std::size_t klen = key ? std::strlen(key) : 0;

	// luau mainposition + next chain — не жрать весь node[]
	if (key_ts)
	{
		unsigned int h = key_hash;
		if (!h)
			h = g_Memory.Read<unsigned int>(key_ts + 16);

		int i = (int)(h & (unsigned int)(n - 1));
		unsigned char nb[32];
		for (int hop = 0; hop < n && hop < 256; ++hop)
		{
			const std::uint64_t nd = node + (std::uint64_t)i * 32ull;
			if (g_Memory.ReadRaw((uintptr_t)nd, nb, 32) != 32)
				break;

			const std::uint32_t kw = (std::uint32_t)nb[28]
				| ((std::uint32_t)nb[29] << 8)
				| ((std::uint32_t)nb[30] << 16)
				| ((std::uint32_t)nb[31] << 24);
			const std::uint8_t ktt = (std::uint8_t)(kw & 0xF);
			if (ktt == 6)
			{
				std::uint64_t ts = 0;
				std::memcpy(&ts, nb + 16, 8);
				if (ts == key_ts)
				{
					val_node = nd;
					return true;
				}
			}

			const std::int32_t nx = (std::int32_t)kw >> 4;
			if (nx == 0)
				break;

			i += nx;
			if (i < 0 || i >= n)
				break;
		}
		// hash miss — ниже линейный (оффсет hash кривой / dead key)
	}

	// один ReadRaw на чанк — RPM/slot убивал findgc
	unsigned char buf[32 * 64];
	int off = 0;
	while (off < n)
	{
		int chunk = n - off;
		if (chunk > 64)
			chunk = 64;

		const std::size_t bytes = (std::size_t)chunk * 32ull;
		if (g_Memory.ReadRaw((uintptr_t)(node + (std::uint64_t)off * 32ull), buf, bytes) != bytes)
			return false;

		for (int i = 0; i < chunk; ++i)
		{
			const unsigned char* nd = buf + (std::size_t)i * 32ull;
			const std::uint8_t ktt = (std::uint8_t)(nd[28] & 0xF);
			if (ktt != 6)
				continue;

			std::uint64_t ts = 0;
			std::memcpy(&ts, nd + 16, 8);

			if (key_ts)
			{
				if (ts != key_ts)
					continue;
			}

			else if (!key || !TsEq(ts, key, klen))
			{
				continue;
			}

			val_node = node + (std::uint64_t)(off + i) * 32ull;
			return true;
		}

		off += chunk;
	}

	return false;
}

std::uint64_t ProxyAddr(lua_State* L, int idx)
{
	lua_getfield(L, LUA_REGISTRYINDEX, "jp_gtables");
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return 0;
	}

	lua_pushvalue(L, idx);
	lua_rawget(L, -2);
	const std::uint64_t a = (std::uint64_t)lua_tointeger(L, -1);
	lua_pop(L, 2);
	return a;
}

void PushGameTValue(lua_State* L, std::uint64_t tv);

void PushTableProxy(lua_State* L, std::uint64_t addr)
{
	if (!lua_checkstack(L, 5))
	{
		lua_pushnil(L);
		return;
	}

	lua_newtable(L);

	luaL_getmetatable(L, "jp.luau_table");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		return;
	}
	lua_setmetatable(L, -2);

	lua_getfield(L, LUA_REGISTRYINDEX, "jp_gtables");
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return;
	}
	lua_pushvalue(L, -2);
	lua_pushinteger(L, (lua_Integer)addr);
	lua_rawset(L, -3);
	lua_pop(L, 1);
}

void PushGameTValue(lua_State* L, std::uint64_t tv)
{
	const int tt = g_Memory.Read<std::int32_t>(tv + 12);

	if (tt == 0)
	{
		lua_pushnil(L);
		return;
	}

	if (tt == 1)
	{
		lua_pushboolean(L, g_Memory.Read<std::int32_t>(tv) != 0);
		return;
	}

	if (tt == 3)
	{
		lua_pushnumber(L, g_Memory.Read<double>(tv));
		return;
	}

	if (tt == 6)
	{
		const std::uint64_t ts = g_Memory.Read<std::uint64_t>(tv);
		if (!g_Memory.IsValid(ts))
		{
			lua_pushnil(L);
			return;
		}

		const std::uint32_t len = g_Memory.Read<std::uint32_t>(ts + 20);
		if (len > 0x10000)
		{
			lua_pushnil(L);
			return;
		}

		if (len == 0)
		{
			lua_pushlstring(L, "", 0);
			return;
		}

		char stack[256];
		char* buf = stack;
		std::string heap;
		if (len > sizeof(stack))
		{
			heap.resize(len);
			buf = heap.data();
		}

		if (g_Memory.ReadRaw((uintptr_t)(ts + 24), buf, len) != len)
		{
			lua_pushnil(L);
			return;
		}

		lua_pushlstring(L, buf, len);
		return;
	}

	if (tt == 7)
	{
		const std::uint64_t t = g_Memory.Read<std::uint64_t>(tv);
		if (LooksLikeTable(t))
			PushTableProxy(L, t);

		else
			lua_pushnil(L);

		return;
	}

	lua_pushnil(L);
}

struct GcValue
{
	int tt = 0; // 1 bool, 3 number
	double num = 0.0;
	bool b = false;
};

bool ReadGcValue(lua_State* L, int idx, GcValue& out)
{
	if (lua_type(L, idx) == LUA_TNUMBER)
	{
		out.tt = 3;
		out.num = lua_tonumber(L, idx);
		return true;
	}

	if (lua_type(L, idx) == LUA_TBOOLEAN)
	{
		out.tt = 1;
		out.b = lua_toboolean(L, idx) != 0;
		return true;
	}

	return false;
}

void WriteGcNode(std::uint64_t node, const GcValue& v)
{
	if (v.tt == 3)
	{
		g_Memory.Write<double>(node + 0, v.num);
		g_Memory.Write<std::int32_t>(node + 12, 3);
		return;
	}

	if (v.tt == 1)
	{
		g_Memory.Write<std::int32_t>(node + 0, v.b ? 1 : 0);
		g_Memory.Write<std::int32_t>(node + 12, 1);
	}
}

// узел мог уехать после rehash — ключ обязан совпадать перед записью, want_tt < 0 = любой
bool NodeStillHasKey(std::uint64_t node, std::uint64_t ts, int want_tt)
{
	if (!node || (node & 0x7))
		return false;

	unsigned char nb[32];
	if (g_Memory.ReadRaw((uintptr_t)node, nb, 32) != 32)
		return false;

	if ((std::uint8_t)(nb[28] & 0xF) != 6)
		return false;

	std::uint64_t cur = 0;
	std::memcpy(&cur, nb + 16, 8);
	if (cur != ts)
		return false;

	if (want_tt < 0)
		return true;

	std::int32_t vtt = 0;
	std::memcpy(&vtt, nb + 12, 4);
	return (vtt & 0xF) == want_tt;
}

int proxy_index(lua_State* L)
{
	const std::uint64_t tbl = ProxyAddr(L, 1);
	if (!tbl)
	{
		lua_pushnil(L);
		return 1;
	}

	const char* key = lua_tostring(L, 2);
	if (!key)
	{
		lua_pushnil(L);
		return 1;
	}

	std::uint64_t nd = 0;
	if (!TableFindStr(tbl, key, 0, 0, nd))
	{
		lua_pushnil(L);
		return 1;
	}

	PushGameTValue(L, nd);
	return 1;
}

int proxy_newindex(lua_State* L)
{
	const std::uint64_t tbl = ProxyAddr(L, 1);
	if (!tbl)
		return 0;

	const char* key = lua_tostring(L, 2);
	if (!key)
		return 0;

	// пока number / bool, хватит для ammo
	GcValue v{};
	if (!ReadGcValue(L, 3, v))
		return 0;

	const std::size_t klen = std::strlen(key);
	const std::uint64_t id = PcId(tbl, key, klen);

	auto it = g_pc.find(id);
	if (it != g_pc.end() && it->second.key == key &&
		NodeStillHasKey(it->second.node, it->second.ts, -1))
	{
		WriteGcNode(it->second.node, v);
		return 0;
	}

	std::uint64_t nd = 0;
	if (!TableFindStr(tbl, key, 0, 0, nd))
		return 0;

	if (g_pc.size() > 4096)
		g_pc.clear();

	PcEnt e{};
	e.node = nd;
	e.ts = g_Memory.Read<std::uint64_t>(nd + 16);
	e.key = key;
	g_pc[id] = std::move(e);

	WriteGcNode(nd, v);
	return 0;
}

int l_rawget_proxy(lua_State* L)
{
	luaL_checkany(L, 1);
	luaL_checkany(L, 2);

	if (lua_istable(L, 1) && ProxyAddr(L, 1) != 0 && lua_type(L, 2) == LUA_TSTRING)
	{
		const std::uint64_t tbl = ProxyAddr(L, 1);
		const char* key = lua_tostring(L, 2);
		std::uint64_t nd = 0;
		if (key && TableFindStr(tbl, key, 0, 0, nd))
		{
			PushGameTValue(L, nd);
			return 1;
		}

		lua_pushnil(L);
		return 1;
	}

	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_call(L, 2, 1);
	return 1;
}

int getgc_iter(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	const lua_Integer i = luaL_checkinteger(L, 2) + 1;
	lua_rawgeti(L, 1, i);
	if (lua_isnil(L, -1))
		return 0;

	lua_pushinteger(L, i);
	lua_insert(L, -2);
	return 2;
}

// ---- heap snapshot ----

struct Snapshot
{
	std::uint64_t G = 0;
	std::uint64_t tb = 0;
	std::vector<std::uint64_t> tables;
	bool truncated = false;
	std::chrono::steady_clock::time_point at{};
};

std::mutex g_snap_mx;
std::mutex g_walk_mx;
std::unordered_map<std::uint64_t, std::shared_ptr<const Snapshot>> g_snaps;

std::shared_ptr<const Snapshot> BuildSnapshot(std::uint64_t G)
{
	auto s = std::make_shared<Snapshot>();
	s->G = G;
	s->tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);

	std::unordered_set<std::uint64_t> seen;
	seen.reserve(1u << 16);

	auto add = [&](std::uint64_t o)
	{
		if (s->tables.size() >= k_snapshot_max)
		{
			s->truncated = true;
			return false;
		}

		if (seen.insert(o).second)
			s->tables.push_back(o);
		return true;
	};

	const uintptr_t lists[3] = {
		Offsets::LuauGlobal::gray,
		Offsets::LuauGlobal::grayagain,
		Offsets::LuauGlobal::weak,
	};

	for (uintptr_t off : lists)
	{
		if (s->truncated)
			break;

		WalkGcoChain(g_Memory.Read<std::uint64_t>(G + off), [&](std::uint64_t cur, int tt)
		{
			if (tt != 7)
				return true;
			return add(cur);
		});
	}

	const std::uint64_t empty = EmptyNode();
	PageBuf pb;

	WalkAllPages(G, [&](std::uint64_t p)
	{
		if (s->truncated)
			return false;
		if (!ReadPage(p, 40, pb))
			return true;

		for (int i = 0; i < pb.count; ++i)
		{
			const unsigned char* b = pb.blocks.data() + (std::size_t)i * (std::size_t)pb.block;
			if (!BlockIsTable(b, empty))
				continue;

			if (!add(pb.base + (std::uint64_t)i * (std::uint64_t)pb.block))
				return false;
		}

		return true;
	});

	s->at = std::chrono::steady_clock::now();
	return s;
}

bool SnapshotFresh(const Snapshot& s)
{
	if (std::chrono::steady_clock::now() - s.at > k_snapshot_ttl)
		return false;

	// куча заметно двинулась — старые адреса уже не описывают её состав
	const std::uint64_t tb = g_Memory.Read<std::uint64_t>(s.G + Offsets::LuauGlobal::totalbytes);
	if (!tb || !s.tb)
		return false;

	const std::uint64_t d = (tb > s.tb) ? (tb - s.tb) : (s.tb - tb);
	return d <= (s.tb >> 3);
}

std::shared_ptr<const Snapshot> CachedSnapshot(std::uint64_t G)
{
	std::lock_guard<std::mutex> g(g_snap_mx);
	auto it = g_snaps.find(G);
	if (it == g_snaps.end())
		return nullptr;
	return it->second;
}

std::shared_ptr<const Snapshot> GetSnapshot(std::uint64_t G)
{
	auto cur = CachedSnapshot(G);
	if (cur && SnapshotFresh(*cur))
		return cur;

	// один тяжёлый проход за раз: скрипт и lock-поток не должны дублировать RPM
	std::lock_guard<std::mutex> w(g_walk_mx);

	cur = CachedSnapshot(G);
	if (cur && SnapshotFresh(*cur))
		return cur;

	auto s = BuildSnapshot(G);
	{
		std::lock_guard<std::mutex> g(g_snap_mx);
		if (g_snaps.size() > 8)
			g_snaps.clear();
		g_snaps[G] = s;
	}
	return s;
}

void FlushCaches()
{
	{
		std::lock_guard<std::mutex> g(g_snap_mx);
		g_snaps.clear();
	}

	std::lock_guard<std::mutex> g(g_strt_mx);
	g_strt.clear();
}

// ---- paged collection ----

struct Collect
{
	std::int64_t offset = 0;
	std::int64_t limit = k_result_default;
	std::int64_t matched = 0;
	int n = 0;
	bool truncated = false;
};

void ReadRange(lua_State* L, int idx, Collect& c)
{
	if (lua_type(L, idx) == LUA_TTABLE)
	{
		lua_getfield(L, idx, "offset");
		if (lua_isnumber(L, -1))
			c.offset = (std::int64_t)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, idx, "limit");
		if (lua_isnumber(L, -1))
			c.limit = (std::int64_t)lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	else if (lua_type(L, idx) == LUA_TNUMBER)
	{
		c.limit = (std::int64_t)lua_tonumber(L, idx);
		if (lua_type(L, idx + 1) == LUA_TNUMBER)
			c.offset = (std::int64_t)lua_tonumber(L, idx + 1);
	}

	if (c.offset < 0)
		c.offset = 0;
	if (c.limit <= 0 || c.limit > k_result_max)
		c.limit = k_result_max;
}

void PushRangeInfo(lua_State* L, const Collect& c)
{
	lua_pushinteger(L, c.n);
	lua_setfield(L, -2, "count");
	lua_pushinteger(L, (lua_Integer)c.offset);
	lua_setfield(L, -2, "offset");
	lua_pushinteger(L, (lua_Integer)c.limit);
	lua_setfield(L, -2, "limit");
	lua_pushboolean(L, c.truncated ? 1 : 0);
	lua_setfield(L, -2, "truncated");
	lua_pushinteger(L, (lua_Integer)(c.truncated ? c.offset + c.n : 0));
	lua_setfield(L, -2, "next_offset");
}

bool MatchTable(std::uint64_t obj)
{
	if (g_fval_only && g_fval)
		return TableHasStrVal(obj, g_fval, std::strlen(g_fval));

	if (g_fkey)
	{
		std::uint64_t nd = 0;
		if (!TableFindStr(obj, g_fkey, g_fkey_ts, g_fkey_hash, nd))
			return false;

		if (g_fval)
			return TableValStrEq(nd, g_fval, std::strlen(g_fval));

		return true;
	}

	return LooksLikeTable(obj);
}

void CollectFromSnapshot(lua_State* L, const Snapshot& s, Collect& c, std::int64_t stop_at)
{
	for (std::uint64_t obj : s.tables)
	{
		if (c.n >= stop_at)
		{
			c.truncated = true;
			return;
		}

		// адрес из кэша, объект мог освободиться — Match перечитывает заголовок
		if (!MatchTable(obj))
			continue;

		++c.matched;
		if (c.matched <= c.offset)
			continue;

		PushTableProxy(L, obj);
		lua_rawseti(L, -2, ++c.n);
	}

	if (s.truncated)
		c.truncated = true;
}

// getgc(true [, {offset=,limit=}]) -> for _,v in getgc(true) do  (iter, arr, 0)
// getgc() / false -> getgc_info stats
int l_getgc(lua_State* L)
{
	const int top = lua_gettop(L);
	const bool want_tables = (top >= 1) && lua_toboolean(L, 1);

	if (!want_tables)
		return l_getgc_info(L);

	std::vector<GameVm> vms;
	CollectGameVms(vms);
	if (vms.empty())
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	Collect c{};
	ReadRange(L, 2, c);

	g_fkey = nullptr;
	g_fval = nullptr;
	g_fval_only = false;
	g_fkey_ts = 0;
	g_fval_ts = 0;
	g_fkey_hash = 0;

	lua_createtable(L, (int)std::min<std::int64_t>(c.limit, 1024), 8);

	for (const auto& vm : vms)
	{
		CollectFromSnapshot(L, *GetSnapshot(vm.G), c, c.limit);
		if (c.n >= c.limit)
			break;
	}

	PushRangeInfo(L, c);

	lua_pushcfunction(L, getgc_iter);
	lua_insert(L, -2);
	lua_pushinteger(L, 0);
	return 3;
}

// findgc({"FireRate","RPM",...}) — один walk, map key -> {proxies}
// иначе findgc(key) / findgc(key,val) / findgc(nil,val)
int l_findgc_keys(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	struct KEnt
	{
		const char* s;
		std::size_t len;
		std::uint64_t ts;
		int n; // hits
	};

	std::vector<KEnt> keys;
	keys.reserve(64);
	const int narg = (int)lua_rawlen(L, 1);
	for (int i = 1; i <= narg && (int)keys.size() < 64; ++i)
	{
		lua_rawgeti(L, 1, i);
		if (lua_type(L, -1) == LUA_TSTRING)
		{
			KEnt e{};
			e.s = lua_tostring(L, -1);
			e.len = e.s ? std::strlen(e.s) : 0;
			e.ts = 0;
			e.n = 0;
			if (e.s && e.len)
				keys.push_back(e);
		}
		lua_pop(L, 1);
	}

	if (keys.empty())
	{
		lua_createtable(L, 0, 0);
		lua_pushboolean(L, 0);
		return 2;
	}

	Collect c{};
	c.limit = k_keys_per_key_default;
	ReadRange(L, 2, c);
	const int per_key = (int)c.limit;

	std::vector<GameVm> vms;
	CollectGameVms(vms);
	if (vms.empty())
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	std::sort(vms.begin(), vms.end(), [](const GameVm& a, const GameVm& b)
	{
		return a.tb < b.tb;
	});

	// out[key] = array
	lua_createtable(L, 0, (int)keys.size());
	const int out = lua_gettop(L);
	for (std::size_t ki = 0; ki < keys.size(); ++ki)
	{
		lua_createtable(L, 8, 0);
		lua_setfield(L, out, keys[ki].s);
	}

	bool truncated = false;

	// одна таблица = один скан node[], все ключи сразу
	auto try_one = [&](std::uint64_t obj)
	{
		int left = 0;
		for (std::size_t ki = 0; ki < keys.size(); ++ki)
		{
			if (keys[ki].ts && keys[ki].n < per_key)
				++left;

			else if (keys[ki].ts)
				truncated = true;
		}

		if (!left)
			return;

		std::uint8_t lsz = 0;
		std::uint64_t node = 0;
		if (!ReadTableHdr(obj, lsz, node))
			return;

		if (!lua_checkstack(L, 8))
			return;

		const int nn = 1 << lsz;
		unsigned char got[64];
		std::memset(got, 0, sizeof(got));
		int hit_here = 0;

		unsigned char buf[32 * 64];
		int off = 0;
		while (off < nn && hit_here < left)
		{
			int chunk = nn - off;
			if (chunk > 64)
				chunk = 64;

			const std::size_t bytes = (std::size_t)chunk * 32ull;
			if (g_Memory.ReadRaw((uintptr_t)(node + (std::uint64_t)off * 32ull), buf, bytes) != bytes)
				return;

			for (int i = 0; i < chunk; ++i)
			{
				const unsigned char* nd = buf + (std::size_t)i * 32ull;
				if ((std::uint8_t)(nd[28] & 0xF) != 6)
					continue;

				std::uint64_t ts = 0;
				std::memcpy(&ts, nd + 16, 8);

				for (std::size_t ki = 0; ki < keys.size(); ++ki)
				{
					KEnt& e = keys[ki];
					if (!e.ts || e.n >= per_key || got[ki])
						continue;

					if (ts != e.ts)
						continue;

					got[ki] = 1;
					++hit_here;
					lua_getfield(L, out, e.s);
					PushTableProxy(L, obj);
					lua_rawseti(L, -2, ++e.n);
					lua_pop(L, 1);
				}
			}

			off += chunk;
		}
	};

	for (const auto& vm : vms)
	{
		int alive = 0;
		for (std::size_t ki = 0; ki < keys.size(); ++ki)
		{
			keys[ki].ts = FindStrtCached(vm.G, keys[ki].s, nullptr);
			if (keys[ki].ts)
				++alive;
		}

		if (!alive)
			continue;

		auto snap = GetSnapshot(vm.G);
		if (snap->truncated)
			truncated = true;

		for (std::uint64_t obj : snap->tables)
			try_one(obj);
	}

	lua_pushboolean(L, truncated ? 1 : 0);
	return 2;
}

int l_findgc(lua_State* L)
{
	if (lua_istable(L, 1))
		return l_findgc_keys(L);

	const char* key = nullptr;
	const char* val = nullptr;
	bool val_only = false;

	if (lua_type(L, 1) == LUA_TSTRING)
		key = lua_tostring(L, 1);

	else if (lua_isnoneornil(L, 1) && lua_type(L, 2) == LUA_TSTRING)
	{
		val = lua_tostring(L, 2);
		val_only = true;
	}

	else
		return luaL_error(L, "findgc(key|keys) or findgc(key,val) or findgc(nil,val)");

	if (!val_only && lua_gettop(L) >= 2 && lua_type(L, 2) == LUA_TSTRING)
		val = lua_tostring(L, 2);

	Collect c{};
	ReadRange(L, (lua_type(L, 2) == LUA_TTABLE) ? 2 : 3, c);

	std::vector<GameVm> vms;
	CollectGameVms(vms);
	if (vms.empty())
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	// value/точное имя — большой VM первым (локальная пушка там)
	// иначе мелкий первым — ammo ~1MB, roact ~90MB
	if (val || val_only)
	{
		std::sort(vms.begin(), vms.end(), [](const GameVm& a, const GameVm& b)
		{
			return a.tb > b.tb;
		});
	}

	else
	{
		std::sort(vms.begin(), vms.end(), [](const GameVm& a, const GameVm& b)
		{
			return a.tb < b.tb;
		});
	}

	g_fkey = key;
	g_fval = val;
	g_fval_only = val_only;
	g_fkey_hash = 0;
	lua_createtable(L, (int)std::min<std::int64_t>(c.limit, 1024), 8);

	// бюджет на VM — мелкий не сожрёт всю страницу до большого
	const std::int64_t per_vm = (vms.size() > 1) ? (c.limit / 2 + 1) : c.limit;

	for (const auto& vm : vms)
	{
		g_fkey_ts = 0;
		g_fval_ts = 0;
		g_fkey_hash = 0;

		if (key)
		{
			g_fkey_ts = FindStrtCached(vm.G, key, &g_fkey_hash);
			if (!g_fkey_ts)
				continue;
		}

		if (val)
		{
			g_fval_ts = FindStrtCached(vm.G, val, nullptr);
			if (!g_fval_ts)
				continue;
		}

		std::int64_t stop_at = c.n + per_vm;
		if (stop_at > c.limit)
			stop_at = c.limit;

		CollectFromSnapshot(L, *GetSnapshot(vm.G), c, stop_at);
		if (c.n >= c.limit)
			break;
	}

	g_fkey = nullptr;
	g_fval = nullptr;
	g_fval_only = false;
	g_fkey_ts = 0;
	g_fval_ts = 0;
	g_fkey_hash = 0;

	PushRangeInfo(L, c);

	lua_pushcfunction(L, getgc_iter);
	lua_insert(L, -2);
	lua_pushinteger(L, 0);
	return 3;
}

int CountStrtNeedle(std::uint64_t G, const char* needle)
{
	const int size = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::strt_size);
	const std::uint64_t hash = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::strt_hash);
	if (size <= 0 || size > (1 << 22) || !g_Memory.IsValid(hash))
		return -1;

	const std::size_t nlen = std::strlen(needle);
	int hits = 0;

	for (int i = 0; i < size; ++i)
	{
		std::uint64_t ts = g_Memory.Read<std::uint64_t>(hash + (std::uint64_t)i * 8ull);
		for (int k = 0; k < k_strt_chain_max && ts; ++k)
		{
			if (!g_Memory.IsValid(ts) || (ts & 7))
				break;

			if (TsEq(ts, needle, nlen))
				++hits;

			const std::uint64_t nxt = g_Memory.Read<std::uint64_t>(ts + 8);
			if (nxt == ts)
				break;
			ts = nxt;
		}
	}

	return hits;
}

int CountPageStrings(std::uint64_t G, const char* needle, int lim)
{
	const std::size_t nlen = std::strlen(needle);
	int hits = 0;
	PageBuf pb;

	WalkAllPages(G, [&](std::uint64_t p)
	{
		if (hits >= lim)
			return false;
		if (!ReadPage(p, 24, pb))
			return true;

		for (int i = 0; i < pb.count && hits < lim; ++i)
		{
			const unsigned char* b = pb.blocks.data() + (std::size_t)i * (std::size_t)pb.block;
			if (BlockIsStr(b, pb.block, needle, nlen))
				++hits;
		}

		return hits < lim;
	});

	return hits;
}

// gcprobe() — все VM + где ammo строка
int l_gcprobe(lua_State* L)
{
	std::vector<GameVm> vms;
	CollectGameVms(vms);
	if (vms.empty())
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	lua_createtable(L, 0, 8);
	lua_pushinteger(L, (lua_Integer)vms.size());
	lua_setfield(L, -2, "vm_count");

	lua_createtable(L, (int)vms.size(), 0);
	int ammo_vm = -1;

	for (int vi = 0; vi < (int)vms.size(); ++vi)
	{
		const auto& vm = vms[vi];
		const std::uint64_t G = vm.G;

		std::int64_t n_hash = 0;
		std::int64_t n_strkeys = 0;
		std::int64_t n_tag_key = 0;
		std::int64_t n_ammo_val = 0;

		lua_createtable(L, 0, 16);
		lua_createtable(L, 20, 0);
		const int samples = lua_gettop(L);

		auto snap = GetSnapshot(G);
		const std::uint64_t ammo_ts = FindStrtCached(G, "jewsploit_ammo_test", nullptr);

		unsigned char buf[32 * 64];
		for (std::uint64_t obj : snap->tables)
		{
			std::uint8_t lsz = 0;
			std::uint64_t node = 0;
			if (!ReadTableHdr(obj, lsz, node))
				continue;

			++n_hash;

			const int nn = 1 << lsz;
			int off = 0;
			while (off < nn)
			{
				int chunk = nn - off;
				if (chunk > 64)
					chunk = 64;

				const std::size_t bytes = (std::size_t)chunk * 32ull;
				if (g_Memory.ReadRaw((uintptr_t)(node + (std::uint64_t)off * 32ull), buf, bytes) != bytes)
					break;

				for (int i = 0; i < chunk; ++i)
				{
					const unsigned char* nd = buf + (std::size_t)i * 32ull;
					if ((std::uint8_t)(nd[28] & 0xF) != 6)
						continue;

					std::uint64_t ts = 0;
					std::memcpy(&ts, nd + 16, 8);
					if (!ts || (ts & 7))
						continue;

					char sbuf[96]{};
					if (g_Memory.Read<std::uint8_t>(ts + 1) != 6)
						continue;

					const std::uint32_t len = g_Memory.Read<std::uint32_t>(ts + 20);
					if (len == 0 || len >= sizeof(sbuf))
						continue;

					if (g_Memory.ReadRaw((uintptr_t)(ts + 24), sbuf, len) != len)
						continue;

					++n_strkeys;
					if (len == 4 && std::memcmp(sbuf, "_tag", 4) == 0)
						++n_tag_key;

					std::int32_t vtt = 0;
					std::memcpy(&vtt, nd + 12, 4);
					if (vtt == 6 && ammo_ts)
					{
						std::uint64_t vs = 0;
						std::memcpy(&vs, nd, 8);
						if (vs == ammo_ts)
							++n_ammo_val;
					}

					if (n_strkeys <= 15)
					{
						lua_pushlstring(L, sbuf, len);
						lua_rawseti(L, samples, (int)n_strkeys);
					}
				}

				off += chunk;
			}
		}

		lua_setfield(L, -2, "sample_keys");

		const int st_ammo = CountStrtNeedle(G, "jewsploit_ammo_test");
		const int pg_ammo = CountPageStrings(G, "jewsploit_ammo_test", 50);
		const int st_tag = CountStrtNeedle(G, "_tag");

		if (st_ammo > 0 || pg_ammo > 0 || n_tag_key > 0 || n_ammo_val > 0)
			ammo_vm = vi;

		lua_pushinteger(L, (lua_Integer)vm.wrap);
		lua_setfield(L, -2, "wrap");
		lua_pushinteger(L, (lua_Integer)(vm.tb >> 10));
		lua_setfield(L, -2, "kb");
		lua_pushinteger(L, (lua_Integer)n_hash);
		lua_setfield(L, -2, "hash_tables");
		lua_pushinteger(L, (lua_Integer)n_strkeys);
		lua_setfield(L, -2, "str_keys");
		lua_pushinteger(L, (lua_Integer)n_tag_key);
		lua_setfield(L, -2, "tag_keys");
		lua_pushinteger(L, (lua_Integer)n_ammo_val);
		lua_setfield(L, -2, "ammo_as_val");
		lua_pushinteger(L, st_tag);
		lua_setfield(L, -2, "strt_tag");
		lua_pushinteger(L, st_ammo);
		lua_setfield(L, -2, "strt_ammo");
		lua_pushinteger(L, pg_ammo);
		lua_setfield(L, -2, "page_ammo");
		lua_pushboolean(L, snap->truncated ? 1 : 0);
		lua_setfield(L, -2, "truncated");

		char buf2[32];
		std::snprintf(buf2, sizeof(buf2), "0x%llX", (unsigned long long)G);
		lua_pushstring(L, buf2);
		lua_setfield(L, -2, "G");
		std::snprintf(buf2, sizeof(buf2), "0x%llX", (unsigned long long)vm.L);
		lua_pushstring(L, buf2);
		lua_setfield(L, -2, "L");

		lua_rawseti(L, -2, vi + 1);
	}

	lua_setfield(L, -2, "vms");
	lua_pushinteger(L, ammo_vm + 1); // 1-based or 0 if none
	lua_setfield(L, -2, "ammo_vm");

	return 1;
}

// getrawkeys(proxy) -> { key = value, ... } только string keys
int l_getrawkeys(lua_State* L)
{
	const std::uint64_t tbl = ProxyAddr(L, 1);
	std::uint8_t lsz = 0;
	std::uint64_t node = 0;
	if (!tbl || !ReadTableHdr(tbl, lsz, node))
	{
		lua_createtable(L, 0, 0);
		return 1;
	}

	const int n = 1 << lsz;
	lua_createtable(L, 0, n > 128 ? 128 : n);

	unsigned char buf[32 * 64];
	int off = 0;
	while (off < n)
	{
		int chunk = n - off;
		if (chunk > 64)
			chunk = 64;

		const std::size_t bytes = (std::size_t)chunk * 32ull;
		if (g_Memory.ReadRaw((uintptr_t)(node + (std::uint64_t)off * 32ull), buf, bytes) != bytes)
			break;

		if (!lua_checkstack(L, 8))
			break;

		for (int i = 0; i < chunk; ++i)
		{
			const unsigned char* nd = buf + (std::size_t)i * 32ull;
			if ((std::uint8_t)(nd[28] & 0xF) != 6)
				continue;

			std::int32_t vtt = 0;
			std::memcpy(&vtt, nd + 12, 4);
			if ((vtt & 0xF) == 0)
				continue;

			std::uint64_t ts = 0;
			std::memcpy(&ts, nd + 16, 8);
			if (!ts || (ts & 7) || g_Memory.Read<std::uint8_t>(ts + 1) != 6)
				continue;

			const std::uint32_t len = g_Memory.Read<std::uint32_t>(ts + 20);
			if (len == 0 || len > 128)
				continue;

			char kbuf[128];
			if (g_Memory.ReadRaw((uintptr_t)(ts + 24), kbuf, len) != len)
				continue;

			lua_pushlstring(L, kbuf, len);
			PushGameTValue(L, node + (std::uint64_t)(off + i) * 32ull);
			lua_rawset(L, -3);
		}

		off += chunk;
	}

	return 1;
}

struct KeyHit
{
	std::uint64_t tbl = 0;
	std::uint64_t node = 0;
	std::uint64_t ts = 0;
};

// все узлы с ключом из keys, текущий тип значения == want_tt
void ScanKeyNodes(
	const std::vector<std::string>& keys,
	int want_tt,
	std::vector<KeyHit>& out,
	int cap)
{
	out.clear();

	std::vector<GameVm> vms;
	CollectGameVms(vms);
	if (vms.empty())
		return;

	std::sort(vms.begin(), vms.end(), [](const GameVm& a, const GameVm& b)
	{
		return a.tb < b.tb;
	});

	std::vector<std::uint64_t> ks(keys.size(), 0);

	auto try_tbl = [&](std::uint64_t obj)
	{
		std::uint8_t lsz = 0;
		std::uint64_t node = 0;
		if (!ReadTableHdr(obj, lsz, node))
			return;

		const int nn = 1 << lsz;
		unsigned char buf[32 * 64];
		int off = 0;

		while (off < nn && (int)out.size() < cap)
		{
			int chunk = nn - off;
			if (chunk > 64)
				chunk = 64;

			const std::size_t bytes = (std::size_t)chunk * 32ull;
			if (g_Memory.ReadRaw((uintptr_t)(node + (std::uint64_t)off * 32ull), buf, bytes) != bytes)
				return;

			for (int i = 0; i < chunk; ++i)
			{
				const unsigned char* nd = buf + (std::size_t)i * 32ull;
				if ((std::uint8_t)(nd[28] & 0xF) != 6)
					continue;

				std::uint64_t ts = 0;
				std::memcpy(&ts, nd + 16, 8);

				bool match = false;
				for (std::size_t ki = 0; ki < ks.size(); ++ki)
				{
					if (ks[ki] && ts == ks[ki])
					{
						match = true;
						break;
					}
				}

				if (!match)
					continue;

				std::int32_t vtt = 0;
				std::memcpy(&vtt, nd + 12, 4);
				if ((vtt & 0xF) != want_tt)
					continue;

				KeyHit h{};
				h.tbl = obj;
				h.node = node + (std::uint64_t)(off + i) * 32ull;
				h.ts = ts;
				out.push_back(h);
			}

			off += chunk;
		}
	};

	for (const auto& vm : vms)
	{
		int alive = 0;
		for (std::size_t ki = 0; ki < keys.size(); ++ki)
		{
			ks[ki] = FindStrtCached(vm.G, keys[ki].c_str(), nullptr);
			if (ks[ki])
				++alive;
		}

		if (!alive)
			continue;

		auto snap = GetSnapshot(vm.G);
		for (std::uint64_t obj : snap->tables)
		{
			if ((int)out.size() >= cap)
				break;
			try_tbl(obj);
		}

		if ((int)out.size() >= cap)
			break;
	}
}

struct GcLock
{
	std::mutex mx;
	std::vector<std::string> keys;
	std::string tag;
	GcValue val;
	std::vector<KeyHit> hits;
	std::chrono::steady_clock::time_point last_scan{};
};

std::mutex g_lock_mx;
std::mutex g_thr_mx;
std::mutex g_cv_mx;
std::condition_variable g_lock_cv;
std::vector<std::shared_ptr<GcLock>> g_locks;
std::thread g_lock_thr;
std::atomic<bool> g_lock_run{false};
std::atomic<int> g_lock_ms{100};

void LockWorker()
{
	while (g_lock_run.load())
	{
		std::vector<std::shared_ptr<GcLock>> snap;
		{
			std::lock_guard<std::mutex> g(g_lock_mx);
			snap = g_locks;
		}

		for (const auto& lk : snap)
		{
			if (!g_lock_run.load())
				break;

			std::lock_guard<std::mutex> lg(lk->mx);

			int alive = 0;
			for (const auto& h : lk->hits)
			{
				if (!NodeStillHasKey(h.node, h.ts, lk->val.tt))
					continue;

				WriteGcNode(h.node, lk->val);
				++alive;
			}

			// таблицы пересобрались (респавн, смена оружия) — ищем заново,
			// но скан тяжёлый, поэтому не чаще раза в 2 секунды
			const auto now = std::chrono::steady_clock::now();
			if (alive == 0 && now - lk->last_scan >= std::chrono::seconds(2))
			{
				lk->last_scan = now;
				ScanKeyNodes(lk->keys, lk->val.tt, lk->hits, k_hits_max);
			}
		}

		int ms = g_lock_ms.load();
		if (ms < 10)
			ms = 10;

		std::unique_lock<std::mutex> ul(g_cv_mx);
		g_lock_cv.wait_for(ul, std::chrono::milliseconds(ms), []
		{
			return !g_lock_run.load();
		});
	}
}

void EnsureLockThread()
{
	std::lock_guard<std::mutex> g(g_thr_mx);
	if (g_lock_run.load())
		return;

	if (g_lock_thr.joinable())
		g_lock_thr.join();

	g_lock_run.store(true);
	g_lock_thr = std::thread(LockWorker);
}

bool CollectKeyArgs(lua_State* L, int idx, std::vector<std::string>& out, std::string& tag)
{
	out.clear();
	tag.clear();

	if (lua_type(L, idx) == LUA_TSTRING)
	{
		const char* s = lua_tostring(L, idx);
		if (!s || !*s)
			return false;

		out.emplace_back(s);
		tag = s;
		return true;
	}

	if (!lua_istable(L, idx))
		return false;

	const int n = (int)lua_rawlen(L, idx);
	for (int i = 1; i <= n && (int)out.size() < 64; ++i)
	{
		lua_rawgeti(L, idx, i);
		if (lua_type(L, -1) == LUA_TSTRING)
		{
			const char* s = lua_tostring(L, -1);
			if (s && *s)
			{
				if (!tag.empty())
					tag += ",";
				tag += s;
				out.emplace_back(s);
			}
		}
		lua_pop(L, 1);
	}

	return !out.empty();
}

// setgc("ShootCooldown", 0) / setgc({"A","B"}, false)
int SetGcByKey(lua_State* L)
{
	std::vector<std::string> keys;
	std::string tag;
	if (!CollectKeyArgs(L, 1, keys, tag))
		return luaL_error(L, "setgc(key|{keys}, value)");

	GcValue v{};
	if (!ReadGcValue(L, 2, v))
		return luaL_error(L, "setgc: value must be number or boolean");

	std::vector<KeyHit> hits;
	ScanKeyNodes(keys, v.tt, hits, k_hits_max);

	for (const auto& h : hits)
		WriteGcNode(h.node, v);

	lua_pushinteger(L, (lua_Integer)hits.size());
	return 1;
}

// lockgc("ShootCooldown", 0, 100) — держит значение в фоне
int l_lockgc(lua_State* L)
{
	std::vector<std::string> keys;
	std::string tag;
	if (!CollectKeyArgs(L, 1, keys, tag))
		return luaL_error(L, "lockgc(key|{keys}, value [, interval_ms])");

	GcValue v{};
	if (!ReadGcValue(L, 2, v))
		return luaL_error(L, "lockgc: value must be number or boolean");

	if (lua_type(L, 3) == LUA_TNUMBER)
	{
		int ms = (int)lua_tointeger(L, 3);
		if (ms < 10)
			ms = 10;
		if (ms > 5000)
			ms = 5000;
		g_lock_ms.store(ms);
	}

	auto lk = std::make_shared<GcLock>();
	lk->keys = keys;
	lk->tag = tag;
	lk->val = v;
	lk->last_scan = std::chrono::steady_clock::now();
	ScanKeyNodes(lk->keys, v.tt, lk->hits, k_hits_max);

	for (const auto& h : lk->hits)
		WriteGcNode(h.node, v);

	const std::size_t nhits = lk->hits.size();

	{
		std::lock_guard<std::mutex> g(g_lock_mx);
		for (auto it = g_locks.begin(); it != g_locks.end(); ++it)
		{
			if ((*it)->tag == tag)
			{
				g_locks.erase(it);
				break;
			}
		}
		g_locks.push_back(lk);
	}

	EnsureLockThread();

	lua_pushinteger(L, (lua_Integer)nhits);
	return 1;
}

// unlockgc() — всё, unlockgc("ShootCooldown") — один
int l_unlockgc(lua_State* L)
{
	const char* tag = (lua_type(L, 1) == LUA_TSTRING) ? lua_tostring(L, 1) : nullptr;
	int removed = 0;

	{
		std::lock_guard<std::mutex> g(g_lock_mx);
		if (!tag)
		{
			removed = (int)g_locks.size();
			g_locks.clear();
		}

		else
		{
			for (auto it = g_locks.begin(); it != g_locks.end();)
			{
				if ((*it)->tag == tag)
				{
					it = g_locks.erase(it);
					++removed;
				}

				else
				{
					++it;
				}
			}
		}
	}

	lua_pushinteger(L, removed);
	return 1;
}

int l_listgc_locks(lua_State* L)
{
	std::vector<std::shared_ptr<GcLock>> snap;
	{
		std::lock_guard<std::mutex> g(g_lock_mx);
		snap = g_locks;
	}

	lua_createtable(L, (int)snap.size(), 0);

	int n = 0;
	for (const auto& lk : snap)
	{
		std::lock_guard<std::mutex> lg(lk->mx);
		lua_createtable(L, 0, 3);
		lua_pushstring(L, lk->tag.c_str());
		lua_setfield(L, -2, "key");
		if (lk->val.tt == 3)
			lua_pushnumber(L, lk->val.num);
		else
			lua_pushboolean(L, lk->val.b ? 1 : 0);
		lua_setfield(L, -2, "value");
		lua_pushinteger(L, (lua_Integer)lk->hits.size());
		lua_setfield(L, -2, "hits");
		lua_rawseti(L, -2, ++n);
	}

	return 1;
}

// gc.flush() — сбросить кэш прохода по куче, следующий getgc/findgc пойдёт заново
int l_gcflush(lua_State* L)
{
	FlushCaches();
	g_pc.clear();
	lua_pushboolean(L, 1);
	return 1;
}

void RegisterNamespace(lua_State* L)
{
	struct Entry
	{
		const char* name;
		lua_CFunction fn;
	};

	const Entry entries[] = {
		{ "set", l_setgc },
		{ "get", l_getgc },
		{ "find", l_findgc },
		{ "info", l_getgc_info },
		{ "lock", l_lockgc },
		{ "unlock", l_unlockgc },
		{ "locks", l_listgc_locks },
		{ "keys", l_getrawkeys },
		{ "probe", l_gcprobe },
		{ "flush", l_gcflush },
	};

	lua_createtable(L, 0, (int)(sizeof(entries) / sizeof(entries[0])));
	for (const auto& e : entries)
	{
		lua_pushcfunction(L, e.fn);
		lua_setfield(L, -2, e.name);
	}
	lua_setglobal(L, "gc");
}

void EnsureProxyMt(lua_State* L)
{
	luaL_newmetatable(L, "jp.luau_table");
	lua_pushcfunction(L, proxy_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, proxy_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pop(L, 1);

	// слабые ключи: иначе реестр держит каждый когда-либо созданный прокси
	lua_newtable(L);
	lua_newtable(L);
	lua_pushstring(L, "k");
	lua_setfield(L, -2, "__mode");
	lua_setmetatable(L, -2);
	lua_setfield(L, LUA_REGISTRYINDEX, "jp_gtables");
}

#endif // ===== /LuaGc implementation disabled =====
} // namespace

void Register(lua_State* L)
{
#if 0 // ===== LuaGc disabled (setgc/getgc) =====
	EnsureProxyMt(L);

	lua_pushcfunction(L, l_setgc);
	lua_setglobal(L, "setgc");
	lua_pushcfunction(L, l_getgc);
	lua_setglobal(L, "getgc");
	lua_pushcfunction(L, l_getgc_info);
	lua_setglobal(L, "getgc_info");
	lua_pushcfunction(L, l_findgc);
	lua_setglobal(L, "findgc");
	lua_pushcfunction(L, l_gcprobe);
	lua_setglobal(L, "gcprobe");
	lua_pushcfunction(L, l_getrawkeys);
	lua_setglobal(L, "getrawkeys");
	lua_pushcfunction(L, l_lockgc);
	lua_setglobal(L, "lockgc");
	lua_pushcfunction(L, l_unlockgc);
	lua_setglobal(L, "unlockgc");

	RegisterNamespace(L);

	// rawget на прокси должен лезть в игру, не в пустую cheat-таблицу
	lua_getglobal(L, "rawget");
	if (lua_isfunction(L, -1) && lua_tocfunction(L, -1) != l_rawget_proxy)
	{
		lua_pushcclosure(L, l_rawget_proxy, 1);
		lua_setglobal(L, "rawget");
	}

	else
	{
		lua_pop(L, 1);
	}
#endif // ===== LuaGc disabled (setgc/getgc) =====
	(void)L; // no-op while disabled
}

void Stop()
{
#if 0 // ===== LuaGc disabled (setgc/getgc) — migrating to external offsets; keep for later =====
	{
		std::lock_guard<std::mutex> g(g_lock_mx);
		g_locks.clear();
	}

	std::lock_guard<std::mutex> t(g_thr_mx);
	{
		std::lock_guard<std::mutex> g(g_cv_mx);
		g_lock_run.store(false);
	}
	g_lock_cv.notify_all();

	if (g_lock_thr.joinable())
		g_lock_thr.join();

	FlushCaches();
#endif // ===== LuaGc disabled (setgc/getgc) =====
}

} // namespace LuaGc
} // namespace Features
} // namespace Cheat
