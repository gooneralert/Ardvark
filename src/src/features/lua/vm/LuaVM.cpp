#include "pch.h"
#include "LuaVM.h"
#include "LuaBridge.h"
#include "LuaTypes.h"
#include "LuaDrawing.h"
#include "LuaScripts.h"
#include "LuaGc.h"
#include "LuaMem.h"
#include "LuaFiles.h"
#include "LuaPreprocess.h"
#include "ScriptBytecode.h"
#include "features/lua/LuaExecutor.h"
#include "renderer/Renderer.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"
#include "app/Settings.h"

#include <Windows.h>
#include <winhttp.h>
#include <cctype>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaVM {
namespace {

lua_State* g_L = nullptr;
std::mutex g_lua_mu;
std::atomic<bool> g_busy{ false };
std::atomic<bool> g_tick_run{ false };
std::thread g_tick_th;
// set from the UI thread when a restart is requested while the VM lock is busy;
// the tick thread applies it once it can take the lock (next script yield).
std::atomic<bool> g_restart_pending{ false };

// SEH guard around lua_resume (POD-only function so __try is allowed). A fault
// inside the Lua VM becomes LUA_ERRRUN instead of killing the whole external.
// Normal Lua errors (longjmp) pass through untouched.
int LuaResumeCatch(lua_State* co, lua_State* L, int nargs, int* nres)
{
	__try
	{
		return lua_resume(co, L, nargs, nres);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return LUA_ERRRUN;
	}
}

// SEH guard around the source compile (luaL_loadbuffer). A parser/codegen fault
// becomes LUA_ERRRUN instead of taking down the external.
int LuaLoadBufferCatch(lua_State* L, const char* src, size_t len, const char* name)
{
	__try
	{
		return luaL_loadbuffer(L, src, len, name);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return LUA_ERRRUN;
	}
}

struct WaitEntry {
	int ref = LUA_NOREF;
	float left = 0.f;
};
std::vector<WaitEntry> g_waits;

// 0 hb  1 PlrAdd  2 PlrRem  3 CharAdd  4 InBeg  5 InEnd
// 6 ChildAdd  7 ChildRem  8 DescAdd  9 Prop  10 Bindable
// 11 OnClientEvent  12 ProxTriggered
struct ConnEntry {
	int fn_ref = LUA_NOREF;
	bool alive = false;
	bool once = false;
	int kind = 0;
	std::uint64_t owner = 0;
	std::uint32_t seq = 0;
	std::string prop;
};
std::vector<ConnEntry> g_conns;

// ÑÐ»Ð¾Ñ‚Ñ‹ Ð¿ÐµÑ€ÐµÐ¸ÑÐ¿Ð¾Ð»ÑŒÐ·ÑƒÑŽÑ‚ÑÑ Ð¸ Ñ‡Ð¸ÑÑ‚ÑÑ‚ÑÑ Ð½Ð° ÐºÐ°Ð¶Ð´Ñ‹Ð¹ execute â€” Ð±ÐµÐ· seq ÑÑ‚Ð°Ñ€Ñ‹Ð¹
// Connection/Task Ñ‚Ð¾ÐºÐµÐ½ ÑƒÐ±Ð¸Ð» Ð±Ñ‹ Ñ‡ÑƒÐ¶ÑƒÑŽ Ð·Ð°Ð¿Ð¸ÑÑŒ
std::uint32_t g_token_seq = 0;

struct ConnUd {
	int idx = -1;
	std::uint32_t seq = 0;
};

struct SigWait {
	int thr_ref = LUA_NOREF;
	bool alive = false;
	int kind = 0;
	std::uint64_t owner = 0;
	std::string prop;
};
std::vector<SigWait> g_sigwaits;

std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> g_kids_snap;
std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> g_desc_snap;
std::unordered_map<std::string, std::string> g_prop_snap; // "addr|prop" -> val
constexpr size_t k_max_prop_polls = 16;
size_t g_prop_cursor = 0;

struct DelayEntry {
	int fn_ref = LUA_NOREF;
	float left = 0.f;
	bool alive = false;
	std::uint32_t seq = 0;
};
std::vector<DelayEntry> g_delays;

struct DelayUd {
	int idx = -1;
	std::uint32_t seq = 0;
};

unsigned g_poll_tick = 0;

void schedule_wait(lua_State* L, float sec)
{
	// resume Ð³Ð»Ð°Ð²Ð½Ð¾Ð³Ð¾ ÑÑ‚ÐµÐ¹Ñ‚Ð° = ÑÐ¼ÐµÑ€Ñ‚ÑŒ vm, Ñ‚ÑƒÐ´Ð° Ð¿Ð¾Ð¿Ð°ÑÑ‚ÑŒ Ð¼Ð¾Ð¶Ð½Ð¾ Ð¸Ð· delay-ÐºÐ¾Ð»Ð±ÑÐºÐ°
	if (!lua_isyieldable(L))
		return;

	if (sec < 0.f)
		sec = 0.f;

	lua_pushthread(L);
	const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
	g_waits.push_back({ ref, sec });
}

void LogInfo(const char* msg)
{
	LuaExecutor::Log(LuaExecutor::LogLevel::Print, "%s", msg ? msg : "");
}

void LogErr(const char* msg)
{
	LuaExecutor::Log(LuaExecutor::LogLevel::Error, "%s", msg ? msg : "");
}

int l_print(lua_State* L)
{
	const int n = lua_gettop(L);
	std::string line;
	lua_getglobal(L, "tostring");
	for (int i = 1; i <= n; ++i)
	{
		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		if (lua_pcall(L, 1, 1, 0) != LUA_OK)
		{
			LogErr(lua_tostring(L, -1));
			lua_pop(L, 1);
			continue;
		}
		const char* s = lua_tostring(L, -1);
		if (i > 1)
			line.push_back('\t');
		line += s ? s : "nil";
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	LogInfo(line.c_str());
	return 0;
}

int l_warn(lua_State* L)
{
	const int n = lua_gettop(L);
	std::string line;
	for (int i = 1; i <= n; ++i)
	{
		const char* s = luaL_tolstring(L, i, nullptr);
		if (i > 1)
			line.push_back('\t');
		line += s ? s : "nil";
		lua_pop(L, 1);
	}
	LuaExecutor::Log(LuaExecutor::LogLevel::Warn, "%s", line.c_str());
	return 0;
}

int l_identifyexecutor(lua_State* L)
{
	lua_pushstring(L, "Ardvark");
	lua_pushstring(L, "0.1");
	return 2;
}

int l_wait(lua_State* L)
{
	float t = static_cast<float>(luaL_optnumber(L, 1, 0.03));
	if (!lua_isyieldable(L))
		return 0;
	schedule_wait(L, t);
	return lua_yield(L, 0);
}

int l_spawn(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_State* co = lua_newthread(L);
	lua_pushvalue(L, 1);
	lua_xmove(L, co, 1);
	// Lua 5.4: nresults must be a valid int* (writes through it)
	int nres = 0;
	const int status = LuaResumeCatch(co, L, 0, &nres);
	if (status != LUA_OK && status != LUA_YIELD)
	{
		LogErr(lua_tostring(co, -1));
		lua_pop(co, 1);
	}
	// thread Ð½Ð°Ñ€ÑƒÐ¶Ñƒ â€” task.cancel(thread)
	return 1;
}

int alloc_delay(float sec, lua_State* L, int fn_idx)
{
	if (sec < 0.f)
		sec = 0.f;

	int idx = -1;
	for (int i = 0; i < static_cast<int>(g_delays.size()); ++i)
	{
		if (!g_delays[static_cast<size_t>(i)].alive && g_delays[static_cast<size_t>(i)].fn_ref == LUA_NOREF)
		{
			idx = i;
			break;
		}
	}

	if (idx < 0)
	{
		idx = static_cast<int>(g_delays.size());
		g_delays.push_back({});
	}

	lua_pushvalue(L, fn_idx);
	DelayEntry& d = g_delays[static_cast<size_t>(idx)];
	d.fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	d.left = sec;
	d.alive = true;
	d.seq = ++g_token_seq;
	return idx;
}

void push_task_token(lua_State* L, int idx)
{
	auto* ud = static_cast<DelayUd*>(lua_newuserdata(L, sizeof(DelayUd)));
	ud->idx = idx;
	ud->seq = g_delays[static_cast<size_t>(idx)].seq;
	luaL_getmetatable(L, "jewsploit.Task");
	lua_setmetatable(L, -2);
}

int l_delay(lua_State* L)
{
	float t = static_cast<float>(luaL_checknumber(L, 1));
	luaL_checktype(L, 2, LUA_TFUNCTION);
	push_task_token(L, alloc_delay(t, L, 2));
	return 1;
}

int l_defer(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	push_task_token(L, alloc_delay(0.f, L, 1));
	return 1;
}

int l_cancel(lua_State* L)
{
	// delay token
	if (auto* ud = static_cast<DelayUd*>(luaL_testudata(L, 1, "jewsploit.Task")))
	{
		if (ud->idx < 0 || ud->idx >= static_cast<int>(g_delays.size()))
			return 0;

		DelayEntry& d = g_delays[static_cast<size_t>(ud->idx)];
		if (!d.alive || d.seq != ud->seq)
			return 0;

		d.alive = false;
		if (d.fn_ref != LUA_NOREF)
		{
			luaL_unref(L, LUA_REGISTRYINDEX, d.fn_ref);
			d.fn_ref = LUA_NOREF;
		}
		return 0;
	}

	// yielded thread Ð¸Ð· wait/spawn
	if (lua_isthread(L, 1))
	{
		lua_State* co = lua_tothread(L, 1);
		for (size_t i = 0; i < g_waits.size(); ++i)
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, g_waits[i].ref);
			lua_State* wco = lua_tothread(L, -1);
			lua_pop(L, 1);
			if (wco != co)
				continue;

			luaL_unref(L, LUA_REGISTRYINDEX, g_waits[i].ref);
			g_waits.erase(g_waits.begin() + static_cast<std::ptrdiff_t>(i));
			break;
		}

		for (size_t i = 0; i < g_sigwaits.size(); ++i)
		{
			SigWait& w = g_sigwaits[i];
			if (!w.alive || w.thr_ref == LUA_NOREF)
				continue;

			lua_rawgeti(L, LUA_REGISTRYINDEX, w.thr_ref);
			lua_State* wco = lua_tothread(L, -1);
			lua_pop(L, 1);
			if (wco != co)
				continue;

			luaL_unref(L, LUA_REGISTRYINDEX, w.thr_ref);
			w.thr_ref = LUA_NOREF;
			w.alive = false;
			break;
		}
	}

	return 0;
}

int l_getmousepos(lua_State* L)
{
	POINT pt{};
	GetCursorPos(&pt);
	if (HWND hwnd = Renderer::GetHwnd())
		ScreenToClient(hwnd, &pt);
	// Ð´Ð²Ð° number â€” ÐºÐ°Ðº Ð² Ð±Ð¾Ð»ÑŒÑˆÐ¸Ð½ÑÑ‚Ð²Ðµ executor api
	lua_pushnumber(L, static_cast<lua_Number>(pt.x));
	lua_pushnumber(L, static_cast<lua_Number>(pt.y));
	return 2;
}

int l_isrbxactive(lua_State* L)
{
	lua_pushboolean(L, Renderer::IsGameActive() ? 1 : 0);
	return 1;
}

int l_iskeypressed(lua_State* L)
{
	int vk = 0;
	if (lua_type(L, 1) == LUA_TSTRING)
	{
		const char* s = lua_tostring(L, 1);
		if (!s || !s[0]) { lua_pushboolean(L, 0); return 1; }
		if (_stricmp(s, "left") == 0 || _stricmp(s, "lmb") == 0) vk = VK_LBUTTON;
		else if (_stricmp(s, "right") == 0 || _stricmp(s, "rmb") == 0) vk = VK_RBUTTON;
		else if (_stricmp(s, "middle") == 0 || _stricmp(s, "mmb") == 0) vk = VK_MBUTTON;
		else if (_stricmp(s, "space") == 0) vk = VK_SPACE;
		else if (_stricmp(s, "shift") == 0) vk = VK_SHIFT;
		else if (_stricmp(s, "ctrl") == 0) vk = VK_CONTROL;
		else if (_stricmp(s, "alt") == 0) vk = VK_MENU;
		else if (_stricmp(s, "tab") == 0) vk = VK_TAB;
		else if (_stricmp(s, "escape") == 0 || _stricmp(s, "esc") == 0) vk = VK_ESCAPE;
		else vk = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(s[0])));
	}
	else
		vk = static_cast<int>(luaL_checkinteger(L, 1));
	lua_pushboolean(L, (GetAsyncKeyState(vk) & 0x8000) != 0);
	return 1;
}

