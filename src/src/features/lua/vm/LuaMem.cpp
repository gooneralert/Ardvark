#include "pch.h"
#include "LuaMem.h"
#include "CallGate.h"
#include "InstanceCreate.h"
#include "Reflect.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaMem {
namespace {

constexpr const char* k_inst_mt = "jewsploit.Instance";

std::uintptr_t Base()
{
	return g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
}

bool Plausible(std::uint64_t a)
{
	return a >= 0x10000 && a < 0x7FFFFFFFFFFFull;
}

// адрес берём либо числом, либо из userdata инстанса
std::uint64_t Addr(lua_State* L, int idx)
{
	if (lua_isnumber(L, idx))
		return (std::uint64_t)luaL_checkinteger(L, idx);

	void* ud = luaL_testudata(L, idx, k_inst_mt);
	if (!ud)
		return 0;

	return *static_cast<std::uint64_t*>(ud);
}

template <typename T>
int ReadAs(lua_State* L)
{
	const auto a = Addr(L, 1);
	if (!Plausible(a))
	{
		lua_pushnil(L);
		return 1;
	}

	T v{};
	if (g_Memory.ReadRaw((std::uintptr_t)a, &v, sizeof(T)) != sizeof(T))
	{
		lua_pushnil(L);
		return 1;
	}

	if constexpr (std::is_floating_point_v<T>)
		lua_pushnumber(L, (lua_Number)v);
	else
		lua_pushinteger(L, (lua_Integer)v);

	return 1;
}

template <typename T>
int WriteAs(lua_State* L)
{
	const auto a = Addr(L, 1);
	if (!Plausible(a))
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	T v{};
	if constexpr (std::is_floating_point_v<T>)
		v = (T)luaL_checknumber(L, 2);
	else
		v = (T)luaL_checkinteger(L, 2);

	const bool ok =
		g_Memory.WriteRaw((std::uintptr_t)a, &v, sizeof(T)) == sizeof(T);
	lua_pushboolean(L, ok ? 1 : 0);
	return 1;
}

int l_readcstr(lua_State* L)
{
	const auto a = Addr(L, 1);
	auto max = (int)luaL_optinteger(L, 2, 64);
	if (max < 1)
		max = 1;
	if (max > 512)
		max = 512;

	if (!Plausible(a))
	{
		lua_pushnil(L);
		return 1;
	}

	char buf[513]{};
	const auto got = g_Memory.ReadRaw((std::uintptr_t)a, buf, (std::size_t)max);
	if (!got)
	{
		lua_pushnil(L);
		return 1;
	}

	buf[max] = '\0';
	lua_pushstring(L, buf);
	return 1;
}

// std::string, а не сырой char*: короткие строки лежат в самом объекте
int l_readstdstr(lua_State* L)
{
	const auto a = Addr(L, 1);
	if (!Plausible(a))
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, g_Memory.ReadString((std::uintptr_t)a).c_str());
	return 1;
}

int l_writestdstr(lua_State* L)
{
	const auto a = Addr(L, 1);
	const char* text = luaL_checkstring(L, 2);
	const bool ok = Plausible(a) && text &&
		InstanceCreate::SetString(a, text);

	lua_pushboolean(L, ok ? 1 : 0);
	return 1;
}

// Content держит разобранную схему ссылки в кеше рядом со строкой,
// поэтому после подмены текста его надо сбросить, иначе движок
// продолжит искать ассет по старой схеме
int l_writecontent(lua_State* L)
{
	const auto a = Addr(L, 1);
	const char* text = luaL_checkstring(L, 2);
	const bool ok = Plausible(a) && text &&
		InstanceCreate::SetContent(a, text);

	lua_pushboolean(L, ok ? 1 : 0);
	return 1;
}

// вызов произвольной функции движка на его собственном потоке
int l_gatecall(lua_State* L)
{
	const auto fn = (std::uint64_t)luaL_checkinteger(L, 1);
	const auto a0 = (std::uint64_t)luaL_optinteger(L, 2, 0);
	const auto a1 = (std::uint64_t)luaL_optinteger(L, 3, 0);
	const auto a2 = (std::uint64_t)luaL_optinteger(L, 4, 0);
	const auto a3 = (std::uint64_t)luaL_optinteger(L, 5, 0);

	if (!Plausible(fn))
	{
		lua_pushnil(L);
		lua_pushstring(L, "bad function address");
		return 2;
	}

	if (!CallGate::Ready() && !CallGate::Install())
	{
		lua_pushnil(L);
		lua_pushstring(L, "no call gate");
		return 2;
	}

	std::uint64_t ret = 0;
	if (!CallGate::Invoke(fn, a0, a1, a2, a3, &ret))
	{
		lua_pushnil(L);
		lua_pushstring(L, "gate timeout");
		return 2;
	}

	lua_pushinteger(L, (lua_Integer)ret);
	return 1;
}

int l_gatescratch(lua_State* L)
{
	lua_pushinteger(L, (lua_Integer)CallGate::Scratch());
	return 1;
}

int l_rbxbase(lua_State* L)
{
	lua_pushinteger(L, (lua_Integer)Base());
	return 1;
}

int l_addrof(lua_State* L)
{
	lua_pushinteger(L, (lua_Integer)Addr(L, 1));
	return 1;
}

int l_refname(lua_State* L)
{
	const char* s = luaL_checkstring(L, 1);
	const auto base = Base();
	if (!base || !s)
	{
		lua_pushnil(L);
		return 1;
	}

	// RBX::Name interning was removed along with the Reflection tables — disabled.
	(void)s;
	(void)base;
	lua_pushnil(L);
	return 1;
}

