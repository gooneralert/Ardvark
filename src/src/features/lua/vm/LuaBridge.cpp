#include "pch.h"
#include "LuaBridge.h"
#include "LuaTypes.h"
#include "LuaVM.h"
#include "InstanceCreate.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/math/Math.h"
#include "core/roblox/offsets/Offsets.h"
#include "renderer/Renderer.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaBridge {
namespace {

constexpr const char* k_mt = "jewsploit.Instance";

struct LuaInstance {
	std::uint64_t address{ 0 };
};

LuaInstance* CheckInst(lua_State* L, int idx = 1)
{
	return static_cast<LuaInstance*>(luaL_checkudata(L, idx, k_mt));
}

bool ValidAddr(std::uint64_t addr)
{
	return addr != 0 && g_Memory.IsValid(addr);
}

bool IsBasePartClass(const std::string& cls)
{
	return cls == "Part" || cls == "MeshPart" || cls == "BasePart" ||
		cls == "UnionOperation" || cls == "TrussPart" || cls == "WedgePart" ||
		cls == "CornerWedgePart" || cls == "SpawnLocation" || cls == "Seat" ||
		cls == "VehicleSeat";
}

bool IsGuiObjectClass(const std::string& cls)
{
	return cls == "GuiObject" || cls == "Frame" || cls == "TextLabel" ||
		cls == "TextButton" || cls == "TextBox" || cls == "ImageLabel" ||
		cls == "ImageButton" || cls == "ScrollingFrame" || cls == "ViewportFrame" ||
		cls == "CanvasGroup" || cls == "VideoFrame";
}

bool ClassIsA(const std::string& cls, const char* query)
{
	if (!query || !query[0])
		return false;
	if (cls == query)
		return true;
	if (std::strcmp(query, "Instance") == 0)
		return true;

	if (std::strcmp(query, "BasePart") == 0 || std::strcmp(query, "PVInstance") == 0)
		return IsBasePartClass(cls);

	if (std::strcmp(query, "GuiObject") == 0 || std::strcmp(query, "GuiBase2d") == 0
		|| std::strcmp(query, "GuiBase") == 0)
		return IsGuiObjectClass(cls);

	if (std::strcmp(query, "GuiBase2d") == 0)
		return IsGuiObjectClass(cls) || cls == "ScreenGui" || cls == "BillboardGui"
			|| cls == "SurfaceGui";

	if (std::strcmp(query, "ValueBase") == 0)
	{
		return cls == "BoolValue" || cls == "IntValue" || cls == "NumberValue"
			|| cls == "StringValue" || cls == "ObjectValue" || cls == "Vector3Value"
			|| cls == "CFrameValue" || cls == "Color3Value" || cls == "BrickColorValue"
			|| cls == "RayValue";
	}

	if (std::strcmp(query, "Model") == 0)
		return cls == "Model" || cls == "WorldModel" || cls == "Actor";

	if (std::strcmp(query, "LuaSourceContainer") == 0)
		return cls == "LocalScript" || cls == "Script" || cls == "ModuleScript";

	if (std::strcmp(query, "Accoutrement") == 0)
		return cls == "Accoutrement" || cls == "Hat" || cls == "Accessory";

	if (std::strcmp(query, "Tool") == 0)
		return cls == "Tool" || cls == "HopperBin";

	if (std::strcmp(query, "LayerCollector") == 0)
		return cls == "ScreenGui" || cls == "BillboardGui" || cls == "SurfaceGui";

	return false;
}

// fake attrs — create без аллока роблокс-мапы (реал read отдельно)
struct fake_attr_t
{
	int ty{ 0 }; // 0nil 1bool 2num 3str
	bool b{ false };
	double n{ 0.0 };
	std::string s;
};

std::unordered_map<std::uint64_t, std::unordered_map<std::string, fake_attr_t>> g_fake_attr;

#if 0 // ===== attributes feature disabled — Offsets::Attribute / Offsets::Instance::ComponentMap were removed =====

std::uint64_t AttrMapFromInst(std::uint64_t inst)
{
	if (!ValidAddr(inst))
		return 0;

	const std::uint64_t cmap = g_Memory.Read<std::uint64_t>(
		inst + Offsets::Instance::ComponentMap);
	if (!ValidAddr(cmap))
		return 0;

	const uintptr_t base = g_Memory.GetModuleBase();
	if (!base)
		return 0;

	const std::uint16_t want = g_Memory.Read<std::uint16_t>(
		base + Offsets::Attribute::TypeIdRva);
	if (!want)
		return 0;

	const std::uint64_t b = g_Memory.Read<std::uint64_t>(cmap);
	const std::uint64_t e = g_Memory.Read<std::uint64_t>(cmap + 8);
	if (ValidAddr(b) && ValidAddr(e) && e >= b && (e - b) < 0x4000)
	{
		for (std::uint64_t slot = b; slot < e; slot += 16)
		{
			const std::uint16_t t = g_Memory.Read<std::uint16_t>(slot + 8);
			if (t != want)
				continue;
			const std::uint64_t ptr = g_Memory.Read<std::uint64_t>(slot);
			if (ValidAddr(ptr))
				return ptr;
		}
	}

	// overflow page @ +24
	const std::uint64_t page = g_Memory.Read<std::uint64_t>(cmap + 24);
	if (!ValidAddr(page))
		return 0;

	const std::uint32_t n = g_Memory.Read<std::uint32_t>(page + 24);
	const std::uint64_t arr = g_Memory.Read<std::uint64_t>(page);
	if (!ValidAddr(arr) || n > 256)
		return 0;

	for (std::uint32_t i = 0; i < n; ++i)
	{
		const std::uint64_t blk = g_Memory.Read<std::uint64_t>(arr + 8 * (i >> 2));
		if (!ValidAddr(blk))
			continue;
		const std::uint64_t ent = blk + 16ull * (i & 3);
		const std::uint16_t t = g_Memory.Read<std::uint16_t>(ent + 8);
		if (t != want)
			continue;
		const std::uint64_t ptr = g_Memory.Read<std::uint64_t>(ent);
		if (ValidAddr(ptr))
			return ptr;
	}

	return 0;
}

bool PushRealAttrValue(lua_State* L, std::uint64_t entry)
{
	const std::uint64_t va = entry + Offsets::Attribute::Value;
	const std::uint64_t a = g_Memory.Read<std::uint64_t>(va);
	const std::uint64_t c = g_Memory.Read<std::uint64_t>(va + 16);
	const uintptr_t base = g_Memory.GetModuleBase();

	auto finite3 = [](float x, float y, float z) -> bool {
		if (x != x || y != y || z != z) return false;
		if (x < -1e6f || x > 1e6f) return false;
		if (y < -1e6f || y > 1e6f) return false;
		if (z < -1e6f || z > 1e6f) return false;
		return true;
	};

	// type tag маленький (atlanta: 7 = cframe) — vec3/cf @ +16
	if (a > 0 && a < 64)
	{
		const Vector3 pos = g_Memory.Read<Vector3>(va + 16);
		if (finite3(pos.x, pos.y, pos.z))
		{
			if (a == 7 || a == 20 || a == 0x14)
			{
				Matrix4x4 rot{};
				rot.m[0][0] = rot.m[1][1] = rot.m[2][2] = 1.f;
				LuaTypes::PushCFrame(L, pos, rot);
				return true;
			}

			if (a == 4 || a == 17 || a == 0x11)
			{
				LuaTypes::PushVector3(L, pos);
				return true;
			}
		}
	}

	// bool: type в модуле, value 0/1 @ +16
	if (base && a >= base && a < base + 0x10000000ull && (c == 0 || c == 1))
	{
		lua_pushboolean(L, c != 0);
		return true;
	}

	// number double @ +16
	const double d = g_Memory.Read<double>(va + 16);
	if (d == d && d > -1e15 && d < 1e15)
	{
		// если похоже на ptr — не число
		if (!(c > 0x10000 && g_Memory.IsValid(c)))
		{
			lua_pushnumber(L, d);
			return true;
		}
	}

	if (ValidAddr(c))
	{
		const std::string s = g_Memory.ReadString(c);
		if (!s.empty() && s != "Unknown")
		{
			lua_pushstring(L, s.c_str());
			return true;
		}
	}

	lua_pushnil(L);
	return true;
}

void PushFakeAttr(lua_State* L, const fake_attr_t& v)
{
	if (v.ty == 1)
		lua_pushboolean(L, v.b ? 1 : 0);
	else if (v.ty == 2)
		lua_pushnumber(L, v.n);
	else if (v.ty == 3)
		lua_pushstring(L, v.s.c_str());
	else
		lua_pushnil(L);
}
#endif // ===== attribute helpers disabled =====


int l_tostring(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	if (!ValidAddr(ud->address))
	{
		lua_pushstring(L, "Instance(nil)");
		return 1;
	}
	Instance inst(ud->address);
	const std::string name = inst.GetName();
	const std::string cls = inst.GetClassName();
	char buf[256];
	std::snprintf(buf, sizeof(buf), "%s (%s) @ 0x%llX",
		name.empty() ? "?" : name.c_str(),
		cls.empty() ? "?" : cls.c_str(),
		static_cast<unsigned long long>(ud->address));
	lua_pushstring(L, buf);
	return 1;
}

int l_eq(lua_State* L)
{
	// 5.4 зовёт __eq для любых двух userdata, не только наших
	auto* a = static_cast<LuaInstance*>(luaL_testudata(L, 1, k_mt));
	auto* b = static_cast<LuaInstance*>(luaL_testudata(L, 2, k_mt));
	lua_pushboolean(L, a && b && a->address == b->address);
	return 1;
}

int l_GetChildren(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	lua_newtable(L);
	if (!ValidAddr(ud->address))
		return 1;

	const auto kids = Instance(ud->address).GetChildren();
	int i = 1;
	for (const auto& c : kids)
	{
		if (!ValidAddr(c.address))
			continue;
		PushInstance(L, c.address);
		lua_rawseti(L, -2, i++);
	}
	return 1;
}

// name или ClassName (сервисы)
std::uint64_t FindChildNameOrClass(std::uint64_t parent, const char* name)
{
	if (!ValidAddr(parent) || !name || !name[0])
		return 0;

	for (const auto& c : Instance(parent).GetChildren())
	{
		if (!ValidAddr(c.address))
			continue;
		Instance ch(c.address);
		if (ch.GetName() == name || ch.GetClassName() == name)
			return c.address;
	}
	return 0;
}

int l_FindFirstChild(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* name = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !name)
	{
		lua_pushnil(L);
		return 1;
	}
	const std::uint64_t hit = FindChildNameOrClass(ud->address, name);
	if (!hit)
		lua_pushnil(L);
	else
		PushInstance(L, hit);
	return 1;
}