void OpenSafeLibs(lua_State* L)
{
	luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
	lua_pop(L, 1);

	// standard libs the old luavm (and Matcha base) exposes
	luaL_requiref(L, LUA_OSLIBNAME, luaopen_os, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_IOLIBNAME, luaopen_io, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 1);
	lua_pop(L, 1);

	// ÑƒÐ±Ñ€Ð°Ñ‚ÑŒ Ð¾Ð¿Ð°ÑÐ½Ð¾Ðµ Ð¸Ð· base
	lua_pushnil(L);
	lua_setglobal(L, "dofile");
	lua_pushnil(L);
	lua_setglobal(L, "loadfile");
}

int l_conn_disconnect(lua_State* L)
{
	auto* ud = static_cast<ConnUd*>(luaL_checkudata(L, 1, "jewsploit.RBXScriptConnection"));
	if (!ud || ud->idx < 0 || ud->idx >= static_cast<int>(g_conns.size()))
		return 0;

	ConnEntry& c = g_conns[static_cast<size_t>(ud->idx)];
	if (!c.alive || c.seq != ud->seq)
		return 0;

	c.alive = false;
	if (c.fn_ref != LUA_NOREF)
	{
		luaL_unref(L, LUA_REGISTRYINDEX, c.fn_ref);
		c.fn_ref = LUA_NOREF;
	}
	return 0;
}

bool conn_owner_ok(const ConnEntry& c, int kind, std::uint64_t owner, const char* prop)
{
	if (c.kind != kind)
		return false;
	// owner-bound
	if (kind == 3 || kind == 6 || kind == 7 || kind == 8 || kind == 9
		|| kind == 10 || kind == 11 || kind == 12)
	{
		if (c.owner != owner)
			return false;
	}
	if (kind == 9)
	{
		if (!prop || c.prop != prop)
			return false;
	}
	return true;
}

bool wait_owner_ok(const SigWait& w, int kind, std::uint64_t owner, const char* prop)
{
	if (!w.alive || w.thr_ref == LUA_NOREF || w.kind != kind)
		return false;
	if (kind == 3 || kind == 6 || kind == 7 || kind == 8 || kind == 9
		|| kind == 10 || kind == 11 || kind == 12)
	{
		if (w.owner != owner)
			return false;
	}
	if (kind == 9)
	{
		if (!prop || w.prop != prop)
			return false;
	}
	return true;
}

void kill_conn(ConnEntry& c)
{
	c.alive = false;
	if (c.fn_ref != LUA_NOREF && g_L)
	{
		luaL_unref(g_L, LUA_REGISTRYINDEX, c.fn_ref);
		c.fn_ref = LUA_NOREF;
	}
}

// nargs ÑƒÐ¶Ðµ Ð½Ð° g_L ÑÐ²ÐµÑ€Ñ…Ñƒ. Ð½Ðµ Ð¶Ñ€Ñ‘Ð¼ Ð¸Ñ… â€” caller pop
void fire_conns(int kind, std::uint64_t owner, const char* prop, int nargs)
{
	if (!g_L)
		return;

	if (!lua_checkstack(g_L, nargs + 2))
		return;

	const int arg0 = lua_gettop(g_L) - nargs + 1;
	const size_t n = g_conns.size();
	for (size_t i = 0; i < n && i < g_conns.size(); ++i)
	{
		// Ñ…ÐµÐ½Ð´Ð»ÐµÑ€ Ð¼Ð¾Ð¶ÐµÑ‚ Ð·Ð²Ð°Ñ‚ÑŒ Connect Ð¸ Ñ€Ð°ÑÑ‚Ð¸Ñ‚ÑŒ g_conns â€” Ð´ÐµÑ€Ð¶Ð°Ñ‚ÑŒ ÑÑÑ‹Ð»ÐºÑƒ Ð½ÐµÐ»ÑŒÐ·Ñ
		if (!g_conns[i].alive || g_conns[i].fn_ref == LUA_NOREF)
			continue;
		if (!conn_owner_ok(g_conns[i], kind, owner, prop))
			continue;

		const std::uint32_t seq = g_conns[i].seq;
		const bool once = g_conns[i].once;

		lua_rawgeti(g_L, LUA_REGISTRYINDEX, g_conns[i].fn_ref);
		for (int a = 0; a < nargs; ++a)
			lua_pushvalue(g_L, arg0 + a);
		if (lua_pcall(g_L, nargs, 0, 0) != LUA_OK)
		{
			LogErr(lua_tostring(g_L, -1));
			lua_pop(g_L, 1);
		}

		if (once && i < g_conns.size() && g_conns[i].seq == seq)
			kill_conn(g_conns[i]);
	}
}

void wake_waits(int kind, std::uint64_t owner, const char* prop, int nargs)
{
	if (!g_L)
		return;

	if (!lua_checkstack(g_L, nargs + 2))
		return;

	const int arg0 = lua_gettop(g_L) - nargs + 1;
	const size_t n = g_sigwaits.size();
	for (size_t i = 0; i < n && i < g_sigwaits.size(); ++i)
	{
		// Ñ€ÐµÐ·ÑŽÐ¼ Ð¼Ð¾Ð¶ÐµÑ‚ Ð´Ð¾Ð±Ð°Ð²Ð¸Ñ‚ÑŒ Ð½Ð¾Ð²Ñ‹Ñ… Ð¶Ð´ÑƒÐ½Ð¾Ð² â€” Ð¸Ð½Ð´ÐµÐºÑ, Ð½Ðµ ÑÑÑ‹Ð»ÐºÐ°
		if (!wait_owner_ok(g_sigwaits[i], kind, owner, prop))
			continue;

		const int ref = g_sigwaits[i].thr_ref;
		g_sigwaits[i].alive = false;
		g_sigwaits[i].thr_ref = LUA_NOREF;

		lua_rawgeti(g_L, LUA_REGISTRYINDEX, ref);
		lua_State* co = lua_tothread(g_L, -1);
		luaL_unref(g_L, LUA_REGISTRYINDEX, ref);

		if (!co || !lua_checkstack(co, nargs + 1))
		{
			lua_pop(g_L, 1);
			continue;
		}

		for (int a = 0; a < nargs; ++a)
			lua_pushvalue(g_L, arg0 + a);
		lua_xmove(g_L, co, nargs);
		int nres = 0;
		const int st = LuaResumeCatch(co, g_L, nargs, &nres);
		if (st != LUA_OK && st != LUA_YIELD)
		{
			LogErr(lua_tostring(co, -1));
			lua_pop(co, 1);
		}
		lua_pop(g_L, 1);
	}
}

void fire_sig(int kind, std::uint64_t owner, const char* prop, int nargs)
{
	if (!g_L)
		return;
	fire_conns(kind, owner, prop, nargs);
	wake_waits(kind, owner, prop, nargs);
	if (nargs > 0)
		lua_pop(g_L, nargs);
}

