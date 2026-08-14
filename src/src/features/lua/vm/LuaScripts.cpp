#include "pch.h"
#include "LuaScripts.h"
#include "LuaBridge.h"
#include "ScriptBytecode.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"

#include <string>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaScripts {
namespace {

constexpr size_t k_max_scripts = 8000;
constexpr size_t k_max_nodes = 120000;

void CollectScripts(std::uint64_t root, std::vector<std::uint64_t>& out, size_t& nodes)
{
	if (!g_Memory.IsValid(root) || nodes >= k_max_nodes || out.size() >= k_max_scripts)
		return;

	++nodes;
	Instance inst(root);
	const std::string cls = inst.GetClassName();
	if (ScriptBytecode::IsScriptClass(cls))
		out.push_back(root);

	for (const auto& c : inst.GetChildren())
	{
		if (out.size() >= k_max_scripts || nodes >= k_max_nodes)
			break;
		CollectScripts(c.address, out, nodes);
	}
}

int l_getscripts(lua_State* L)
{
	std::vector<std::uint64_t> scripts;
	size_t nodes = 0;

	std::uint64_t root = 0;
	if (lua_gettop(L) >= 1 && !lua_isnil(L, 1))
	{
		root = LuaBridge::CheckAddress(L, 1);
	}
	else if (g_Memory.IsValid(Globals::InstanceDataModel.address))
	{
		root = Globals::InstanceDataModel.address;
	}

	if (root)
		CollectScripts(root, scripts, nodes);

	lua_createtable(L, static_cast<int>(scripts.size()), 0);
	for (size_t i = 0; i < scripts.size(); ++i)
	{
		LuaBridge::PushInstance(L, scripts[i]);
		lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
	}
	return 1;
}

bool ResolveScript(lua_State* L, int idx, std::uint64_t& addr, std::string& cls)
{
	addr = LuaBridge::CheckAddress(L, idx);
	if (!g_Memory.IsValid(addr))
		return false;
	Instance inst(addr);
	cls = inst.GetClassName();
	return ScriptBytecode::IsScriptClass(cls);
}

int l_getscriptbytecode(lua_State* L)
{
	std::uint64_t addr = 0;
	std::string cls;
	if (!ResolveScript(L, 1, addr, cls))
	{
		lua_pushstring(L, "");
		return 1;
	}

	std::vector<std::uint8_t> raw;
	if (!ScriptBytecode::ReadRaw(addr, cls, raw) || raw.empty())
	{
		lua_pushstring(L, "");
		return 1;
	}

	lua_pushlstring(L, reinterpret_cast<const char*>(raw.data()), raw.size());
	return 1;
}

int l_decompile(lua_State* L)
{
	std::vector<std::uint8_t> raw;
	std::string name = "script";

	if (lua_type(L, 1) == LUA_TSTRING)
	{
		size_t len = 0;
		const char* s = lua_tolstring(L, 1, &len);
		raw.assign(reinterpret_cast<const std::uint8_t*>(s), reinterpret_cast<const std::uint8_t*>(s) + len);
		name = "bytecode";
	}
	else
	{
		std::uint64_t addr = 0;
		std::string cls;
		if (!ResolveScript(L, 1, addr, cls))
		{
			lua_pushstring(L, "-- decompile: not a script\n");
			return 1;
		}
		Instance inst(addr);
		name = inst.GetName();
		if (!ScriptBytecode::ReadRaw(addr, cls, raw) || raw.empty())
		{
			lua_pushstring(L, "-- decompile: no bytecode\n");
			return 1;
		}
	}

	const std::string src = ScriptBytecode::Decompile(raw, name.c_str());
	lua_pushlstring(L, src.data(), src.size());
	return 1;
}

int l_getsenv(lua_State* L)
{
	// external: no real script env — empty table stub
	(void)L;
	lua_newtable(L);
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
		{ "get", l_getscripts },
		{ "bytecode", l_getscriptbytecode },
		{ "decompile", l_decompile },
		{ "env", l_getsenv },
	};

	lua_createtable(L, 0, (int)(sizeof(entries) / sizeof(entries[0])));
	for (const auto& e : entries)
	{
		lua_pushcfunction(L, e.fn);
		lua_setfield(L, -2, e.name);
	}
	lua_setglobal(L, "scripts");
}

} // namespace

void Register(lua_State* L)
{
	lua_pushcfunction(L, l_getscripts);
	lua_setglobal(L, "getscripts");
	lua_pushcfunction(L, l_getscriptbytecode);
	lua_setglobal(L, "getscriptbytecode");
	lua_pushcfunction(L, l_decompile);
	lua_setglobal(L, "decompile");
	lua_pushcfunction(L, l_getsenv);
	lua_setglobal(L, "getsenv");

	RegisterNamespace(L);
}

} // namespace LuaScripts
} // namespace Features
} // namespace Cheat