int l_FindFirstChildOfClass(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* cls = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !cls)
	{
		lua_pushnil(L);
		return 1;
	}
	for (const auto& c : Instance(ud->address).GetChildren())
	{
		if (!ValidAddr(c.address))
			continue;
		if (Instance(c.address).GetClassName() == cls)
		{
			PushInstance(L, c.address);
			return 1;
		}
	}
	lua_pushnil(L);
	return 1;
}

int l_IsA(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* cls = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !cls)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_pushboolean(L, ClassIsA(Instance(ud->address).GetClassName(), cls));
	return 1;
}

int l_FindFirstChildWhichIsA(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* cls = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !cls)
	{
		lua_pushnil(L);
		return 1;
	}

	for (const auto& c : Instance(ud->address).GetChildren())
	{
		if (!ValidAddr(c.address))
			continue;
		if (ClassIsA(Instance(c.address).GetClassName(), cls))
		{
			PushInstance(L, c.address);
			return 1;
		}
	}

	lua_pushnil(L);
	return 1;
}

int l_GetPropertyChangedSignal(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* prop = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !prop)
	{
		lua_pushnil(L);
		return 1;
	}
	LuaVM::PushSignal(L, 9, ud->address, prop);
	return 1;
}

int l_IsDescendantOf(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	LuaInstance* anc = static_cast<LuaInstance*>(luaL_testudata(L, 2, k_mt));
	if (!ValidAddr(ud->address) || !anc || !ValidAddr(anc->address))
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	Instance cur(ud->address);
	for (int i = 0; i < 64; ++i)
	{
		auto p = cur.GetParent();
		if (!p || !ValidAddr(p->address))
			break;

		if (p->address == anc->address)
		{
			lua_pushboolean(L, 1);
			return 1;
		}

		cur = *p;
	}

	lua_pushboolean(L, 0);
	return 1;
}