int l_signal_connect(lua_State* L)
{
	luaL_checktype(L, 2, LUA_TFUNCTION);
	const int kind = static_cast<int>(lua_tointeger(L, lua_upvalueindex(1)));
	const std::uint64_t owner = static_cast<std::uint64_t>(lua_tointeger(L, lua_upvalueindex(2)));
	const char* prop = lua_tostring(L, lua_upvalueindex(3));
	const bool once = lua_toboolean(L, lua_upvalueindex(4)) != 0;

	int idx = -1;
	for (int i = 0; i < static_cast<int>(g_conns.size()); ++i)
	{
		if (!g_conns[static_cast<size_t>(i)].alive && g_conns[static_cast<size_t>(i)].fn_ref == LUA_NOREF)
		{
			idx = i;
			break;
		}
	}

	if (idx < 0)
	{
		idx = static_cast<int>(g_conns.size());
		g_conns.push_back({});
	}

	lua_pushvalue(L, 2);
	ConnEntry& c = g_conns[static_cast<size_t>(idx)];
	c.fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	c.alive = true;
	c.once = once;
	c.kind = kind;
	c.owner = owner;
	c.prop = prop ? prop : "";
	c.seq = ++g_token_seq;

	const std::uint32_t seq = c.seq;
	auto* ud = static_cast<ConnUd*>(lua_newuserdata(L, sizeof(ConnUd)));
	ud->idx = idx;
	ud->seq = seq;

	if (luaL_newmetatable(L, "jewsploit.RBXScriptConnection"))
	{
		lua_pushcfunction(L, l_conn_disconnect);
		lua_setfield(L, -2, "Disconnect");
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

int l_signal_wait(lua_State* L)
{
	const int kind = static_cast<int>(lua_tointeger(L, lua_upvalueindex(1)));
	const std::uint64_t owner = static_cast<std::uint64_t>(lua_tointeger(L, lua_upvalueindex(2)));
	const char* prop = lua_tostring(L, lua_upvalueindex(3));

	if (!lua_isyieldable(L))
		return 0;

	lua_pushthread(L);
	const int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	SigWait w{};
	w.thr_ref = ref;
	w.alive = true;
	w.kind = kind;
	w.owner = owner;
	w.prop = prop ? prop : "";
	g_sigwaits.push_back(std::move(w));
	return lua_yield(L, 0);
}

int l_signal_fire(lua_State* L)
{
	(void)L;
	return 0;
}

void push_signal(lua_State* L, int kind, std::uint64_t owner, const char* prop)
{
	lua_newtable(L);

	auto bind = [&](const char* name, bool once) {
		lua_pushinteger(L, kind);
		lua_pushinteger(L, static_cast<lua_Integer>(owner));
		lua_pushstring(L, prop ? prop : "");
		lua_pushboolean(L, once ? 1 : 0);
		lua_pushcclosure(L, l_signal_connect, 4);
		lua_setfield(L, -2, name);
	};

	bind("Connect", false);
	bind("Once", true);

	lua_pushinteger(L, kind);
	lua_pushinteger(L, static_cast<lua_Integer>(owner));
	lua_pushstring(L, prop ? prop : "");
	lua_pushcclosure(L, l_signal_wait, 3);
	lua_setfield(L, -2, "Wait");

	lua_pushinteger(L, kind);
	lua_pushinteger(L, static_cast<lua_Integer>(owner));
	lua_pushstring(L, prop ? prop : "");
	lua_pushcclosure(L, l_signal_fire, 3);
	lua_setfield(L, -2, "Fire");
}

void fire_inst_sig(int kind, std::uint64_t owner, std::uint64_t inst)
{
	if (!g_L || !inst)
		return;
	LuaBridge::PushInstance(g_L, inst);
	fire_sig(kind, owner, nullptr, 1);
}

bool g_plr_seeded = false;
std::unordered_set<std::uint64_t> g_seen_plr;
std::unordered_map<std::uint64_t, std::uint64_t> g_seen_char;

bool has_kind(int k0, int k1 = -1);

void poll_players()
{
	// Ð±ÐµÐ· ÐºÐ¾Ð½Ð½ÐµÐºÑ‚Ð¾Ð² ÑÑ‚Ð¾ Ñ‡Ð¸ÑÑ‚Ñ‹Ð¹ Ð¾Ð±Ñ…Ð¾Ð´ Ð´ÐµÑ‚ÐµÐ¹ Players ÐºÐ°Ð¶Ð´Ñ‹Ð¹ Ñ‚Ð¸Ðº â€” Ð´Ð¾Ñ€Ð¾Ð³Ð¾
	if (!has_kind(1, 2) && !has_kind(3))
	{
		if (g_plr_seeded)
		{
			g_plr_seeded = false;
			g_seen_plr.clear();
			g_seen_char.clear();
		}
		return;
	}

	if (!Globals::Players || !Globals::Players->address)
		return;

	const std::uint64_t pads = Globals::Players->address;
	if (!g_Memory.IsValid(pads))
		return;

	std::unordered_set<std::uint64_t> now;
	for (const auto& c : Instance(pads).GetChildren())
	{
		if (!g_Memory.IsValid(c.address))
			continue;
		if (Instance(c.address).GetClassName() != "Player")
			continue;

		now.insert(c.address);

		if (!g_plr_seeded)
		{
			g_seen_plr.insert(c.address);
			g_seen_char[c.address] = Player(c.address).GetCharacterAddress();
			continue;
		}

		if (!g_seen_plr.count(c.address))
		{
			g_seen_plr.insert(c.address);
			fire_inst_sig(1, 0, c.address);
		}

		const std::uint64_t ch = Player(c.address).GetCharacterAddress();
		const std::uint64_t prev = g_seen_char.count(c.address) ? g_seen_char[c.address] : 0;
		if (ch && ch != prev && g_Memory.IsValid(ch))
			fire_inst_sig(3, c.address, ch);
		g_seen_char[c.address] = ch;
	}

	if (g_plr_seeded)
	{
		for (std::uint64_t old : g_seen_plr)
		{
			if (now.count(old))
				continue;
			fire_inst_sig(2, 0, old);
			g_seen_char.erase(old);
		}
	}

	g_seen_plr.swap(now);
	g_plr_seeded = true;
}

bool g_input_seeded = false;
bool g_key_down[256]{};

void push_input_obj(lua_State* L, int vk)
{
	const char* key_name = "Unknown";
	char letter[2]{};
	if (vk == VK_LBUTTON)
		key_name = "MouseLeftButton";
	else if (vk == VK_RBUTTON)
		key_name = "MouseRightButton";
	else if (vk == VK_MBUTTON)
		key_name = "MouseMiddleButton";
	else if (vk == VK_SPACE)
		key_name = "Space";
	else if (vk == VK_SHIFT)
		key_name = "LeftShift";
	else if (vk == VK_CONTROL)
		key_name = "LeftControl";
	else if (vk == VK_MENU)
		key_name = "LeftAlt";
	else if (vk == VK_TAB)
		key_name = "Tab";
	else if (vk == VK_ESCAPE)
		key_name = "Escape";
	else if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
	{
		letter[0] = static_cast<char>(vk);
		key_name = letter;
	}

	const char* uit = "Keyboard";
	if (vk == VK_LBUTTON)
		uit = "MouseButton1";
	else if (vk == VK_RBUTTON)
		uit = "MouseButton2";
	else if (vk == VK_MBUTTON)
		uit = "MouseButton3";

	lua_newtable(L);

	lua_newtable(L);
	lua_pushinteger(L, vk);
	lua_setfield(L, -2, "Value");
	lua_pushstring(L, key_name);
	lua_setfield(L, -2, "Name");
	lua_setfield(L, -2, "KeyCode");

	lua_newtable(L);
	lua_pushstring(L, uit);
	lua_setfield(L, -2, "Name");
	lua_setfield(L, -2, "UserInputType");
}

void fire_input_sig(int kind, int vk)
{
	if (!g_L)
		return;
	push_input_obj(g_L, vk);
	lua_pushboolean(g_L, 0);
	fire_sig(kind, 0, nullptr, 2);
}

bool has_kind(int k0, int k1)
{
	for (const auto& c : g_conns)
	{
		if (c.alive && (c.kind == k0 || c.kind == k1))
			return true;
	}
	for (const auto& w : g_sigwaits)
	{
		if (w.alive && (w.kind == k0 || w.kind == k1))
			return true;
	}
	return false;
}

void poll_input()
{
	if (!has_kind(4, 5))
		return;

	static const int k_vks[] = {
		VK_LBUTTON, VK_RBUTTON, VK_MBUTTON,
		VK_SPACE, VK_SHIFT, VK_CONTROL, VK_MENU, VK_TAB, VK_ESCAPE,
		VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
		'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
		'Z', 'X', 'C', 'V', 'B', 'N', 'M',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	};

	for (int vk : k_vks)
	{
		if (vk < 0 || vk > 255)
			continue;
		const bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
		if (!g_input_seeded)
		{
			g_key_down[vk] = now;
			continue;
		}

		if (now && !g_key_down[vk])
			fire_input_sig(4, vk);

		else if (!now && g_key_down[vk])
			fire_input_sig(5, vk);

		g_key_down[vk] = now;
	}

	g_input_seeded = true;
}

void collect_desc(std::uint64_t root, std::unordered_set<std::uint64_t>& out, size_t& nodes, int depth = 0)
{
	// workspace DescendantAdded Ð±ÐµÐ· Ð»Ð¸Ð¼Ð¸Ñ‚Ð° = ÑÐ¼ÐµÑ€Ñ‚ÑŒ
	if (!g_Memory.IsValid(root) || nodes > 1500 || depth > 24)
		return;
	for (const auto& c : Instance(root).GetChildren())
	{
		if (!g_Memory.IsValid(c.address))
			continue;
		if (!out.insert(c.address).second)
			continue;
		++nodes;
		collect_desc(c.address, out, nodes, depth + 1);
	}
}

std::string read_prop_snap(std::uint64_t addr, const std::string& prop)
{
	if (!g_Memory.IsValid(addr) || prop.empty())
		return {};

	Instance inst(addr);
	const std::string cls = inst.GetClassName();

	if (prop == "Name")
		return inst.GetName();
	if (prop == "ClassName")
		return cls;
	if (prop == "Parent")
	{
		auto p = inst.GetParent();
		if (!p || !g_Memory.IsValid(p->address))
			return "0";
		return std::to_string(p->address);
	}

	if (prop == "Value")
	{
		const std::uint64_t va = addr + ::Misc::Value;
		if (cls == "BoolValue")
			return g_Memory.Read<std::uint8_t>(va) ? "1" : "0";
		if (cls == "IntValue")
			return std::to_string(g_Memory.Read<std::int32_t>(va));
		if (cls == "NumberValue")
			return std::to_string(g_Memory.Read<double>(va));
		if (cls == "StringValue")
			return g_Memory.ReadString(va);
		if (cls == "ObjectValue")
			return std::to_string(g_Memory.Read<std::uint64_t>(va));
	}

	if (cls == "Humanoid")
	{
		Humanoid hu(addr);
		if (prop == "Health")
			return std::to_string(hu.GetHealth());
		if (prop == "MaxHealth")
			return std::to_string(hu.GetMaxHealth());
		if (prop == "WalkSpeed")
			return std::to_string(hu.GetWalkSpeed());
	}

	if (prop == "Text")
	{
		// gui text â€” ÐµÑÐ»Ð¸ StringValue-like string at Value
		if (cls == "StringValue")
			return g_Memory.ReadString(addr + ::Misc::Value);
	}

	return {};
}

void poll_hierarchy()
{
	if (!g_L)
		return;

	std::unordered_set<std::uint64_t> watch_kids;
	std::unordered_set<std::uint64_t> watch_desc;

	for (const auto& c : g_conns)
	{
		if (!c.alive)
			continue;
		if (c.kind == 6 || c.kind == 7)
			watch_kids.insert(c.owner);
		if (c.kind == 8)
			watch_desc.insert(c.owner);
	}
	for (const auto& w : g_sigwaits)
	{
		if (!w.alive)
			continue;
		if (w.kind == 6 || w.kind == 7)
			watch_kids.insert(w.owner);
		if (w.kind == 8)
			watch_desc.insert(w.owner);
	}

	// ÑÐ½Ð°Ð¿ÑˆÐ¾Ñ‚Ñ‹ Ð¶Ð¸Ð²ÑƒÑ‚ Ð¿Ð¾ Ð°Ð´Ñ€ÐµÑÑƒ â€” Ð±ÐµÐ· Ñ‡Ð¸ÑÑ‚ÐºÐ¸ Ð¼Ð°Ð¿Ñ‹ Ñ€Ð°ÑÑ‚ÑƒÑ‚ Ð²ÑÑŽ ÑÐµÑÑÐ¸ÑŽ
	for (auto it = g_kids_snap.begin(); it != g_kids_snap.end();)
		it = watch_kids.count(it->first) ? std::next(it) : g_kids_snap.erase(it);
	for (auto it = g_desc_snap.begin(); it != g_desc_snap.end();)
		it = watch_desc.count(it->first) ? std::next(it) : g_desc_snap.erase(it);

	// ÐºÐ°Ð¶Ð´Ñ‹Ð¹ Ñ‚Ð¸Ðº Ð¾Ð±Ñ…Ð¾Ð´Ð¸Ñ‚ÑŒ Ð´ÐµÑ‚ÐµÐ¹ Ð²ÑÐµÑ… watched = ÑÐ¾Ñ‚Ð½Ð¸ ReadProcessMemory Ð² ÑÐµÐºÑƒÐ½Ð´Ñƒ
	if ((g_poll_tick % 2) == 0)
	{
		for (std::uint64_t owner : watch_kids)
		{
			if (!g_Memory.IsValid(owner))
				continue;

			std::unordered_set<std::uint64_t> now;
			for (const auto& c : Instance(owner).GetChildren())
			{
				if (g_Memory.IsValid(c.address))
					now.insert(c.address);
			}

			auto it = g_kids_snap.find(owner);
			if (it == g_kids_snap.end())
			{
				g_kids_snap.emplace(owner, std::move(now));
				continue;
			}

			for (std::uint64_t a : now)
			{
				if (!it->second.count(a))
					fire_inst_sig(6, owner, a);
			}
			for (std::uint64_t a : it->second)
			{
				if (!now.count(a))
					fire_inst_sig(7, owner, a);
			}
			it->second.swap(now);
		}
	}

	// DescendantAdded â€” Ñ€Ð°Ð· Ð² N Ñ‚Ð¸ÐºÐ¾Ð², Ð° Ñ‚Ð¾ workspace Ð¶Ñ€Ñ‘Ñ‚ Ð²ÑÑ‘
	if (!watch_desc.empty() && (g_poll_tick % 8) == 0)
	{
		for (std::uint64_t owner : watch_desc)
		{
			if (!g_Memory.IsValid(owner))
				continue;

			std::unordered_set<std::uint64_t> now;
			size_t nodes = 0;
			collect_desc(owner, now, nodes, 0);

			auto it = g_desc_snap.find(owner);
			if (it == g_desc_snap.end())
			{
				g_desc_snap[owner] = std::move(now);
				continue;
			}

			int fired = 0;
			bool capped = false;
			for (std::uint64_t a : now)
			{
				if (it->second.count(a))
					continue;
				if (fired >= 32)
				{
					capped = true;
					break;
				}
				fire_inst_sig(8, owner, a);
				++fired;
				it->second.insert(a);
			}
			// ÑƒÐ¿Ñ‘Ñ€Ð»Ð¸ÑÑŒ Ð² Ð»Ð¸Ð¼Ð¸Ñ‚ â€” Ð¾ÑÑ‚Ð°Ñ‚Ð¾Ðº Ð´Ð¾Ð±Ð¸Ñ€Ð°ÐµÐ¼ ÑÐ»ÐµÐ´ÑƒÑŽÑ‰Ð¸Ð¼ Ð¿Ñ€Ð¾Ñ…Ð¾Ð´Ð¾Ð¼,
			// swap Ð¿Ñ€Ð¾Ð³Ð»Ð¾Ñ‚Ð¸Ð» Ð±Ñ‹ Ð¸Ñ… Ð½Ð°Ð²ÑÐµÐ³Ð´Ð°
			if (!capped)
				it->second.swap(now);
		}
	}
}

void poll_props()
{
	if (!g_L)
		return;

	struct key_t { std::uint64_t addr; std::string prop; std::string mapk; };
	std::vector<key_t> keys;
	std::unordered_set<std::string> seen;

	auto add = [&](std::uint64_t a, const std::string& p) {
		if (!a || p.empty())
			return;
		std::string mapk = std::to_string(a) + "|" + p;
		if (!seen.insert(mapk).second)
			return;
		keys.push_back({ a, p, std::move(mapk) });
	};

	for (const auto& c : g_conns)
	{
		if (c.alive && c.kind == 9)
			add(c.owner, c.prop);
	}
	for (const auto& w : g_sigwaits)
	{
		if (w.alive && w.kind == 9)
			add(w.owner, w.prop);
	}

	if (keys.empty())
	{
		g_prop_snap.clear();
		g_prop_cursor = 0;
		return;
	}

	if (g_prop_snap.size() > keys.size())
	{
		for (auto it = g_prop_snap.begin(); it != g_prop_snap.end();)
			it = seen.count(it->first) ? std::next(it) : g_prop_snap.erase(it);
	}

	// ÐºÐ°Ð¶Ð´Ñ‹Ð¹ ÐºÐ»ÑŽÑ‡ = Ñ‡Ñ‚ÐµÐ½Ð¸Ðµ Ñ‡ÑƒÐ¶Ð¾Ð³Ð¾ Ð¿Ñ€Ð¾Ñ†ÐµÑÑÐ°; Ð·Ð° Ñ‚Ð¸Ðº Ð±ÐµÑ€Ñ‘Ð¼ Ñ„Ð¸ÐºÑÐ¸Ñ€Ð¾Ð²Ð°Ð½Ð½Ñ‹Ð¹ ÐºÑƒÑÐ¾Ðº
	// Ð¿Ð¾ ÐºÑ€ÑƒÐ³Ñƒ, Ð¸Ð½Ð°Ñ‡Ðµ 500 Changed-ÐºÐ¾Ð½Ð½ÐµÐºÑ‚Ð¾Ð² Ð²ÐµÑˆÐ°ÑŽÑ‚ Ñ‚Ð¸ÐºÐµÑ€
	if (g_prop_cursor >= keys.size())
		g_prop_cursor = 0;

	const size_t budget = keys.size() < k_max_prop_polls ? keys.size() : k_max_prop_polls;
	for (size_t n = 0; n < budget; ++n)
	{
		const key_t& k = keys[g_prop_cursor];
		g_prop_cursor = (g_prop_cursor + 1) % keys.size();

		const std::string cur = read_prop_snap(k.addr, k.prop);
		auto it = g_prop_snap.find(k.mapk);
		if (it == g_prop_snap.end())
		{
			g_prop_snap.emplace(k.mapk, cur);
			continue;
		}
		if (it->second == cur)
			continue;
		it->second = cur;
		fire_sig(9, k.addr, k.prop.c_str(), 0);
	}
}

std::uint64_t g_bus_seq_addr = 0;
std::uint64_t g_bus_pay_addr = 0;
std::int32_t g_bus_last_seq = 0;
bool g_bus_seeded = false;

// ReplicatedStorage.JewsploitTest.Bus â€” LocalScript ÐºÐ»Ð°Ð´Ñ‘Ñ‚ Ð¸Ð²ÐµÐ½Ñ‚Ñ‹, Ð¼Ñ‹ poll
std::uint64_t find_child_named(std::uint64_t parent, const char* name)
{
	if (!g_Memory.IsValid(parent) || !name || !name[0])
		return 0;
	for (const auto& c : Instance(parent).GetChildren())
	{
		if (!g_Memory.IsValid(c.address))
			continue;
		if (Instance(c.address).GetName() == name)
			return c.address;
	}
	return 0;
}

std::uint64_t find_jp_bus()
{
	if (!Globals::InstanceDataModel.address
		|| !g_Memory.IsValid(Globals::InstanceDataModel.address))
		return 0;

	const std::uint64_t rs = find_child_named(
		Globals::InstanceDataModel.address, "ReplicatedStorage");
	if (!rs)
		return 0;
	const std::uint64_t folder = find_child_named(rs, "JewsploitTest");
	if (!folder)
		return 0;
	return find_child_named(folder, "Bus");
}

// JP_Prompt Ð»ÐµÐ¶Ð¸Ñ‚ Ð² Workspace.JP_Pad, Ð½Ðµ Ð² ÐºÐ¾Ñ€Ð½Ðµ
std::uint64_t find_child_deep(std::uint64_t parent, const char* name, int depth)
{
	if (!g_Memory.IsValid(parent) || !name || !name[0] || depth < 0)
		return 0;

	const std::uint64_t hit = find_child_named(parent, name);
	if (hit)
		return hit;

	if (depth == 0)
		return 0;

	for (const auto& c : Instance(parent).GetChildren())
	{
		if (!g_Memory.IsValid(c.address))
			continue;
		const std::uint64_t nested = find_child_deep(c.address, name, depth - 1);
		if (nested)
			return nested;
	}
	return 0;
}

std::uint64_t resolve_jp_target(const std::string& name)
{
	if (name.empty())
		return 0;

	const std::uint64_t rs = find_child_named(
		Globals::InstanceDataModel.address, "ReplicatedStorage");
	const std::uint64_t folder = rs ? find_child_named(rs, "JewsploitTest") : 0;
	if (folder)
	{
		const std::uint64_t hit = find_child_deep(folder, name.c_str(), 2);
		if (hit)
			return hit;
	}

	if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
	{
		const std::uint64_t hit = find_child_deep(
			Globals::Workspace->address, name.c_str(), 3);
		if (hit)
			return hit;
	}
	return 0;
}

void split_tab(const std::string& s, std::vector<std::string>& out)
{
	out.clear();
	std::size_t i = 0;
	while (i <= s.size())
	{
		std::size_t j = s.find('\t', i);
		if (j == std::string::npos)
		{
			out.push_back(s.substr(i));
			break;
		}
		out.push_back(s.substr(i, j - i));
		i = j + 1;
	}
}

void poll_jp_bus()
{
	if (!g_L)
		return;

	if (!has_kind(11, 12))
	{
		g_bus_seeded = false;
		g_bus_seq_addr = 0;
		g_bus_pay_addr = 0;
		return;
	}

	// Ñ‚Ñ€Ð¸ Ð¿Ð¾Ð¸ÑÐºÐ° Ð¿Ð¾ Ð¸Ð¼ÐµÐ½Ð¸ Ð½Ð° ÐºÐ°Ð¶Ð´Ñ‹Ð¹ Ñ‚Ð¸Ðº â€” Ð´ÐµÑ€Ð¶Ð¸Ð¼ Ð°Ð´Ñ€ÐµÑÐ°, Ð¿Ð¾ÐºÐ° Ð²Ð°Ð»Ð¸Ð´Ð½Ñ‹
	if (!g_bus_seq_addr || !g_bus_pay_addr
		|| !g_Memory.IsValid(g_bus_seq_addr) || !g_Memory.IsValid(g_bus_pay_addr))
	{
		g_bus_seq_addr = 0;
		g_bus_pay_addr = 0;

		const std::uint64_t bus = find_jp_bus();
		if (!bus)
			return;

		const std::uint64_t seq_i = find_child_named(bus, "Seq");
		const std::uint64_t pay_i = find_child_named(bus, "Payload");
		if (!seq_i || !pay_i)
			return;
		if (Instance(seq_i).GetClassName() != "IntValue")
			return;
		if (Instance(pay_i).GetClassName() != "StringValue")
			return;

		g_bus_seq_addr = seq_i;
		g_bus_pay_addr = pay_i;
		g_bus_seeded = false;
	}

	const std::int32_t seq = g_Memory.Read<std::int32_t>(
		g_bus_seq_addr + ::Misc::Value);

	if (!g_bus_seeded)
	{
		g_bus_last_seq = seq;
		g_bus_seeded = true;
		return;
	}
	if (seq == g_bus_last_seq)
		return;
	if (seq < g_bus_last_seq)
	{
		g_bus_last_seq = seq;
		return;
	}
	g_bus_last_seq = seq;

	const std::string payload = g_Memory.ReadString(
		g_bus_pay_addr + ::Misc::Value);
	if (payload.empty())
		return;

	std::vector<std::string> parts;
	split_tab(payload, parts);
	if (parts.size() < 2)
		return;

	const std::string& kind_s = parts[0];
	const std::string& name = parts[1];
	int kind = 0;
	if (kind_s == "OE")
		kind = 11;

	else if (kind_s == "PX")
		kind = 12;

	else
		return;

	const std::uint64_t target = resolve_jp_target(name);
	if (!target)
		return;

	int nargs = (int)parts.size() - 2;
	if (nargs > 16)
		nargs = 16;
	if (!lua_checkstack(g_L, nargs + 4))
		return;
	for (int i = 0; i < nargs; ++i)
		lua_pushstring(g_L, parts[(std::size_t)(2 + i)].c_str());
	fire_sig(kind, target, nullptr, nargs);
}

void RegisterRunService(lua_State* L)
{
	lua_newtable(L);

	push_signal(L, 0, 0, nullptr);
	lua_setfield(L, -2, "Heartbeat");

	push_signal(L, 0, 0, nullptr);
	lua_setfield(L, -2, "RenderStepped");

	// ÑÑ‚Ð°Ñ€Ñ‹Ðµ ÑÐºÑ€Ð¸Ð¿Ñ‚Ñ‹ Ð¸Ð½Ð¾Ð³Ð´Ð° Stepped Ð¶Ñ€ÑƒÑ‚
	push_signal(L, 0, 0, nullptr);
	lua_setfield(L, -2, "Stepped");

	lua_setglobal(L, "RunService");
}

void RegisterUserInputService(lua_State* L)
{
	lua_newtable(L);
	push_signal(L, 4, 0, nullptr);
	lua_setfield(L, -2, "InputBegan");
	push_signal(L, 5, 0, nullptr);
	lua_setfield(L, -2, "InputEnded");
	lua_setglobal(L, "UserInputService");
}

// --- bit32 ---
std::uint32_t bit_to_u32(lua_State* L, int idx)
{
	return static_cast<std::uint32_t>(static_cast<std::int64_t>(luaL_checknumber(L, idx)));
}

int l_bit_band(lua_State* L)
{
	int n = lua_gettop(L);
	std::uint32_t r = 0xFFFFFFFFu;
	for (int i = 1; i <= n; ++i)
		r &= bit_to_u32(L, i);
	lua_pushinteger(L, static_cast<lua_Integer>(r));
	return 1;
}

int l_bit_bor(lua_State* L)
{
	int n = lua_gettop(L);
	std::uint32_t r = 0;
	for (int i = 1; i <= n; ++i)
		r |= bit_to_u32(L, i);
	lua_pushinteger(L, static_cast<lua_Integer>(r));
	return 1;
}

int l_bit_bxor(lua_State* L)
{
	int n = lua_gettop(L);
	std::uint32_t r = 0;
	for (int i = 1; i <= n; ++i)
		r ^= bit_to_u32(L, i);
	lua_pushinteger(L, static_cast<lua_Integer>(r));
	return 1;
}

int l_bit_bnot(lua_State* L)
{
	const std::uint32_t r = ~bit_to_u32(L, 1);
	lua_pushinteger(L, static_cast<lua_Integer>(r));
	return 1;
}

int l_bit_lshift(lua_State* L)
{
	std::uint32_t x = bit_to_u32(L, 1);
	int d = static_cast<int>(luaL_checkinteger(L, 2)) & 31;
	lua_pushinteger(L, static_cast<lua_Integer>(x << d));
	return 1;
}

int l_bit_rshift(lua_State* L)
{
	std::uint32_t x = bit_to_u32(L, 1);
	int d = static_cast<int>(luaL_checkinteger(L, 2)) & 31;
	lua_pushinteger(L, static_cast<lua_Integer>(x >> d));
	return 1;
}

int l_bit_arshift(lua_State* L)
{
	std::int32_t x = static_cast<std::int32_t>(bit_to_u32(L, 1));
	int d = static_cast<int>(luaL_checkinteger(L, 2)) & 31;
	lua_pushinteger(L, static_cast<lua_Integer>(static_cast<std::uint32_t>(x >> d)));
	return 1;
}

int l_bit_lrotate(lua_State* L)
{
	std::uint32_t x = bit_to_u32(L, 1);
	int d = static_cast<int>(luaL_checkinteger(L, 2)) & 31;
	// ÑÐ´Ð²Ð¸Ð³ Ð½Ð° 32 â€” UB, Ð¿Ñ€Ð¸ d==0 Ð²Ñ‚Ð¾Ñ€Ð°Ñ Ð¿Ð¾Ð»Ð¾Ð²Ð¸Ð½Ð° Ð¾Ð±ÑÐ·Ð°Ð½Ð° Ð±Ñ‹Ñ‚ÑŒ Ð½ÑƒÐ»Ñ‘Ð¼
	lua_pushinteger(L, static_cast<lua_Integer>((x << d) | (x >> ((32 - d) & 31))));
	return 1;
}

int l_bit_rrotate(lua_State* L)
{
	std::uint32_t x = bit_to_u32(L, 1);
	int d = static_cast<int>(luaL_checkinteger(L, 2)) & 31;
	lua_pushinteger(L, static_cast<lua_Integer>((x >> d) | (x << ((32 - d) & 31))));
	return 1;
}

int l_bit_extract(lua_State* L)
{
	std::uint32_t n = bit_to_u32(L, 1);
	int f = static_cast<int>(luaL_checkinteger(L, 2));
	int w = static_cast<int>(luaL_optinteger(L, 3, 1));
	if (f < 0) f = 0;
	if (f > 31) f = 31;
	if (w < 1) w = 1;
	if (w > 32 - f) w = 32 - f;
	std::uint32_t mask = (w >= 32) ? 0xFFFFFFFFu : ((1u << w) - 1u);
	lua_pushinteger(L, static_cast<lua_Integer>((n >> f) & mask));
	return 1;
}

int l_bit_replace(lua_State* L)
{
	std::uint32_t n = bit_to_u32(L, 1);
	std::uint32_t v = bit_to_u32(L, 2);
	int f = static_cast<int>(luaL_checkinteger(L, 3));
	int w = static_cast<int>(luaL_optinteger(L, 4, 1));
	if (f < 0) f = 0;
	if (f > 31) f = 31;
	if (w < 1) w = 1;
	if (w > 32 - f) w = 32 - f;
	std::uint32_t mask = (w >= 32) ? 0xFFFFFFFFu : ((1u << w) - 1u);
	n = (n & ~(mask << f)) | ((v & mask) << f);
	lua_pushinteger(L, static_cast<lua_Integer>(n));
	return 1;
}

void RegisterBit32(lua_State* L)
{
	lua_newtable(L);
	lua_pushcfunction(L, l_bit_band); lua_setfield(L, -2, "band");
	lua_pushcfunction(L, l_bit_bor); lua_setfield(L, -2, "bor");
	lua_pushcfunction(L, l_bit_bxor); lua_setfield(L, -2, "bxor");
	lua_pushcfunction(L, l_bit_bnot); lua_setfield(L, -2, "bnot");
	lua_pushcfunction(L, l_bit_lshift); lua_setfield(L, -2, "lshift");
	lua_pushcfunction(L, l_bit_rshift); lua_setfield(L, -2, "rshift");
	lua_pushcfunction(L, l_bit_arshift); lua_setfield(L, -2, "arshift");
	lua_pushcfunction(L, l_bit_lrotate); lua_setfield(L, -2, "lrotate");
	lua_pushcfunction(L, l_bit_rrotate); lua_setfield(L, -2, "rrotate");
	lua_pushcfunction(L, l_bit_extract); lua_setfield(L, -2, "extract");
	lua_pushcfunction(L, l_bit_replace); lua_setfield(L, -2, "replace");
	lua_setglobal(L, "bit32");
}

// --- tween noop ---
int l_tween_play(lua_State* L)
{
	(void)L;
	return 0;
}

int l_tween_create(lua_State* L)
{
	(void)luaL_checkany(L, 1);
	lua_newtable(L);
	lua_pushcfunction(L, l_tween_play);
	lua_setfield(L, -2, "Play");
	lua_pushcfunction(L, l_tween_play);
	lua_setfield(L, -2, "Cancel");
	lua_pushcfunction(L, l_tween_play);
	lua_setfield(L, -2, "Pause");
	lua_pushcfunction(L, l_tween_play);
	lua_setfield(L, -2, "Destroy");
	// Completed â€” Ð¼Ñ‘Ñ€Ñ‚Ð²Ñ‹Ð¹ signal (kind 99 Ð½Ð¸ÐºÑ‚Ð¾ Ð½Ðµ Ñ„Ð°Ð¹Ñ€Ð¸Ñ‚)
	push_signal(L, 99, 0, nullptr);
	lua_setfield(L, -2, "Completed");
	return 1;
}

void RegisterTweenService(lua_State* L)
{
	lua_newtable(L);
	lua_pushcfunction(L, l_tween_create);
	lua_setfield(L, -2, "Create");
	lua_setglobal(L, "TweenService");
}

// --- http ---
bool parse_url(const std::string& url, bool& https, std::wstring& host, INTERNET_PORT& port, std::wstring& path)
{
	https = false;
	host.clear();
	path = L"/";
	port = 80;

	std::string u = url;
	if (u.rfind("https://", 0) == 0)
	{
		https = true;
		port = 443;
		u = u.substr(8);
	}

	else if (u.rfind("http://", 0) == 0)
	{
		u = u.substr(7);
	}

	else
		return false;

	size_t slash = u.find('/');
	std::string h = (slash == std::string::npos) ? u : u.substr(0, slash);
	std::string p = (slash == std::string::npos) ? "/" : u.substr(slash);

	size_t colon = h.find(':');
	if (colon != std::string::npos)
	{
		port = static_cast<INTERNET_PORT>(std::atoi(h.c_str() + colon + 1));
		h = h.substr(0, colon);
	}

	host.assign(h.begin(), h.end());
	path.assign(p.begin(), p.end());
	return !host.empty();
}

bool http_request_raw(const std::string& method, const std::string& url,
	const std::string& body, const std::string& hdrs,
	std::string& out, int& status)
{
	out.clear();
	status = 0;

	bool https = false;
	std::wstring host, path;
	INTERNET_PORT port = 80;
	if (!parse_url(url, https, host, port, path))
		return false;

	HINTERNET ses = WinHttpOpen(L"jewsploit/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!ses)
		return false;

	HINTERNET con = WinHttpConnect(ses, host.c_str(), port, 0);
	if (!con)
	{
		WinHttpCloseHandle(ses);
		return false;
	}

	std::wstring meth(method.begin(), method.end());
	DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
	HINTERNET req = WinHttpOpenRequest(con, meth.c_str(), path.c_str(), nullptr,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!req)
	{
		WinHttpCloseHandle(con);
		WinHttpCloseHandle(ses);
		return false;
	}

	WinHttpSetTimeouts(req, 5000, 5000, 15000, 15000);

	std::wstring wh;
	if (!hdrs.empty())
		wh.assign(hdrs.begin(), hdrs.end());

	BOOL ok = WinHttpSendRequest(req,
		wh.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wh.c_str(),
		wh.empty() ? 0 : static_cast<DWORD>(-1L),
		body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
		static_cast<DWORD>(body.size()),
		static_cast<DWORD>(body.size()), 0);

	if (!ok || !WinHttpReceiveResponse(req, nullptr))
	{
		WinHttpCloseHandle(req);
		WinHttpCloseHandle(con);
		WinHttpCloseHandle(ses);
		return false;
	}

	DWORD st = 0;
	DWORD sz = sizeof(st);
	WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX, &st, &sz, WINHTTP_NO_HEADER_INDEX);
	status = static_cast<int>(st);

	for (;;)
	{
		DWORD avail = 0;
		if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0)
			break;
		size_t old = out.size();
		out.resize(old + avail);
		DWORD read = 0;
		if (!WinHttpReadData(req, out.data() + old, avail, &read))
		{
			out.resize(old);
			break;
		}
		out.resize(old + read);
		if (out.size() > (8u << 20))
			break;
	}

	WinHttpCloseHandle(req);
	WinHttpCloseHandle(con);
	WinHttpCloseHandle(ses);
	return true;
}

void push_http_result(lua_State* L, bool ok, int status, const std::string& body)
{
	lua_newtable(L);
	lua_pushboolean(L, ok && status >= 200 && status < 300);
	lua_setfield(L, -2, "Success");
	lua_pushinteger(L, status);
	lua_setfield(L, -2, "StatusCode");
	lua_pushlstring(L, body.data(), body.size());
	lua_setfield(L, -2, "Body");
	lua_pushstring(L, ok ? "OK" : "failed");
	lua_setfield(L, -2, "StatusMessage");
	lua_newtable(L);
	lua_setfield(L, -2, "Headers");
}

int l_request(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	std::string url;
	lua_getfield(L, 1, "Url");
	if (const char* u = lua_tostring(L, -1))
		url = u;
	lua_pop(L, 1);
	if (url.empty())
	{
		lua_getfield(L, 1, "url");
		if (const char* u = lua_tostring(L, -1))
			url = u;
		lua_pop(L, 1);
	}
	if (url.empty())
		return luaL_error(L, "request: Url required");

	std::string method = "GET";
	lua_getfield(L, 1, "Method");
	if (lua_isstring(L, -1))
		method = lua_tostring(L, -1);
	lua_pop(L, 1);

	std::string body;
	lua_getfield(L, 1, "Body");
	if (lua_isstring(L, -1))
	{
		size_t n = 0;
		const char* b = lua_tolstring(L, -1, &n);
		body.assign(b, n);
	}
	lua_pop(L, 1);

	std::string hdrs;
	lua_getfield(L, 1, "Headers");
	if (lua_istable(L, -1))
	{
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING)
			{
				hdrs += lua_tostring(L, -2);
				hdrs += ": ";
				hdrs += lua_tostring(L, -1);
				hdrs += "\r\n";
			}
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	std::string out;
	int status = 0;
	bool ok = http_request_raw(method, url, body, hdrs, out, status);
	push_http_result(L, ok, status, out);
	return 1;
}

int l_httpget(lua_State* L)
{
	const char* url = luaL_checkstring(L, 1);
	int status = 0;
	{
		// lua ÑÐ¾Ð±Ñ€Ð°Ð½ ÐºÐ°Ðº C: luaL_error Ð´ÐµÐ»Ð°ÐµÑ‚ longjmp Ð¼Ð¸Ð¼Ð¾ Ð´ÐµÑÑ‚Ñ€ÑƒÐºÑ‚Ð¾Ñ€Ð¾Ð²,
		// Ð¿Ð¾ÑÑ‚Ð¾Ð¼Ñƒ Ñ‚ÐµÐ»Ð¾ Ð´Ð¾Ð»Ð¶Ð½Ð¾ ÑƒÐ¼ÐµÑ€ÐµÑ‚ÑŒ Ð´Ð¾ Ð¾ÑˆÐ¸Ð±ÐºÐ¸
		std::string out;
		if (http_request_raw("GET", url, {}, {}, out, status)
			&& status >= 200 && status < 300)
		{
			lua_pushlstring(L, out.data(), out.size());
			return 1;
		}
	}
	return luaL_error(L, "HttpGet failed (%d)", status);
}

int l_httppost(lua_State* L)
{
	const char* url = luaL_checkstring(L, 1);
	size_t bn = 0;
	const char* body = luaL_optlstring(L, 2, "", &bn);
	const char* ctype = luaL_optstring(L, 3, "application/json");

	std::string hdrs = std::string("Content-Type: ") + ctype + "\r\n";
	if (lua_istable(L, 4))
	{
		lua_pushnil(L);
		while (lua_next(L, 4))
		{
			if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING)
			{
				hdrs += lua_tostring(L, -2);
				hdrs += ": ";
				hdrs += lua_tostring(L, -1);
				hdrs += "\r\n";
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
	}

	std::string out;
	int status = 0;
	if (http_request_raw("POST", url, std::string(body, bn), hdrs, out, status))
	{
		lua_pushlstring(L, out.data(), out.size());
		return 1;
	}
	lua_pushlstring(L, out.data(), out.size());
	return 1;
}

int l_guid(lua_State* L)
{
	// no ole32 dependency: format from the PRNG
	std::uint8_t b[16];
	for (int i = 0; i < 16; ++i)
		b[i] = static_cast<std::uint8_t>(std::rand() & 0xff);
	b[6] = static_cast<std::uint8_t>((b[6] & 0x0f) | 0x40);
	b[8] = static_cast<std::uint8_t>((b[8] & 0x3f) | 0x80);
	char buf[64];
	std::snprintf(buf, sizeof(buf),
		"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
		b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
	lua_pushstring(L, buf);
	return 1;
}

int http_arg(lua_State* L)
{
	// HttpService:Foo(x) â†’ self,x  /  HttpService.Foo(x) â†’ x
	if (lua_istable(L, 1) && lua_gettop(L) >= 2)
		return 2;
	return 1;
}

int l_httpservice_getasync(lua_State* L)
{
	const int i = http_arg(L);
	luaL_checkstring(L, i);
	// Ð½ÐµÐ»ÑŒÐ·Ñ Ð´ÐµÑ€Ð¶Ð°Ñ‚ÑŒ const char* Ð¸ Ñ‡Ð¸ÑÑ‚Ð¸Ñ‚ÑŒ ÑÑ‚ÐµÐº â€” ÑÑ‚Ñ€Ð¾ÐºÑƒ ÑÐ¾Ð±ÐµÑ€Ñ‘Ñ‚ gc
	lua_pushvalue(L, i);
	lua_replace(L, 1);
	lua_settop(L, 1);
	return l_httpget(L);
}

int l_httpservice_requestasync(lua_State* L)
{
	const int i = http_arg(L);
	if (i == 2)
		lua_remove(L, 1);
	return l_request(L);
}

int l_json_encode(lua_State* L)
{
	const int i = http_arg(L);
	int t = lua_type(L, i);
	if (t == LUA_TNIL || lua_gettop(L) < i)
	{
		lua_pushstring(L, "null");
		return 1;
	}
	if (t == LUA_TBOOLEAN)
	{
		lua_pushstring(L, lua_toboolean(L, i) ? "true" : "false");
		return 1;
	}
	if (t == LUA_TNUMBER)
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%.17g", lua_tonumber(L, i));
		lua_pushstring(L, buf);
		return 1;
	}
	if (t == LUA_TSTRING)
	{
		size_t n = 0;
		const char* s = lua_tolstring(L, i, &n);
		std::string o = "\"";
		for (size_t k = 0; k < n; ++k)
		{
			char c = s[k];
			if (c == '"' || c == '\\')
			{
				o.push_back('\\');
				o.push_back(c);
			}

			else if (c == '\n')
				o += "\\n";

			else
				o.push_back(c);
		}
		o.push_back('"');
		lua_pushlstring(L, o.data(), o.size());
		return 1;
	}
	lua_pushstring(L, "null");
	return 1;
}

int l_json_decode(lua_State* L)
{
	const char* s = luaL_checkstring(L, http_arg(L));
	if (!s)
	{
		lua_pushnil(L);
		return 1;
	}
	if (std::strcmp(s, "null") == 0)
	{
		lua_pushnil(L);
		return 1;
	}
	if (std::strcmp(s, "true") == 0)
	{
		lua_pushboolean(L, 1);
		return 1;
	}
	if (std::strcmp(s, "false") == 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	if (s[0] == '"' )
	{
		std::string o;
		for (const char* p = s + 1; *p && *p != '"'; ++p)
		{
			if (*p == '\\' && p[1])
			{
				++p;
				if (*p == 'n') o.push_back('\n');
				else o.push_back(*p);
			}

			else
				o.push_back(*p);
		}
		lua_pushlstring(L, o.data(), o.size());
		return 1;
	}
	char* end = nullptr;
	double d = std::strtod(s, &end);
	if (end != s)
	{
		lua_pushnumber(L, d);
		return 1;
	}
	lua_pushnil(L);
	return 1;
}

void RegisterHttp(lua_State* L)
{
	lua_pushcfunction(L, l_request);   lua_setglobal(L, "request");
	lua_pushcfunction(L, l_request);   lua_setglobal(L, "http_request");
	lua_pushcfunction(L, l_httpget);   lua_setglobal(L, "HttpGet");
	lua_pushcfunction(L, l_httpget);   lua_setglobal(L, "httpget");
	lua_pushcfunction(L, l_httppost);  lua_setglobal(L, "HttpPost");
	lua_pushcfunction(L, l_httppost);  lua_setglobal(L, "httppost");

	lua_newtable(L);
	lua_pushcfunction(L, l_httpservice_getasync);   lua_setfield(L, -2, "GetAsync");
	lua_pushcfunction(L, l_httpservice_requestasync); lua_setfield(L, -2, "RequestAsync");
	lua_pushcfunction(L, l_json_encode);             lua_setfield(L, -2, "JSONEncode");
	lua_pushcfunction(L, l_json_decode);             lua_setfield(L, -2, "JSONDecode");
	lua_pushcfunction(L, l_guid);                    lua_setfield(L, -2, "GenerateGUID");
	lua_setglobal(L, "HttpService");
}

// ============================================================================
// Script-runner compatible globals (Matcha API surface)
// Ported from the old script runner and adapted to the external C++ VM.
// ============================================================================

std::unordered_map<std::string, std::string> g_fflags;
bool g_input_enabled = true;

// How long to hold a synthesized mouse button between press and release so
// Roblox actually registers a click. Back-to-back SendInput DOWN+UP events can
// be coalesced/dropped by the game loop, which reads as "the script never
// clicks" on an external (non-injected) executor. A few ms of hold fixes that.
constexpr DWORD kClickHoldMs = 15;

// --- clipboard ---
int l_setclipboard(lua_State* L)
{
	const char* s = luaL_checkstring(L, 1);
	if (!OpenClipboard(nullptr))
		return 0;
	EmptyClipboard();
	const size_t n = std::strlen(s);
	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n + 1);
	if (mem)
	{
		char* p = static_cast<char*>(GlobalLock(mem));
		if (p)
		{
			std::memcpy(p, s, n);
			p[n] = '\0';
		}
		GlobalUnlock(mem);
		SetClipboardData(CF_TEXT, mem);
	}
	CloseClipboard();
	return 0;
}

int l_getclipboard(lua_State* L)
{
	std::string out;
	if (OpenClipboard(nullptr))
	{
		if (HANDLE h = GetClipboardData(CF_TEXT))
		{
			const char* p = static_cast<const char*>(GlobalLock(h));
			if (p)
				out = p;
			GlobalUnlock(h);
		}
		CloseClipboard();
	}
	lua_pushlstring(L, out.data(), out.size());
	return 1;
}

// --- base64 ---
std::string b64_encode(const std::string& in)
{
	static const char* tbl =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string o;
	o.reserve(((in.size() + 2) / 3) * 4);
	unsigned int carry = 0;
	int bits = 0;
	for (unsigned char c : in)
	{
		carry = (carry << 8) | c;
		bits += 8;
		while (bits >= 6)
		{
			bits -= 6;
			o.push_back(tbl[(carry >> bits) & 0x3f]);
		}
	}
	if (bits > 0)
		o.push_back(tbl[(carry << (6 - bits)) & 0x3f]);
	while (o.size() % 4)
		o.push_back('=');
	return o;
}

std::string b64_decode(const std::string& in)
{
	auto val = [](int c) -> int {
		if (c >= 'A' && c <= 'Z') return c - 'A';
		if (c >= 'a' && c <= 'z') return c - 'a' + 26;
		if (c >= '0' && c <= '9') return c - '0' + 52;
		if (c == '+') return 62;
		if (c == '/') return 63;
		return -1;
	};
	std::string o;
	int carry = 0;
	int bits = 0;
	for (unsigned char c : in)
	{
		if (c == '=' || c == ' ' || c == '\r' || c == '\n' || c == '\t')
			continue;
		const int v = val(c);
		if (v < 0)
			continue;
		carry = (carry << 6) | v;
		bits += 6;
		if (bits >= 8)
		{
			bits -= 8;
			o.push_back(static_cast<char>((carry >> bits) & 0xff));
		}
	}
	return o;
}

int l_base64encode(lua_State* L)
{
	size_t n = 0;
	const char* s = luaL_checklstring(L, 1, &n);
	std::string o = b64_encode(std::string(s, n));
	lua_pushlstring(L, o.data(), o.size());
	return 1;
}

int l_base64decode(lua_State* L)
{
	size_t n = 0;
	const char* s = luaL_checklstring(L, 1, &n);
	std::string o = b64_decode(std::string(s, n));
	lua_pushlstring(L, o.data(), o.size());
	return 1;
}

// --- fast flags ---
int l_setfflag(lua_State* L)
{
	const char* name = luaL_checkstring(L, 1);
	const char* value = luaL_checkstring(L, 2);
	if (name && *name)
		g_fflags[name] = value ? value : "";
	return 0;
}

int l_getfflag(lua_State* L)
{
	const char* name = luaL_checkstring(L, 1);
	auto it = g_fflags.find(name ? name : "");
	if (it == g_fflags.end())
		lua_pushnil(L);
	else
		lua_pushstring(L, it->second.c_str());
	return 1;
}

// --- time ---
int l_tick(lua_State* L)
{
	const auto now = std::chrono::steady_clock::now().time_since_epoch();
	lua_pushnumber(L, std::chrono::duration<double>(now).count());
	return 1;
}

// --- typeof: Roblox-specific names for userdata ---
int l_typeof(lua_State* L)
{
	if (luaL_testudata(L, 1, "jewsploit.Instance")) { lua_pushstring(L, "Instance"); return 1; }
	if (luaL_testudata(L, 1, "jewsploit.Vector3")) { lua_pushstring(L, "Vector3"); return 1; }
	if (luaL_testudata(L, 1, "jewsploit.Vector2")) { lua_pushstring(L, "Vector2"); return 1; }
	if (luaL_testudata(L, 1, "jewsploit.Color3")) { lua_pushstring(L, "Color3"); return 1; }
	if (luaL_testudata(L, 1, "jewsploit.CFrame")) { lua_pushstring(L, "CFrame"); return 1; }
	if (luaL_testudata(L, 1, "jewsploit.Drawing")) { lua_pushstring(L, "Drawing"); return 1; }
	if (luaL_testudata(L, 1, "jewsploit.RBXScriptConnection")) { lua_pushstring(L, "RBXScriptConnection"); return 1; }
	if (luaL_testudata(L, 1, "jewsploit.Mouse")) { lua_pushstring(L, "Mouse"); return 1; }
	if (luaL_testudata(L, 1, "jewsploit.Task")) { lua_pushstring(L, "task"); return 1; }
	lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
	return 1;
}

// --- loadstring / load ---
int l_loadstring(lua_State* L)
{
	size_t n = 0;
	const char* src = luaL_checklstring(L, 1, &n);
	const char* name = luaL_optstring(L, 2, "chunk");
	const std::string processed = LuaPreprocess::Preprocess(std::string(src, n));
	if (luaL_loadbuffer(L, processed.data(), processed.size(), name) != LUA_OK)
	{
		lua_pushnil(L);
		lua_insert(L, -2);
		return 2;
	}
	return 1;
}

int l_load(lua_State* L)
{
	return l_loadstring(L);
}

// --- run_secure: protected payload (base64), fallback to raw ---
int l_run_secure(lua_State* L)
{
	size_t n = 0;
	const char* src = luaL_checklstring(L, 1, &n);
	std::string code(src, n);
	std::string run = b64_decode(code);
	if (run.empty())
		run = code;
	run = LuaPreprocess::Preprocess(run);

	if (luaL_loadbuffer(L, run.data(), run.size(), "run_secure") != LUA_OK)
	{
		LogErr(lua_tostring(L, -1));
		lua_pop(L, 1);
		return 0;
	}
	if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK)
	{
		LogErr(lua_tostring(L, -1));
		lua_pop(L, 1);
		return 0;
	}
	return lua_gettop(L);
}

// --- notify ---
int l_notify(lua_State* L)
{
	const char* msg = luaL_optstring(L, 1, "");
	const char* title = luaL_optstring(L, 2, "Script");
	LuaExecutor::Log(LuaExecutor::LogLevel::Success, "[notify|%s] %s", title, msg);
	return 0;
}

// --- getgamename (script runner also exposed it as getgetname) ---
int l_getgamename(lua_State* L)
{
	if (g_Memory.IsValid(Globals::InstanceDataModel.address))
		lua_pushstring(L, Globals::InstanceDataModel.GetName().c_str());
	else
		lua_pushstring(L, "");
	return 1;
}

// --- getscripthash: FNV-1a 64 over the raw bytecode, hex string ---
std::uint64_t fnv1a64(const std::string& s)
{
	std::uint64_t h = 14695981039346656037ULL;
	for (unsigned char c : s)
	{
		h ^= c;
		h *= 1099511628211ULL;
	}
	return h;
}

int l_getscripthash(lua_State* L)
{
	std::uint64_t addr = LuaBridge::CheckAddress(L, 1);
	if (!g_Memory.IsValid(addr))
	{
		lua_pushstring(L, "0");
		return 1;
	}
	Instance inst(addr);
	const std::string cls = inst.GetClassName();
	std::vector<std::uint8_t> raw;
	if (!ScriptBytecode::ReadRaw(addr, cls, raw) || raw.empty())
	{
		lua_pushstring(L, "0");
		return 1;
	}
	const std::string bytes(reinterpret_cast<const char*>(raw.data()), raw.size());
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%016llx",
		static_cast<unsigned long long>(fnv1a64(bytes)));
	lua_pushstring(L, buf);
	return 1;
}