int l_refcreator(lua_State* L)
{
	const auto base = Base();
	if (!base)
	{
		lua_pushnil(L);
		return 1;
	}

	// class-name string -> ICreator via the Creators map (guide "Instance.new").
	if (lua_isstring(L, 1) && !lua_isnumber(L, 1))
	{
		const auto c = Reflect::CreatorByName(base, lua_tostring(L, 1));
		if (!c)
			lua_pushnil(L);
		else
			lua_pushinteger(L, (lua_Integer)c);
		return 1;
	}

	// integer (interned-name) path needs the Reflection tables, which are gone.
	lua_pushnil(L);
	return 1;
}

int l_classdesc(lua_State* L)
{
	const auto inst = Addr(L, 1);
	if (!Plausible(inst))
	{
		lua_pushnil(L);
		return 1;
	}

	const auto cd = g_Memory.Read<std::uint64_t>(
		(std::uintptr_t)inst + ::Instance::ClassDescriptor);
	if (!Plausible(cd))
		lua_pushnil(L);
	else
		lua_pushinteger(L, (lua_Integer)cd);

	return 1;
}

// ищет qword needle в [addr, addr+span) и возвращает список смещений
int l_scanptr(lua_State* L)
{
	const auto a = Addr(L, 1);
	const auto needle = (std::uint64_t)luaL_checkinteger(L, 2);
	auto span = (int)luaL_optinteger(L, 3, 0x400);
	if (span < 8)
		span = 8;
	if (span > 0x10000)
		span = 0x10000;

	lua_newtable(L);
	if (!Plausible(a) || !needle)
		return 1;

	std::vector<std::uint8_t> buf((std::size_t)span);
	const auto got =
		g_Memory.ReadRaw((std::uintptr_t)a, buf.data(), (std::size_t)span);
	if (!got)
		return 1;

	int n = 0;
	for (std::size_t off = 0; off + 8 <= got; off += 8)
	{
		std::uint64_t v = 0;
		std::memcpy(&v, buf.data() + off, 8);
		if (v != needle)
			continue;

		lua_pushinteger(L, (lua_Integer)off);
		lua_rawseti(L, -2, ++n);
	}

	return 1;
}

int l_get_class(lua_State* L)
{
	const auto a = Addr(L, 1);
	if (!Plausible(a))
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, Instance(a).GetClassName().c_str());
	return 1;
}

int l_get_name(lua_State* L)
{
	const auto a = Addr(L, 1);
	if (!Plausible(a))
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, Instance(a).GetName().c_str());
	return 1;
}

void Set(lua_State* L, const char* name, lua_CFunction fn)
{
	lua_pushcfunction(L, fn);
	lua_setglobal(L, name);
}

void RegisterNamespace(lua_State* L)
{
	struct Entry
	{
		const char* name;
		lua_CFunction fn;
	};

	const Entry entries[] = {
		{ "rdq", ReadAs<std::uint64_t> },
		{ "rdd", ReadAs<std::uint32_t> },
		{ "rdw", ReadAs<std::uint16_t> },
		{ "rdb", ReadAs<std::uint8_t> },
		{ "rdf", ReadAs<float> },
		{ "rdcstr", l_readcstr },
		{ "rdstr", l_readstdstr },

		{ "wrq", WriteAs<std::uint64_t> },
		{ "wrd", WriteAs<std::uint32_t> },
		{ "wrw", WriteAs<std::uint16_t> },
		{ "wrb", WriteAs<std::uint8_t> },
		{ "wrf", WriteAs<float> },
		{ "wrstr", l_writestdstr },
		{ "wrcontent", l_writecontent },

		{ "gatecall", l_gatecall },
		{ "gatescratch", l_gatescratch },

		{ "rbxbase", l_rbxbase },
		{ "addrof", l_addrof },
		{ "refname", l_refname },
		{ "refcreator", l_refcreator },
		{ "classdesc", l_classdesc },
		{ "scanptr", l_scanptr },
		{ "get_class", l_get_class },
		{ "get_name", l_get_name },
	};

	lua_createtable(L, 0, (int)(sizeof(entries) / sizeof(entries[0])));
	for (const auto& e : entries)
	{
		lua_pushcfunction(L, e.fn);
		lua_setfield(L, -2, e.name);
	}
	lua_setglobal(L, "mem");
}

} // namespace

void Register(lua_State* L)
{
	Set(L, "rdq", ReadAs<std::uint64_t>);
	Set(L, "rdd", ReadAs<std::uint32_t>);
	Set(L, "rdw", ReadAs<std::uint16_t>);
	Set(L, "rdb", ReadAs<std::uint8_t>);
	Set(L, "rdf", ReadAs<float>);
	Set(L, "rdcstr", l_readcstr);
	Set(L, "rdstr", l_readstdstr);

	Set(L, "wrq", WriteAs<std::uint64_t>);
	Set(L, "wrd", WriteAs<std::uint32_t>);
	Set(L, "wrw", WriteAs<std::uint16_t>);
	Set(L, "wrb", WriteAs<std::uint8_t>);
	Set(L, "wrf", WriteAs<float>);
	Set(L, "wrstr", l_writestdstr);
	Set(L, "wrcontent", l_writecontent);

	Set(L, "gatecall", l_gatecall);
	Set(L, "gatescratch", l_gatescratch);

	Set(L, "rbxbase", l_rbxbase);
	Set(L, "addrof", l_addrof);
	Set(L, "refname", l_refname);
	Set(L, "refcreator", l_refcreator);
	Set(L, "classdesc", l_classdesc);
	Set(L, "scanptr", l_scanptr);

	Set(L, "get_class", l_get_class);
	Set(L, "get_name", l_get_name);

	RegisterNamespace(L);
}

} // namespace LuaMem
} // namespace Features
} // namespace Cheat