struct wfc_t
{
	std::uint64_t parent{ 0 };
	float left{ 0.f };
	char name[128]{};
};

constexpr float k_wfc_poll = 0.05f;
constexpr int k_wfc_slot = 4;

int wfc_cont(lua_State* L, int status, lua_KContext ctx);

int l_WaitForChild(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* name = luaL_checkstring(L, 2);
	float timeout = static_cast<float>(luaL_optnumber(L, 3, 5.0));
	if (timeout < 0.f)
		timeout = 0.f;
	if (timeout > 3600.f)
		timeout = 3600.f;

	if (!ValidAddr(ud->address) || !name)
	{
		lua_pushnil(L);
		return 1;
	}

	const std::uint64_t hit = FindChildNameOrClass(ud->address, name);
	if (hit)
	{
		PushInstance(L, hit);
		return 1;
	}

	if (timeout <= 0.f)
	{
		lua_pushnil(L);
		return 1;
	}

	if (!lua_isyieldable(L))
	{
		lua_pushnil(L);
		return 1;
	}

	// состояние держим в кадре корутины, а не в реестре: брошенный
	// (cancel / переисполнение скрипта) поток иначе течёт ref'ом навсегда
	lua_settop(L, 3);
	wfc_t* w = static_cast<wfc_t*>(lua_newuserdatauv(L, sizeof(wfc_t), 0));
	w->parent = ud->address;
	w->left = timeout;
	{
		size_t n = std::strlen(name);
		if (n > sizeof(w->name) - 1)
			n = sizeof(w->name) - 1;
		std::memcpy(w->name, name, n);
		w->name[n] = 0;
	}

	LuaVM::ScheduleWait(L, k_wfc_poll);
	return lua_yieldk(L, 0, 0, wfc_cont);
}

int wfc_cont(lua_State* L, int /*status*/, lua_KContext /*ctx*/)
{
	wfc_t* w = static_cast<wfc_t*>(lua_touserdata(L, k_wfc_slot));
	if (!w)
	{
		lua_pushnil(L);
		return 1;
	}

	const std::uint64_t hit = FindChildNameOrClass(w->parent, w->name);
	if (hit)
	{
		PushInstance(L, hit);
		return 1;
	}

	w->left -= k_wfc_poll;
	if (w->left <= 0.f)
	{
		lua_pushnil(L);
		return 1;
	}

	LuaVM::ScheduleWait(L, k_wfc_poll);
	return lua_yieldk(L, 0, 0, wfc_cont);
}

int l_GetFullName(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	if (!ValidAddr(ud->address))
	{
		lua_pushstring(L, "");
		return 1;
	}

	std::vector<std::string> parts;
	Instance cur(ud->address);
	for (int depth = 0; depth < 64; ++depth)
	{
		if (!ValidAddr(cur.address))
			break;
		if (cur.GetClassName() == "DataModel")
			break;
		parts.push_back(cur.GetName());
		auto p = cur.GetParent();
		if (!p || !ValidAddr(p->address))
			break;
		cur = *p;
	}

	std::string full;
	for (int i = static_cast<int>(parts.size()) - 1; i >= 0; --i)
	{
		if (!full.empty())
			full += '.';
		full += parts[static_cast<size_t>(i)];
	}
	lua_pushstring(L, full.c_str());
	return 1;
}

int l_GetService(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* name = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !name)
	{
		lua_pushnil(L);
		return 1;
	}

	// наш фейк, в DataModel детей нет
	if (_stricmp(name, "RunService") == 0)
	{
		lua_getglobal(L, "RunService");
		return 1;
	}
	if (_stricmp(name, "UserInputService") == 0)
	{
		lua_getglobal(L, "UserInputService");
		return 1;
	}
	if (_stricmp(name, "TweenService") == 0)
	{
		lua_getglobal(L, "TweenService");
		return 1;
	}
	if (_stricmp(name, "HttpService") == 0)
	{
		lua_getglobal(L, "HttpService");
		return 1;
	}

	for (const auto& c : Instance(ud->address).GetChildren())
	{
		if (!ValidAddr(c.address))
			continue;
		Instance child(c.address);
		if (child.GetClassName() == name || child.GetName() == name)
		{
			PushInstance(L, c.address);
			return 1;
		}
	}
	lua_pushnil(L);
	return 1;
}

int l_GetPlayers(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	lua_newtable(L);
	if (!ValidAddr(ud->address))
		return 1;

	int i = 1;
	for (const auto& c : Instance(ud->address).GetChildren())
	{
		if (!ValidAddr(c.address))
			continue;
		if (Instance(c.address).GetClassName() != "Player")
			continue;
		PushInstance(L, c.address);
		lua_rawseti(L, -2, i++);
	}
	return 1;
}