// --- WorldToScreen ---
int l_worldtoscreen(lua_State* L)
{
	Vector3 world{};
	auto fail = [](lua_State* L) -> int {
		LuaTypes::PushVector2(L, 0.f, 0.f);
		lua_pushboolean(L, 0);
		return 2;
	};
	if (!LuaTypes::ToVector3(L, 1, world))
		return fail(L);
	if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
		return fail(L);
	std::shared_ptr<Instance> cam = Globals::Workspace->GetCurrentCamera();
	if (!cam || !g_Memory.IsValid(cam->address))
		return fail(L);

	Cheat::Camera c(cam->address);
	Vector2 scr{};
	const bool on = c.WorldToScreen(world, scr);
	LuaTypes::PushVector2(L, scr.x, scr.y);
	lua_pushboolean(L, on ? 1 : 0);
	return 2;
}

// --- input ---
// The external is a separate process (not injected), so SendInput only reaches
// Roblox when the game window is the foreground window. If the overlay/console is
// foreground, Roblox never gets the input (reel bar doesn't move). Focus the game
// before sending, but leave the foreground alone while the user drives the overlay.
void focus_game()
{
	HWND game = Renderer::GetGameHwnd();
	if (!game || !IsWindow(game))
		return;
	if (GetForegroundWindow() == game)
		return;
	ShowWindow(game, SW_RESTORE);
	SetForegroundWindow(game);
}

void send_key(int vk, bool down)
{
	focus_game();
	INPUT in{};
	in.type = INPUT_KEYBOARD;
	in.ki.wVk = static_cast<WORD>(vk);
	in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
	SendInput(1, &in, sizeof(in));
}

void send_mouse(DWORD flags)
{
	focus_game();
	INPUT in{};
	in.type = INPUT_MOUSE;
	in.mi.dwFlags = flags;
	SendInput(1, &in, sizeof(in));
}

// Translates a point given in the Roblox viewport's coordinate space (origin
// top-left, same space as WorldToScreen / Camera.ViewportSize) into real screen
// coordinates for SetCursorPos. The viewport fills the game window's client
// rect, so map through that window (fall back to the overlay hwnd if the game
// window isn't available yet), mirroring the aim/esp modules.
void viewport_to_screen(LONG& x, LONG& y)
{
	POINT pt{ x, y };
	HWND hwnd = Renderer::GetGameHwnd();
	if (!hwnd || !IsWindow(hwnd))
		hwnd = Renderer::GetHwnd();
	if (hwnd && IsWindow(hwnd))
		ClientToScreen(hwnd, &pt);
	x = pt.x;
	y = pt.y;
}

int l_setrobloxinput(lua_State* L)
{
	g_input_enabled = lua_toboolean(L, 1) != 0;
	return 0;
}