int l_index(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* key = luaL_checkstring(L, 2);

	luaL_getmetatable(L, k_mt);
	lua_pushvalue(L, 2);
	lua_rawget(L, -2);
	if (!lua_isnil(L, -1))
	{
		lua_remove(L, -2);
		return 1;
	}
	lua_pop(L, 2);

	if (!ValidAddr(ud->address))
	{
		lua_pushnil(L);
		return 1;
	}

	Instance inst(ud->address);

	// GetClassName — чтение чужого процесса, не дёргаем его ради Name/Parent
	if (std::strcmp(key, "Name") == 0)
	{
		lua_pushstring(L, inst.GetName().c_str());
		return 1;
	}
	if (std::strcmp(key, "Address") == 0)
	{
		lua_pushinteger(L, static_cast<lua_Integer>(ud->address));
		return 1;
	}
	if (std::strcmp(key, "Parent") == 0)
	{
		auto p = inst.GetParent();
		if (!p || !ValidAddr(p->address))
			lua_pushnil(L);
		else
			PushInstance(L, p->address);
		return 1;
	}
	if (std::strcmp(key, "ChildAdded") == 0)
	{
		LuaVM::PushSignal(L, 6, ud->address);
		return 1;
	}
	if (std::strcmp(key, "ChildRemoved") == 0)
	{
		LuaVM::PushSignal(L, 7, ud->address);
		return 1;
	}
	if (std::strcmp(key, "DescendantAdded") == 0)
	{
		LuaVM::PushSignal(L, 8, ud->address);
		return 1;
	}

	const std::string cls = inst.GetClassName();
	if (std::strcmp(key, "ClassName") == 0)
	{
		lua_pushstring(L, cls.c_str());
		return 1;
	}

	if (cls == "BindableEvent")
	{
		if (std::strcmp(key, "Event") == 0)
		{
			LuaVM::PushSignal(L, 10, ud->address);
			return 1;
		}
	}

	if (cls == "RemoteEvent" || cls == "UnreliableRemoteEvent")
	{
		if (std::strcmp(key, "OnClientEvent") == 0)
		{
			LuaVM::PushSignal(L, 11, ud->address);
			return 1;
		}
	}

	if (cls == "ProximityPrompt")
	{
		if (std::strcmp(key, "Triggered") == 0)
		{
			LuaVM::PushSignal(L, 12, ud->address);
			return 1;
		}
	}

	if (cls == "DataModel")
	{
		DataModel dm(ud->address);
		if (std::strcmp(key, "PlaceId") == 0)
		{
			lua_pushinteger(L, static_cast<lua_Integer>(dm.GetPlaceId()));
			return 1;
		}
		if (std::strcmp(key, "GameId") == 0)
		{
			lua_pushinteger(L, static_cast<lua_Integer>(dm.GetGameId()));
			return 1;
		}
		if (std::strcmp(key, "JobId") == 0)
		{
			lua_pushstring(L, dm.GetJobId().c_str());
			return 1;
		}
		if (std::strcmp(key, "Workspace") == 0 || std::strcmp(key, "workspace") == 0)
		{
			if (Globals::Workspace && ValidAddr(Globals::Workspace->address))
				PushInstance(L, Globals::Workspace->address);
			else
				lua_pushnil(L);
			return 1;
		}
	}

	if (cls == "Players")
	{
		if (std::strcmp(key, "LocalPlayer") == 0)
		{
			const std::uint64_t lp = g_Memory.Read<std::uint64_t>(
				ud->address + ::Player::LocalPlayer);
			if (ValidAddr(lp))
				PushInstance(L, lp);
			else
				lua_pushnil(L);
			return 1;
		}
		if (std::strcmp(key, "PlayerAdded") == 0)
		{
			LuaVM::PushSignal(L, 1, 0);
			return 1;
		}
		if (std::strcmp(key, "PlayerRemoving") == 0)
		{
			LuaVM::PushSignal(L, 2, 0);
			return 1;
		}
	}

	if (cls == "Player")
	{
		Player pl(ud->address);
		if (std::strcmp(key, "Character") == 0)
		{
			auto ch = pl.GetCharacter();
			if (!ch || !ValidAddr(ch->address))
				lua_pushnil(L);
			else
				PushInstance(L, ch->address);
			return 1;
		}
		if (std::strcmp(key, "CharacterAdded") == 0)
		{
			LuaVM::PushSignal(L, 3, ud->address);
			return 1;
		}
		if (std::strcmp(key, "DisplayName") == 0)
		{
			lua_pushstring(L, pl.GetDisplayName().c_str());
			return 1;
		}
		if (std::strcmp(key, "UserId") == 0)
		{
			lua_pushinteger(L, static_cast<lua_Integer>(pl.GetUserId()));
			return 1;
		}
		if (std::strcmp(key, "Team") == 0)
		{
			const std::uint64_t team = pl.GetTeam();
			if (ValidAddr(team))
				PushInstance(L, team);
			else
				lua_pushnil(L);
			return 1;
		}
	}

	if (cls == "Workspace")
	{
		if (std::strcmp(key, "CurrentCamera") == 0)
		{
			auto cam = Workspace(ud->address).GetCurrentCamera();
			if (!cam || !ValidAddr(cam->address))
				lua_pushnil(L);
			else
				PushInstance(L, cam->address);
			return 1;
		}
	}

	if (cls == "Lighting")
	{
		Lighting light(ud->address);
		if (std::strcmp(key, "Brightness") == 0)
		{
			lua_pushnumber(L, light.GetBrightness());
			return 1;
		}

		if (std::strcmp(key, "ClockTime") == 0)
		{
			lua_pushnumber(L, light.GetClockTime());
			return 1;
		}

		if (std::strcmp(key, "FogStart") == 0)
		{
			lua_pushnumber(L, light.GetFogStart());
			return 1;
		}

		if (std::strcmp(key, "FogEnd") == 0)
		{
			lua_pushnumber(L, light.GetFogEnd());
			return 1;
		}

		if (std::strcmp(key, "GlobalShadows") == 0)
		{
			lua_pushboolean(L, light.GetGlobalShadows());
			return 1;
		}

		if (std::strcmp(key, "Ambient") == 0)
		{
			const Color3 c = light.GetAmbient();
			LuaTypes::PushColor3(L, c.r, c.g, c.b);
			return 1;
		}

		if (std::strcmp(key, "OutdoorAmbient") == 0)
		{
			const Color3 c = light.GetOutdoorAmbient();
			LuaTypes::PushColor3(L, c.r, c.g, c.b);
			return 1;
		}

		if (std::strcmp(key, "FogColor") == 0)
		{
			const Color3 c = light.GetFogColor();
			LuaTypes::PushColor3(L, c.r, c.g, c.b);
			return 1;
		}
	}

	if (cls == "Camera")
	{
		if (std::strcmp(key, "FieldOfView") == 0)
		{
			lua_pushnumber(L, Camera(ud->address).GetFieldOfView());
			return 1;
		}
		if (std::strcmp(key, "ViewportSize") == 0)
		{
			const Vector2 v = Camera(ud->address).GetViewportSize();
			LuaTypes::PushVector2(L, v.x, v.y);
			return 1;
		}
	}

	// GuiObject / GuiBase2D layout props (Fisch-style scripts read these for
	// minigame UI detection/positioning)
	if (IsGuiObjectClass(cls))
	{
		if (std::strcmp(key, "AbsolutePosition") == 0)
		{
			const Vector2 v = g_Memory.Read<Vector2>(ud->address + ::GuiBase2D::AbsolutePosition);
			LuaTypes::PushVector2(L, v.x, v.y);
			return 1;
		}
		if (std::strcmp(key, "AbsoluteSize") == 0)
		{
			const Vector2 v = g_Memory.Read<Vector2>(ud->address + ::GuiBase2D::AbsoluteSize);
			LuaTypes::PushVector2(L, v.x, v.y);
			return 1;
		}
		if (std::strcmp(key, "BackgroundColor3") == 0)
		{
			const Vector3 c = g_Memory.Read<Vector3>(ud->address + ::GuiObject::BackgroundColor3);
			LuaTypes::PushColor3(L, c.x, c.y, c.z);
			return 1;
		}
	}

	if (IsBasePartClass(cls))
	{
		BasePart part(ud->address);
		if (std::strcmp(key, "Position") == 0)
		{
			LuaTypes::PushVector3(L, part.GetPosition());
			return 1;
		}
		if (std::strcmp(key, "Size") == 0)
		{
			LuaTypes::PushVector3(L, part.GetSize());
			return 1;
		}
		if (std::strcmp(key, "CFrame") == 0)
		{
			LuaTypes::PushCFrame(L, part.GetPosition(), part.GetRotation());
			return 1;
		}
		if (std::strcmp(key, "Transparency") == 0)
		{
			lua_pushnumber(L, part.GetTransparency());
			return 1;
		}
		if (std::strcmp(key, "Anchored") == 0)
		{
			lua_pushboolean(L, part.IsAnchored());
			return 1;
		}
		if (std::strcmp(key, "Velocity") == 0 || std::strcmp(key, "AssemblyLinearVelocity") == 0)
		{
			LuaTypes::PushVector3(L, part.GetAssemblyLinearVelocity());
			return 1;
		}
		if (std::strcmp(key, "AssemblyAngularVelocity") == 0)
		{
			LuaTypes::PushVector3(L, part.GetAssemblyAngularVelocity());
			return 1;
		}
	}

	if (cls == "Humanoid")
	{
		Humanoid hum(ud->address);
		if (std::strcmp(key, "Health") == 0)
		{
			lua_pushnumber(L, hum.GetHealth());
			return 1;
		}
		if (std::strcmp(key, "MaxHealth") == 0)
		{
			lua_pushnumber(L, hum.GetMaxHealth());
			return 1;
		}
		if (std::strcmp(key, "WalkSpeed") == 0)
		{
			lua_pushnumber(L, hum.GetWalkSpeed());
			return 1;
		}
		if (std::strcmp(key, "DisplayName") == 0)
		{
			lua_pushstring(L, hum.GetDisplayName().c_str());
			return 1;
		}
		if (std::strcmp(key, "RootPart") == 0)
		{
			auto rp = hum.GetRootPart();
			if (!rp || !ValidAddr(rp->address))
				lua_pushnil(L);
			else
				PushInstance(L, rp->address);
			return 1;
		}
	}

	// ValueBase — IDA + live dump: Misc::Value = 0xb8
	if (std::strcmp(key, "Value") == 0)
	{
		const std::uint64_t va = ud->address + ::Misc::Value;

		if (cls == "BoolValue")
		{
			lua_pushboolean(L, g_Memory.Read<std::uint8_t>(va) != 0);
			return 1;
		}

		if (cls == "IntValue")
		{
			lua_pushinteger(L, g_Memory.Read<std::int32_t>(va));
			return 1;
		}

		if (cls == "NumberValue")
		{
			lua_pushnumber(L, g_Memory.Read<double>(va));
			return 1;
		}

		if (cls == "StringValue")
		{
			lua_pushstring(L, g_Memory.ReadString(va).c_str());
			return 1;
		}

		if (cls == "ObjectValue")
		{
			const std::uint64_t obj = g_Memory.Read<std::uint64_t>(va);
			if (ValidAddr(obj))
				PushInstance(L, obj);
			else
				lua_pushnil(L);
			return 1;
		}

		if (cls == "Vector3Value")
		{
			LuaTypes::PushVector3(L, g_Memory.Read<Vector3>(va));
			return 1;
		}
	}

	auto child = inst.FindFirstChild(key);
	if (child && ValidAddr(child->address))
	{
		PushInstance(L, child->address);
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

constexpr size_t k_desc_cap = 50000;
constexpr int k_desc_depth = 64;

// битый Parent/Children в чужой памяти даёт цикл: без лимита глубины
// рекурсия сожрёт стек раньше, чем сработает cap по узлам
void CollectDescendants(std::uint64_t addr, lua_State* L, int table_idx, int& n, size_t& nodes, int depth)
{
	if (!ValidAddr(addr) || nodes >= k_desc_cap || depth >= k_desc_depth)
		return;
	Instance inst(addr);
	for (const auto& c : inst.GetChildren())
	{
		if (nodes >= k_desc_cap)
			break;
		if (!ValidAddr(c.address))
			continue;
		++nodes;
		++n;
		PushInstance(L, c.address);
		lua_rawseti(L, table_idx, n);
		CollectDescendants(c.address, L, table_idx, n, nodes, depth + 1);
	}
}

int l_GetDescendants(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	if (!lua_checkstack(L, 4))
		return luaL_error(L, "GetDescendants: stack");
	lua_newtable(L);
	int n = 0;
	size_t nodes = 0;
	if (ValidAddr(ud->address))
		CollectDescendants(ud->address, L, lua_gettop(L), n, nodes, 0);
	return 1;
}

int l_Instance_new(lua_State* L)
{
	const char* name = luaL_checkstring(L, 1);
	if (!name || !name[0])
	{
		lua_pushnil(L);
		return 1;
	}

	std::uint64_t parent = 0;
	if (!lua_isnoneornil(L, 2))
	{
		auto* pud = static_cast<LuaInstance*>(luaL_testudata(L, 2, k_mt));
		if (!pud)
		{
			lua_pushnil(L);
			return 1;
		}
		parent = pud->address;
	}

	std::uint64_t addr = 0;
	if (!InstanceCreate::New(name, parent, &addr) || !addr)
	{
		char buf[128]{};
		const int fe = InstanceCreate::LastFail();
		const char* why = "";
		switch (fe)
		{
		case 3:  why = "unknown class"; break;
		case 6:  why = "no call gate"; break;
		case 8:  why = "gate timeout"; break;
		case 9:  why = "not creatable"; break;
		case 10: why = "create timeout"; break;
		case 11: why = "create returned junk"; break;
		case 12: why = "parent failed"; break;
		default: break;
		}

		if (why[0])
			std::snprintf(buf, sizeof(buf), "unable create %s: %s", name, why);
		else
			std::snprintf(buf, sizeof(buf), "unable create %s (%d)", name, fe);
		lua_getglobal(L, "print");
		if (lua_isfunction(L, -1))
		{
			lua_pushstring(L, buf);
			lua_pcall(L, 1, 0, 0);
		}

		else
			lua_pop(L, 1);

		lua_pushnil(L);
		return 1;
	}

	PushInstance(L, addr);
	return 1;
}

int l_newindex(lua_State* L)
{
	LuaInstance* ud = CheckInst(L, 1);
	const char* key = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address))
		return 0;

	Instance inst(ud->address);
	const std::string cls = inst.GetClassName();

	if (std::strcmp(key, "Parent") == 0)
	{
		if (lua_isnil(L, 3))
		{
			InstanceCreate::SetParent(ud->address, 0);
			return 0;
		}

		LuaInstance* p = static_cast<LuaInstance*>(
			luaL_testudata(L, 3, k_mt));
		if (!p || !ValidAddr(p->address))
			return 0;

		InstanceCreate::SetParent(ud->address, p->address);
		return 0;
	}

	if (IsBasePartClass(cls))
	{
		BasePart part(ud->address);
		if (std::strcmp(key, "Position") == 0)
		{
			Vector3 v{};
			if (LuaTypes::ToVector3(L, 3, v))
				part.SetPosition(v);
			return 0;
		}

		if (std::strcmp(key, "CFrame") == 0)
		{
			Vector3 pos{};
			Matrix4x4 rot{};
			if (LuaTypes::ToCFrame(L, 3, pos, rot))
				part.SetPosition(pos);

			else
			{
				Vector3 v{};
				if (LuaTypes::ToVector3(L, 3, v))
					part.SetPosition(v);
			}
			return 0;
		}

		if (std::strcmp(key, "Size") == 0)
		{
			Vector3 v{};
			if (LuaTypes::ToVector3(L, 3, v))
				part.SetSize(v);
			return 0;
		}

		if (std::strcmp(key, "Transparency") == 0)
		{
			part.SetTransparency(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}

		if (std::strcmp(key, "Anchored") == 0)
		{
			part.SetAnchored(lua_toboolean(L, 3) != 0);
			return 0;
		}

		if (std::strcmp(key, "Velocity") == 0 || std::strcmp(key, "AssemblyLinearVelocity") == 0)
		{
			Vector3 v{};
			if (LuaTypes::ToVector3(L, 3, v))
				part.SetAssemblyLinearVelocity(v);
			return 0;
		}

		if (std::strcmp(key, "CanCollide") == 0)
		{
			part.SetCanCollide(lua_toboolean(L, 3) != 0);
			return 0;
		}

		if (std::strcmp(key, "Color") == 0 || std::strcmp(key, "Color3") == 0)
		{
			float r = 0.f, g = 0.f, b = 0.f;
			if (LuaTypes::ToColor3(L, 3, r, g, b))
				part.SetColor(Color3{ r, g, b });
			return 0;
		}
	}

	if (cls == "Humanoid")
	{
		Humanoid hum(ud->address);
		if (std::strcmp(key, "Health") == 0)
		{
			hum.SetHealth(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}

		if (std::strcmp(key, "MaxHealth") == 0)
		{
			hum.SetMaxHealth(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}

		if (std::strcmp(key, "WalkSpeed") == 0)
		{
			hum.SetWalkSpeed(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}

		if (std::strcmp(key, "JumpPower") == 0)
		{
			hum.SetJumpPower(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}

		if (std::strcmp(key, "JumpHeight") == 0)
		{
			hum.SetJumpHeight(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}
	}

	if (cls == "Lighting")
	{
		Lighting light(ud->address);
		if (std::strcmp(key, "Brightness") == 0)
		{
			light.SetBrightness(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}

		if (std::strcmp(key, "ClockTime") == 0)
		{
			light.SetClockTime(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}

		if (std::strcmp(key, "FogStart") == 0)
		{
			light.SetFogStart(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}

		if (std::strcmp(key, "FogEnd") == 0)
		{
			light.SetFogEnd(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}

		if (std::strcmp(key, "GlobalShadows") == 0)
		{
			light.SetGlobalShadows(lua_toboolean(L, 3) != 0);
			return 0;
		}

		if (std::strcmp(key, "Ambient") == 0)
		{
			float r = 0.f, g = 0.f, b = 0.f;
			if (LuaTypes::ToColor3(L, 3, r, g, b))
				light.SetAmbient(Color3{ r, g, b });
			return 0;
		}

		if (std::strcmp(key, "OutdoorAmbient") == 0)
		{
			float r = 0.f, g = 0.f, b = 0.f;
			if (LuaTypes::ToColor3(L, 3, r, g, b))
				light.SetOutdoorAmbient(Color3{ r, g, b });
			return 0;
		}

		if (std::strcmp(key, "FogColor") == 0)
		{
			float r = 0.f, g = 0.f, b = 0.f;
			if (LuaTypes::ToColor3(L, 3, r, g, b))
				light.SetFogColor(Color3{ r, g, b });
			return 0;
		}
	}

	if (cls == "Camera")
	{
		Camera cam(ud->address);
		if (std::strcmp(key, "FieldOfView") == 0)
		{
			cam.SetFieldOfView(static_cast<float>(luaL_checknumber(L, 3)));
			return 0;
		}
	}

	// ValueBase write — StringValue только SSO (<16), длинные пока мимо
	if (std::strcmp(key, "Value") == 0)
	{
		const std::uint64_t va = ud->address + ::Misc::Value;

		if (cls == "BoolValue")
		{
			g_Memory.Write<std::uint8_t>(va, lua_toboolean(L, 3) ? 1 : 0);
			return 0;
		}

		if (cls == "IntValue")
		{
			g_Memory.Write<std::int32_t>(va, static_cast<std::int32_t>(luaL_checkinteger(L, 3)));
			return 0;
		}

		if (cls == "NumberValue")
		{
			g_Memory.Write<double>(va, luaL_checknumber(L, 3));
			return 0;
		}

		if (cls == "ObjectValue")
		{
			if (lua_isnil(L, 3))
			{
				g_Memory.Write<std::uint64_t>(va, 0);
				return 0;
			}

			LuaInstance* o = static_cast<LuaInstance*>(luaL_testudata(L, 3, k_mt));
			if (!o)
				return 0;

			g_Memory.Write<std::uint64_t>(va, o->address);
			return 0;
		}

		if (cls == "Vector3Value")
		{
			Vector3 v{};
			if (LuaTypes::ToVector3(L, 3, v))
				g_Memory.Write<Vector3>(va, v);
			return 0;
		}

		if (cls == "StringValue")
		{
			const char* s = luaL_checkstring(L, 3);
			if (!s)
				return 0;

			const int n = static_cast<int>(std::strlen(s));
			if (n < 0 || n >= 16)
				return 0; // длинные через heap — потом

			char buf[24]{};
			std::memcpy(buf, s, static_cast<size_t>(n));
			*reinterpret_cast<std::int32_t*>(buf + 0x10) = n;
			g_Memory.WriteRaw(static_cast<uintptr_t>(va), buf, 0x18);
			return 0;
		}
	}

	return 0;
}

#if 0 // ===== attribute Lua API disabled — Offsets::Attribute was removed =====

int l_GetAttribute(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* name = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !name)
	{
		lua_pushnil(L);
		return 1;
	}

	// fake first — наш SetAttribute create
	auto it = g_fake_attr.find(ud->address);
	if (it != g_fake_attr.end())
	{
		auto jt = it->second.find(name);
		if (jt != it->second.end())
		{
			PushFakeAttr(L, jt->second);
			return 1;
		}
	}

	const std::uint64_t amap = AttrMapFromInst(ud->address);
	if (!amap)
	{
		lua_pushnil(L);
		return 1;
	}

	const std::uint32_t cnt = g_Memory.Read<std::uint32_t>(
		amap + Offsets::AttributesMap::Length);
	const std::uint64_t ents = g_Memory.Read<std::uint64_t>(
		amap + Offsets::AttributesMap::Attributes);
	if (!ValidAddr(ents) || cnt == 0 || cnt > 256)
	{
		lua_pushnil(L);
		return 1;
	}

	for (std::uint32_t i = 0; i < cnt; ++i)
	{
		const std::uint64_t ent = ents + Offsets::Attribute::Size * i;
		const std::uint64_t k = g_Memory.Read<std::uint64_t>(ent + Offsets::Attribute::Key);
		if (!ValidAddr(k))
			continue;
		if (g_Memory.ReadString(k) != name)
			continue;
		PushRealAttrValue(L, ent);
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

int l_SetAttribute(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* name = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !name || !name[0])
		return 0;

	fake_attr_t v{};
	const int ty = lua_type(L, 3);
	if (ty == LUA_TNIL || ty == LUA_TNONE)
		v.ty = 0;
	else if (ty == LUA_TBOOLEAN)
	{
		v.ty = 1;
		v.b = lua_toboolean(L, 3) != 0;
	}

	else if (ty == LUA_TNUMBER)
	{
		v.ty = 2;
		v.n = lua_tonumber(L, 3);
	}

	else if (ty == LUA_TSTRING)
	{
		v.ty = 3;
		const char* s = lua_tostring(L, 3);
		v.s = s ? s : "";
	}

	else
		return 0;

	if (v.ty == 0)
		g_fake_attr[ud->address].erase(name);
	else
		g_fake_attr[ud->address][name] = v;

	// реал write только если entry уже есть (create в роблокс-мапу потом)
	const std::uint64_t amap = AttrMapFromInst(ud->address);
	if (!amap || v.ty == 0)
		return 0;

	const std::uint32_t cnt = g_Memory.Read<std::uint32_t>(
		amap + Offsets::AttributesMap::Length);
	const std::uint64_t ents = g_Memory.Read<std::uint64_t>(
		amap + Offsets::AttributesMap::Attributes);
	if (!ValidAddr(ents) || cnt == 0 || cnt > 256)
		return 0;

	for (std::uint32_t i = 0; i < cnt; ++i)
	{
		const std::uint64_t ent = ents + Offsets::Attribute::Size * i;
		const std::uint64_t k = g_Memory.Read<std::uint64_t>(ent + Offsets::Attribute::Key);
		if (!ValidAddr(k))
			continue;
		if (g_Memory.ReadString(k) != name)
			continue;

		const std::uint64_t va = ent + Offsets::Attribute::Value;
		if (v.ty == 1)
			g_Memory.Write<std::uint64_t>(va + 16, v.b ? 1 : 0);
		else if (v.ty == 2)
			g_Memory.Write<double>(va + 16, v.n);
		break;
	}

	return 0;
}

int l_GetAttributes(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	lua_newtable(L);
	if (!ValidAddr(ud->address))
		return 1;

	auto it = g_fake_attr.find(ud->address);
	if (it != g_fake_attr.end())
	{
		for (const auto& kv : it->second)
		{
			lua_pushstring(L, kv.first.c_str());
			PushFakeAttr(L, kv.second);
			lua_settable(L, -3);
		}
	}

	const std::uint64_t amap = AttrMapFromInst(ud->address);
	if (!amap)
		return 1;

	const std::uint32_t cnt = g_Memory.Read<std::uint32_t>(
		amap + Offsets::AttributesMap::Length);
	const std::uint64_t ents = g_Memory.Read<std::uint64_t>(
		amap + Offsets::AttributesMap::Attributes);
	if (!ValidAddr(ents) || cnt == 0 || cnt > 256)
		return 1;

	for (std::uint32_t i = 0; i < cnt; ++i)
	{
		const std::uint64_t ent = ents + Offsets::Attribute::Size * i;
		const std::uint64_t k = g_Memory.Read<std::uint64_t>(ent + Offsets::Attribute::Key);
		if (!ValidAddr(k))
			continue;
		const std::string nm = g_Memory.ReadString(k);
		if (nm.empty() || nm == "Unknown")
			continue;
		// fake перекрывает
		if (it != g_fake_attr.end() && it->second.count(nm))
			continue;
		lua_pushstring(L, nm.c_str());
		PushRealAttrValue(L, ent);
		lua_settable(L, -3);
	}

	return 1;
}
#endif // ===== attribute Lua API disabled =====

// No-op stubs so the (now removed) attribute API still resolves without inventing offsets.
int l_GetAttribute(lua_State* L)   { (void)L; lua_pushnil(L);  return 1; }
int l_SetAttribute(lua_State* L)   { (void)L; return 0; }
int l_GetAttributes(lua_State* L)  { (void)L; lua_newtable(L); return 1; }


int l_Fire(lua_State* L)
{
	return luaL_error(L, "Fire: temporarily disabled");
}

// --- Mouse (Player:GetMouse) ---
constexpr const char* k_mouse_mt = "jewsploit.Mouse";

struct LuaMouse
{
	int dummy{ 0 };
};

static HWND MouseHwnd()
{
	if (HWND h = Renderer::GetGameHwnd())
		return h;
	return Renderer::GetHwnd();
}

int l_mouse_index(lua_State* L)
{
	const char* key = luaL_checkstring(L, 2);

	if (std::strcmp(key, "X") == 0 || std::strcmp(key, "Y") == 0)
	{
		POINT pt{};
		GetCursorPos(&pt);
		if (HWND h = MouseHwnd())
			ScreenToClient(h, &pt);
		lua_pushinteger(L, key[0] == 'X' ? static_cast<lua_Integer>(pt.x)
			: static_cast<lua_Integer>(pt.y));
		return 1;
	}
	if (std::strcmp(key, "Button1Down") == 0)
	{
		lua_pushboolean(L, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0);
		return 1;
	}
	if (std::strcmp(key, "Button2Down") == 0)
	{
		lua_pushboolean(L, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 1 : 0);
		return 1;
	}
	if (std::strcmp(key, "Hit") == 0)
	{
		LuaTypes::PushVector3(L, Vector3(0.f, 0.f, 0.f)); // external: no raycast hit
		return 1;
	}
	if (std::strcmp(key, "Target") == 0)
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

int l_GetMouse(lua_State* L)
{
	(void)L;
	auto* m = static_cast<LuaMouse*>(lua_newuserdatauv(L, sizeof(LuaMouse), 0));
	m->dummy = 0;
	luaL_getmetatable(L, k_mouse_mt);
	lua_setmetatable(L, -2);
	return 1;
}

void CreateMouseMetatable(lua_State* L)
{
	if (luaL_newmetatable(L, k_mouse_mt))
	{
		lua_pushcfunction(L, l_mouse_index);
		lua_setfield(L, -2, "__index");
	}
	lua_pop(L, 1);
}

void CreateMetatable(lua_State* L)
{
	if (luaL_newmetatable(L, k_mt))
	{
		lua_pushcfunction(L, l_index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, l_newindex);
		lua_setfield(L, -2, "__newindex");
		lua_pushcfunction(L, l_tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, l_eq);
		lua_setfield(L, -2, "__eq");

		lua_pushcfunction(L, l_GetChildren);
		lua_setfield(L, -2, "GetChildren");
		lua_pushcfunction(L, l_FindFirstChild);
		lua_setfield(L, -2, "FindFirstChild");
		lua_pushcfunction(L, l_FindFirstChildOfClass);
		lua_setfield(L, -2, "FindFirstChildOfClass");
		lua_pushcfunction(L, l_FindFirstChildWhichIsA);
		lua_setfield(L, -2, "FindFirstChildWhichIsA");
		lua_pushcfunction(L, l_IsA);
		lua_setfield(L, -2, "IsA");
		lua_pushcfunction(L, l_IsDescendantOf);
		lua_setfield(L, -2, "IsDescendantOf");
		lua_pushcfunction(L, l_WaitForChild);
		lua_setfield(L, -2, "WaitForChild");
		lua_pushcfunction(L, l_GetAttribute);
		lua_setfield(L, -2, "GetAttribute");
		lua_pushcfunction(L, l_SetAttribute);
		lua_setfield(L, -2, "SetAttribute");
		lua_pushcfunction(L, l_GetAttributes);
		lua_setfield(L, -2, "GetAttributes");
		lua_pushcfunction(L, l_GetFullName);
		lua_setfield(L, -2, "GetFullName");
		lua_pushcfunction(L, l_GetService);
		lua_setfield(L, -2, "GetService");
		lua_pushcfunction(L, l_GetPlayers);
		lua_setfield(L, -2, "GetPlayers");
		lua_pushcfunction(L, l_GetDescendants);
		lua_setfield(L, -2, "GetDescendants");
		lua_pushcfunction(L, l_GetPropertyChangedSignal);
		lua_setfield(L, -2, "GetPropertyChangedSignal");
		lua_pushcfunction(L, l_Fire);
		lua_setfield(L, -2, "Fire");
		lua_pushcfunction(L, l_GetMouse);
		lua_setfield(L, -2, "GetMouse");
	}
	lua_pop(L, 1);
}

} // namespace

void PushInstance(lua_State* L, std::uint64_t address)
{
	auto* ud = static_cast<LuaInstance*>(lua_newuserdatauv(L, sizeof(LuaInstance), 0));
	ud->address = address;
	luaL_getmetatable(L, k_mt);
	lua_setmetatable(L, -2);
}

std::uint64_t CheckAddress(lua_State* L, int idx)
{
	auto* ud = static_cast<LuaInstance*>(luaL_checkudata(L, idx, "jewsploit.Instance"));
	return ud->address;
}

void RefreshGlobals(lua_State* L)
{
	// зовётся на каждый execute; адреса мёртвых инстансов иначе копятся сессию
	g_fake_attr.clear();

	if (ValidAddr(Globals::InstanceDataModel.address))
		PushInstance(L, Globals::InstanceDataModel.address);
	else
		lua_pushnil(L);
	lua_setglobal(L, "game");

	if (Globals::Workspace && ValidAddr(Globals::Workspace->address))
		PushInstance(L, Globals::Workspace->address);
	else
		lua_pushnil(L);
	lua_setglobal(L, "workspace");
}

void Register(lua_State* L)
{
	CreateMetatable(L);
	CreateMouseMetatable(L);
	RefreshGlobals(L);

	lua_newtable(L);
	lua_pushcfunction(L, l_Instance_new);
	lua_setfield(L, -2, "new");
	lua_setglobal(L, "Instance");
}

} // namespace LuaBridge
} // namespace Features
} // namespace Cheat