int l_keypress(lua_State* L)
{
	if (g_input_enabled)
		send_key(static_cast<int>(luaL_checkinteger(L, 1)), true);
	lua_pushboolean(L, 1);
	return 1;
}

int l_keyrelease(lua_State* L)
{
	if (g_input_enabled)
		send_key(static_cast<int>(luaL_checkinteger(L, 1)), false);
	lua_pushboolean(L, 1);
	return 1;
}

int l_ismouse1pressed(lua_State* L)
{
	lua_pushboolean(L, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0);
	return 1;
}

int l_ismouse2pressed(lua_State* L)
{
	lua_pushboolean(L, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 1 : 0);
	return 1;
}

int l_mouse1press(lua_State* L)
{
	if (g_input_enabled) send_mouse(MOUSEEVENTF_LEFTDOWN);
	return 0;
}

int l_mouse1release(lua_State* L)
{
	if (g_input_enabled) send_mouse(MOUSEEVENTF_LEFTUP);
	return 0;
}

int l_mouse1click(lua_State* L)
{
	if (g_input_enabled)
	{
		send_mouse(MOUSEEVENTF_LEFTDOWN);
		Sleep(kClickHoldMs);
		send_mouse(MOUSEEVENTF_LEFTUP);
	}
	return 0;
}

int l_mouse2press(lua_State* L)
{
	if (g_input_enabled) send_mouse(MOUSEEVENTF_RIGHTDOWN);
	return 0;
}

int l_mouse2release(lua_State* L)
{
	if (g_input_enabled) send_mouse(MOUSEEVENTF_RIGHTUP);
	return 0;
}

int l_mouse2click(lua_State* L)
{
	if (g_input_enabled)
	{
		send_mouse(MOUSEEVENTF_RIGHTDOWN);
		Sleep(kClickHoldMs);
		send_mouse(MOUSEEVENTF_RIGHTUP);
	}
	return 0;
}

int l_mousemoveabs(lua_State* L)
{
	// matcha: mousemoveabs takes Roblox viewport pixels (top-left origin, same
	// space as WorldToScreen / Camera.ViewportSize) — a WorldToScreen result can
	// be fed straight in. Convert those viewport coords to real screen coords
	// through the game window's client rect before warping the cursor.
	LONG x = static_cast<LONG>(luaL_checkinteger(L, 1));
	LONG y = static_cast<LONG>(luaL_checkinteger(L, 2));
	viewport_to_screen(x, y);
	SetCursorPos(x, y);
	return 0;
}

int l_mousemoverel(lua_State* L)
{
	const int dx = static_cast<int>(luaL_checkinteger(L, 1));
	const int dy = static_cast<int>(luaL_checkinteger(L, 2));
	mouse_event(MOUSEEVENTF_MOVE, dx, dy, 0, 0);
	return 0;
}

int l_mousescroll(lua_State* L)
{
	const int amount = static_cast<int>(luaL_checkinteger(L, 1));
	mouse_event(MOUSEEVENTF_WHEEL, 0, 0,
		static_cast<DWORD>(amount) * static_cast<DWORD>(WHEEL_DELTA), 0);
	return 0;
}

void RegisterCompatGlobals(lua_State* L)
{
	lua_pushcfunction(L, l_setclipboard);   lua_setglobal(L, "setclipboard");
	lua_pushcfunction(L, l_getclipboard);   lua_setglobal(L, "getclipboard");
	lua_pushcfunction(L, l_base64encode);   lua_setglobal(L, "base64encode");
	lua_pushcfunction(L, l_base64decode);   lua_setglobal(L, "base64decode");
	lua_pushcfunction(L, l_setfflag);       lua_setglobal(L, "setfflag");
	lua_pushcfunction(L, l_getfflag);       lua_setglobal(L, "getfflag");
	lua_pushcfunction(L, l_tick);           lua_setglobal(L, "tick");
	lua_pushcfunction(L, l_typeof);         lua_setglobal(L, "typeof");
	lua_pushcfunction(L, l_loadstring);     lua_setglobal(L, "loadstring");
	lua_pushcfunction(L, l_load);           lua_setglobal(L, "load");
	lua_pushcfunction(L, l_run_secure);     lua_setglobal(L, "run_secure");
	lua_pushcfunction(L, l_notify);         lua_setglobal(L, "notify");
	lua_pushcfunction(L, l_getgamename);    lua_setglobal(L, "getgamename");
	lua_pushcfunction(L, l_getgamename);    lua_setglobal(L, "getgetname");
	lua_pushcfunction(L, l_getscripthash);  lua_setglobal(L, "getscripthash");
	lua_pushcfunction(L, l_worldtoscreen);  lua_setglobal(L, "WorldToScreen");

	lua_pushcfunction(L, l_setrobloxinput); lua_setglobal(L, "setrobloxinput");
	lua_pushcfunction(L, l_keypress);       lua_setglobal(L, "keypress");
	lua_pushcfunction(L, l_keyrelease);     lua_setglobal(L, "keyrelease");
	lua_pushcfunction(L, l_ismouse1pressed);lua_setglobal(L, "ismouse1pressed");
	lua_pushcfunction(L, l_ismouse2pressed);lua_setglobal(L, "ismouse2pressed");
	lua_pushcfunction(L, l_mouse1press);    lua_setglobal(L, "mouse1press");
	lua_pushcfunction(L, l_mouse1release);  lua_setglobal(L, "mouse1release");
	lua_pushcfunction(L, l_mouse1click);    lua_setglobal(L, "mouse1click");
	lua_pushcfunction(L, l_mouse2press);    lua_setglobal(L, "mouse2press");
	lua_pushcfunction(L, l_mouse2release);  lua_setglobal(L, "mouse2release");
	lua_pushcfunction(L, l_mouse2click);    lua_setglobal(L, "mouse2click");
	lua_pushcfunction(L, l_mousemoveabs);   lua_setglobal(L, "mousemoveabs");
	lua_pushcfunction(L, l_mousemoverel);   lua_setglobal(L, "mousemoverel");
	lua_pushcfunction(L, l_mousescroll);    lua_setglobal(L, "mousescroll");
}

std::unordered_set<const void*> g_ro_tables;

// --- environment functions (ported from the old luavm env) ---
int l_getgenv(lua_State* L)
{
	lua_settop(L, 0);
	lua_pushglobaltable(L);
	return 1;
}

int l_getrenv(lua_State* L)
{
	return l_getgenv(L);
}

int l_getfenv(lua_State* L)
{
	if (lua_isfunction(L, 1) && lua_getupvalue(L, 1, 1))
		return 1;
	lua_settop(L, 0);
	lua_pushglobaltable(L);
	return 1;
}

int l_setfenv(lua_State* L)
{
	if (!lua_isfunction(L, 1) || !lua_istable(L, 2))
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushvalue(L, 2);
	if (!lua_setupvalue(L, 1, 1))
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushvalue(L, 1);
	return 1;
}

int l_getrawmetatable(lua_State* L)
{
	if (!lua_getmetatable(L, 1))
		lua_pushnil(L);
	return 1;
}

int l_setrawmetatable(lua_State* L)
{
	lua_setmetatable(L, 1);
	return 0;
}

int l_setreadonly(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	if (lua_toboolean(L, 2))
		g_ro_tables.insert(lua_topointer(L, 1));
	else
		g_ro_tables.erase(lua_topointer(L, 1));
	return 0;
}

int l_isreadonly(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_pushboolean(L, g_ro_tables.count(lua_topointer(L, 1)) ? 1 : 0);
	return 1;
}

int l_makereadonly(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	g_ro_tables.insert(lua_topointer(L, 1));
	return 0;
}

int l_makewriteable(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	g_ro_tables.erase(lua_topointer(L, 1));
	return 0;
}

int l_newcclosure(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_pushvalue(L, 1);
	return 1;
}

int l_clonefunction(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_pushvalue(L, 1);
	return 1;
}

int l_iscclosure(lua_State* L)
{
	lua_pushboolean(L, lua_iscfunction(L, 1) ? 1 : 0);
	return 1;
}

int l_islclosure(lua_State* L)
{
	lua_pushboolean(L, (lua_isfunction(L, 1) && !lua_iscfunction(L, 1)) ? 1 : 0);
	return 1;
}

int l_checkcaller(lua_State* L)
{
	(void)L;
	lua_pushboolean(L, 1);
	return 1;
}

int l_getexecutorname(lua_State* L)
{
	lua_pushstring(L, "Ardvark");
	return 1;
}

int l_cloneref(lua_State* L)
{
	lua_pushvalue(L, 1);
	return 1;
}

int l_gethui(lua_State* L)
{
	(void)L;
	lua_pushnil(L);
	return 1;
}

int l_getnamecallmethod(lua_State* L)
{
	(void)L;
	lua_pushnil(L);
	return 1;
}

int l_setnamecallmethod(lua_State* L)
{
	return 0;
}

void RegisterEnvGlobals(lua_State* L)
{
	lua_pushcfunction(L, l_getgenv);          lua_setglobal(L, "getgenv");
	lua_pushcfunction(L, l_getrenv);          lua_setglobal(L, "getrenv");
	lua_pushcfunction(L, l_getfenv);          lua_setglobal(L, "getfenv");
	lua_pushcfunction(L, l_setfenv);          lua_setglobal(L, "setfenv");
	lua_pushcfunction(L, l_getrawmetatable);  lua_setglobal(L, "getrawmetatable");
	lua_pushcfunction(L, l_setrawmetatable);  lua_setglobal(L, "setrawmetatable");
	lua_pushcfunction(L, l_setreadonly);      lua_setglobal(L, "setreadonly");
	lua_pushcfunction(L, l_isreadonly);       lua_setglobal(L, "isreadonly");
	lua_pushcfunction(L, l_makereadonly);     lua_setglobal(L, "makereadonly");
	lua_pushcfunction(L, l_makewriteable);    lua_setglobal(L, "makewriteable");
	lua_pushcfunction(L, l_newcclosure);      lua_setglobal(L, "newcclosure");
	lua_pushcfunction(L, l_clonefunction);    lua_setglobal(L, "clonefunction");
	lua_pushcfunction(L, l_iscclosure);       lua_setglobal(L, "iscclosure");
	lua_pushcfunction(L, l_islclosure);       lua_setglobal(L, "islclosure");
	lua_pushcfunction(L, l_checkcaller);      lua_setglobal(L, "checkcaller");
	lua_pushcfunction(L, l_getexecutorname);  lua_setglobal(L, "getexecutorname");
	lua_pushcfunction(L, l_cloneref);         lua_setglobal(L, "cloneref");
	lua_pushcfunction(L, l_gethui);           lua_setglobal(L, "gethui");
	lua_pushcfunction(L, l_getnamecallmethod);lua_setglobal(L, "getnamecallmethod");
	lua_pushcfunction(L, l_setnamecallmethod);lua_setglobal(L, "setnamecallmethod");
}

void RegisterApi(lua_State* L)
{
	lua_pushcfunction(L, l_print);
	lua_setglobal(L, "print");
	lua_pushcfunction(L, l_warn);
	lua_setglobal(L, "warn");
	lua_pushcfunction(L, l_identifyexecutor);
	lua_setglobal(L, "identifyexecutor");
	lua_pushcfunction(L, l_wait);
	lua_setglobal(L, "wait");
	lua_pushcfunction(L, l_spawn);
	lua_setglobal(L, "spawn");
	lua_pushcfunction(L, l_getmousepos);
	lua_setglobal(L, "getmousepos");
	lua_pushcfunction(L, l_isrbxactive);
	lua_setglobal(L, "isrbxactive");
	lua_pushcfunction(L, l_iskeypressed);
	lua_setglobal(L, "iskeypressed");

	RegisterCompatGlobals(L);
	RegisterEnvGlobals(L);

	luaL_newmetatable(L, "jewsploit.Task");
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushcfunction(L, l_wait);
	lua_setfield(L, -2, "wait");
	lua_pushcfunction(L, l_spawn);
	lua_setfield(L, -2, "spawn");
	lua_pushcfunction(L, l_delay);
	lua_setfield(L, -2, "delay");
	lua_pushcfunction(L, l_defer);
	lua_setfield(L, -2, "defer");
	lua_pushcfunction(L, l_cancel);
	lua_setfield(L, -2, "cancel");
	lua_setglobal(L, "task");

	LuaTypes::Register(L);
	LuaDrawing::Register(L);
	LuaScripts::Register(L);
	LuaGc::Register(L);
	LuaMem::Register(L);
	LuaFiles::Register(L);
	LuaBridge::Register(L);
	RegisterRunService(L);
	RegisterUserInputService(L);
	RegisterBit32(L);
	RegisterTweenService(L);
	RegisterHttp(L);
}

} // namespace

void PushSignal(lua_State* L, int kind, std::uint64_t owner, const char* prop)
{
	push_signal(L, kind, owner, prop);
}

void FireSignal(lua_State* L, int kind, std::uint64_t owner, const char* prop)
{
	(void)L;
	(void)kind;
	(void)owner;
	(void)prop;
}

void ScheduleWait(lua_State* L, float sec)
{
	schedule_wait(L, sec);
}

void TickLoop()
{
	using clock = std::chrono::steady_clock;
	auto last = clock::now();

	while (g_tick_run.load())
	{
		auto now = clock::now();
		float dt = std::chrono::duration<float>(now - last).count();
		last = now;
		if (dt < 0.f)
			dt = 0.f;
		if (dt > 0.1f)
			dt = 0.1f;

		Tick(dt);
		int ms = (int)g_Settings.lua.ticks_ms;
		if (ms < 1) ms = 1;
		if (ms > 15) ms = 15;
		Sleep(ms); // configurable: coroutine pump interval (15ms = ~60fps default)
	}
}

bool Initialize()
{
	if (g_L)
		return true;

	g_L = luaL_newstate();
	if (!g_L)
		return false;

	OpenSafeLibs(g_L);
	RegisterApi(g_L);

	// wait resume Ð½Ðµ Ð½Ð° render â€” Ð¸Ð½Ð°Ñ‡Ðµ hold/findgc Ð´ÑƒÑˆÐ°Ñ‚ esp
	if (!g_tick_run.load())
	{
		g_tick_run.store(true);
		g_tick_th = std::thread(TickLoop);
	}

	return true;
}

void Shutdown()
{
	g_tick_run.store(false);
	if (g_tick_th.joinable())
		g_tick_th.join();

	for (int i = 0; i < 200 && g_busy.load(); ++i)
		Sleep(10);

	std::lock_guard<std::mutex> lock(g_lua_mu);
	LuaDrawing::Clear();

	if (!g_L)
		return;

	for (auto& w : g_waits)
	{
		if (w.ref != LUA_NOREF)
			luaL_unref(g_L, LUA_REGISTRYINDEX, w.ref);
	}
	g_waits.clear();

	for (auto& c : g_conns)
	{
		if (c.fn_ref != LUA_NOREF)
			luaL_unref(g_L, LUA_REGISTRYINDEX, c.fn_ref);
		c.fn_ref = LUA_NOREF;
		c.alive = false;
	}
	g_conns.clear();

	for (auto& w : g_sigwaits)
	{
		if (w.thr_ref != LUA_NOREF)
			luaL_unref(g_L, LUA_REGISTRYINDEX, w.thr_ref);
		w.thr_ref = LUA_NOREF;
		w.alive = false;
	}
	g_sigwaits.clear();

	for (auto& d : g_delays)
	{
		if (d.fn_ref != LUA_NOREF)
			luaL_unref(g_L, LUA_REGISTRYINDEX, d.fn_ref);
		d.fn_ref = LUA_NOREF;
		d.alive = false;
	}
	g_delays.clear();

	g_plr_seeded = false;
	g_seen_plr.clear();
	g_seen_char.clear();
	g_input_seeded = false;
	std::memset(g_key_down, 0, sizeof(g_key_down));
	g_kids_snap.clear();
	g_desc_snap.clear();
	g_prop_snap.clear();
	g_prop_cursor = 0;
	g_bus_seq_addr = 0;
	g_bus_pay_addr = 0;
	g_bus_last_seq = 0;
	g_bus_seeded = false;
	g_poll_tick = 0;

	lua_close(g_L);
	g_L = nullptr;
}

lua_State* State()
{
	return g_L;
}

bool Ready()
{
	return g_L != nullptr;
}

void wipe_script_signals()
{
	if (!g_L)
		return;

	// ÐºÐ°Ð¶Ð´Ñ‹Ð¹ execute â€” Ð¸Ð½Ð°Ñ‡Ðµ Connect ÐºÐ¾Ð¿Ð¸Ñ‚ÑÑ Ð¸ server_tick x3
	for (auto& c : g_conns)
	{
		if (c.fn_ref != LUA_NOREF)
			luaL_unref(g_L, LUA_REGISTRYINDEX, c.fn_ref);
		c.fn_ref = LUA_NOREF;
		c.alive = false;
	}
	g_conns.clear();

	for (auto& w : g_sigwaits)
	{
		if (w.thr_ref != LUA_NOREF)
			luaL_unref(g_L, LUA_REGISTRYINDEX, w.thr_ref);
		w.thr_ref = LUA_NOREF;
		w.alive = false;
	}
	g_sigwaits.clear();

	// ÐºÐ¾Ñ€ÑƒÑ‚Ð¸Ð½Ñ‹ Ð¿Ñ€Ð¾ÑˆÐ»Ð¾Ð³Ð¾ Ð·Ð°Ð¿ÑƒÑÐºÐ° Ð¸Ð½Ð°Ñ‡Ðµ Ð¶Ð¸Ð²ÑƒÑ‚ Ð²ÐµÑ‡Ð½Ð¾: Ð¸Ñ… ref Ð² Ñ€ÐµÐµÑÑ‚Ñ€Ðµ Ð½Ðµ Ð´Ð°Ñ‘Ñ‚
	// gc ÑÐ¾Ð±Ñ€Ð°Ñ‚ÑŒ Ñ‚Ñ€ÐµÐ´, Ð° Ñ‚Ð¸ÐºÐµÑ€ Ð¿Ñ€Ð¾Ð´Ð¾Ð»Ð¶Ð°ÐµÑ‚ Ð¸Ñ… ÐºÑ€ÑƒÑ‚Ð¸Ñ‚ÑŒ
	for (auto& w : g_waits)
	{
		if (w.ref != LUA_NOREF)
			luaL_unref(g_L, LUA_REGISTRYINDEX, w.ref);
	}
	g_waits.clear();

	for (auto& d : g_delays)
	{
		if (d.fn_ref != LUA_NOREF)
			luaL_unref(g_L, LUA_REGISTRYINDEX, d.fn_ref);
		d.fn_ref = LUA_NOREF;
		d.alive = false;
	}
	g_delays.clear();

	g_kids_snap.clear();
	g_desc_snap.clear();
	g_prop_snap.clear();
	g_prop_cursor = 0;
	g_bus_seq_addr = 0;
	g_bus_pay_addr = 0;
	g_bus_seeded = false;
	lua_gc(g_L, LUA_GCCOLLECT, 0);
}

// Called with g_lua_mu held. Stops every running/yielded script thread, drops
// all Drawing objects, and resets the per-execution poll caches so the VM is
// a clean slate for the next script.
void ResetLocked()
{
	if (!g_L)
		return;

	// waits/conns/sigwaits/delays + snapshot/bus caches + a gc collect
	wipe_script_signals();

	// drop on-screen drawings (fish bar / debug boxes esp.)
	LuaDrawing::Clear();

	// reset poll "seed"/seen caches so the new script re-fires signals cleanly
	g_plr_seeded = false;
	g_seen_plr.clear();
	g_seen_char.clear();
	g_input_seeded = false;
	std::memset(g_key_down, 0, sizeof(g_key_down));
	g_poll_tick = 0;
}

void Restart()
{
	std::unique_lock<std::mutex> lock(g_lua_mu, std::try_to_lock);
	if (lock.owns_lock())
	{
		ResetLocked();
		LogInfo("[Executor] Restarted — ready for a new script");
	}
	else
	{
		// a script is mid-run on the execute thread: apply on its next yield
		g_restart_pending.store(true);
		LogInfo("[Executor] Next script still running — Restart will apply when it yields");
	}
}

void ExecuteLocked(const std::string& source, const std::string& name)
{
	if (!g_L)
	{
		LogErr("lua vm init failed");
		return;
	}

	wipe_script_signals();
	LuaBridge::RefreshGlobals(g_L);

	const std::string processed = LuaPreprocess::Preprocess(source);
	if (LuaLoadBufferCatch(g_L, processed.data(), processed.size(), name.c_str()) != LUA_OK)
	{
		LogErr(lua_tostring(g_L, -1));
		lua_pop(g_L, 1);
		return;
	}

	lua_State* co = lua_newthread(g_L);
	lua_pushvalue(g_L, -2);
	lua_xmove(g_L, co, 1);
	lua_pop(g_L, 1);

	int nres = 0;
	const int status = LuaResumeCatch(co, g_L, 0, &nres);
	if (status == LUA_OK)
	{
		lua_pop(g_L, 1);
		LuaExecutor::Log(LuaExecutor::LogLevel::Success, "ok");
		return;
	}

	if (status == LUA_YIELD)
	{
		lua_pop(g_L, 1);
		return;
	}

	LogErr(lua_tostring(co, -1));
	lua_pop(co, 1);
	lua_pop(g_L, 1);
}

bool Execute(const std::string& source, const char* chunk_name)
{
	if (!Initialize())
	{
		LogErr("lua vm init failed");
		return false;
	}

	bool expected = false;
	if (!g_busy.compare_exchange_strong(expected, true))
	{
		LogErr("script already running");
		return false;
	}

	std::string src = source;
	std::string name = chunk_name ? chunk_name : "script";

	std::thread([src = std::move(src), name = std::move(name)]()
	{
		{
			std::lock_guard<std::mutex> lock(g_lua_mu);
			ExecuteLocked(src, name);
		}
		g_busy.store(false);
	}).detach();

	return true;
}

void Tick(float dt)
{
	std::unique_lock<std::mutex> lock(g_lua_mu, std::try_to_lock);
	if (!lock.owns_lock())
		return;

	if (!g_L)
		return;

	if (g_restart_pending.exchange(false))
	{
		ResetLocked();
		LogInfo("[Executor] Restart applied");
	}

	++g_poll_tick;

	poll_players();
	poll_input();
	poll_hierarchy();
	if ((g_poll_tick % 2) == 0)
		poll_props();
	poll_jp_bus();

	// fake Heartbeat / RenderStepped / Stepped
	{
		lua_pushnumber(g_L, dt);
		fire_sig(0, 0, nullptr, 1);
	}

	// task.delay / defer â€” ÐºÐ¾Ð»Ð±ÑÐº Ð¼Ð¾Ð¶ÐµÑ‚ Ð·Ð²Ð°Ñ‚ÑŒ task.delay, Ð²ÐµÐºÑ‚Ð¾Ñ€ Ð¿ÐµÑ€ÐµÐµÐ´ÐµÑ‚
	const size_t ndelay = g_delays.size();
	for (size_t i = 0; i < ndelay && i < g_delays.size(); ++i)
	{
		if (!g_delays[i].alive || g_delays[i].fn_ref == LUA_NOREF)
			continue;

		g_delays[i].left -= dt;
		if (g_delays[i].left > 0.f)
			continue;

		const int fn = g_delays[i].fn_ref;
		g_delays[i].alive = false;
		g_delays[i].fn_ref = LUA_NOREF;
		lua_rawgeti(g_L, LUA_REGISTRYINDEX, fn);
		luaL_unref(g_L, LUA_REGISTRYINDEX, fn);
		if (lua_pcall(g_L, 0, 0, 0) != LUA_OK)
		{
			LogErr(lua_tostring(g_L, -1));
			lua_pop(g_L, 1);
		}
	}

	if (g_waits.empty())
		return;

	// cap resumes/frame so wait(0) loops cannot freeze the UI thread
	constexpr int k_max_resumes = 64;

	// ÑÐ½Ð°Ñ‡Ð°Ð»Ð° Ð²Ñ‹Ð´Ñ‘Ñ€Ð³Ð¸Ð²Ð°ÐµÐ¼ Ð³Ð¾Ñ‚Ð¾Ð²Ñ‹Ñ…, Ð¿Ð¾Ñ‚Ð¾Ð¼ Ñ€ÐµÐ·ÑŽÐ¼Ð¸Ð¼: resume Ð²Ð½ÑƒÑ‚Ñ€Ð¸ Ñ†Ð¸ÐºÐ»Ð°
	// Ð´Ð¾Ð±Ð°Ð²Ð»ÑÐµÑ‚ Ð½Ð¾Ð²Ñ‹Ðµ wait'Ñ‹ Ð¸ Ð»Ð¾Ð¼Ð°ÐµÑ‚ Ð¸Ð½Ð´ÐµÐºÑÑ‹
	static std::vector<int> ready;
	ready.clear();

	for (size_t i = 0; i < g_waits.size();)
	{
		g_waits[i].left -= dt;
		if (g_waits[i].left > 0.f)
		{
			++i;
			continue;
		}

		if (static_cast<int>(ready.size()) >= k_max_resumes)
		{
			g_waits[i].left = 0.f; // retry next frame
			++i;
			continue;
		}

		ready.push_back(g_waits[i].ref);
		g_waits.erase(g_waits.begin() + static_cast<std::ptrdiff_t>(i));
	}

	for (int ref : ready)
	{
		lua_rawgeti(g_L, LUA_REGISTRYINDEX, ref); // keep thread alive on stack
		lua_State* co = lua_tothread(g_L, -1);
		luaL_unref(g_L, LUA_REGISTRYINDEX, ref);

		if (!co)
		{
			lua_pop(g_L, 1);
			continue;
		}

		int nres = 0;
		const int status = LuaResumeCatch(co, g_L, 0, &nres);
		if (status == LUA_OK)
		{
			LuaExecutor::Log(LuaExecutor::LogLevel::Success, "ok");
		}

		else if (status != LUA_YIELD)
		{
			LogErr(lua_tostring(co, -1));
			lua_pop(co, 1);
		}

		lua_pop(g_L, 1);
	}
}

} // namespace LuaVM
} // namespace Features
} // namespace Cheat
