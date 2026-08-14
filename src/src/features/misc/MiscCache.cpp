#include "pch.h"
#include "MiscCache.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"
#include <Windows.h>

#undef GetClassName

namespace Cheat {
namespace Features {
namespace MiscCache {

static Cache g_cache;

Cache& Get()
{
	return g_cache;
}

std::uint64_t find_svc(const char* cls)
{
	for (const auto& c : Cheat::Globals::InstanceDataModel.GetChildren())
	{
		if (c.GetClassName() == cls)
			return c.address;
	}
	return 0;
}

std::uint64_t local_chara()
{
	if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
		return 0;

	std::uint64_t lp = g_Memory.Read<std::uint64_t>(
		Cheat::Globals::Players->address + ::Player::LocalPlayer);
	if (!g_Memory.IsValid(lp))
		return 0;

	std::uint64_t chara = g_Memory.Read<std::uint64_t>(
		lp + ::Player::ModelInstance);
	if (!g_Memory.IsValid(chara))
		return 0;

	return chara;
}

// раз в ~400мс подтягиваем локала / лайтинг
void refresh_cache()
{
	std::uint64_t now = GetTickCount64();
	if (now - g_cache.last_refresh < 400 && g_cache.last_refresh != 0)
		return;
	g_cache.last_refresh = now;

	if (g_cache.datamodel != Cheat::Globals::InstanceDataModel.address)
	{
		g_cache.datamodel = Cheat::Globals::InstanceDataModel.address;
		g_cache.lighting = 0;
	}

	if (!g_Memory.IsValid(g_cache.lighting) || Cheat::g_Settings.world.time_changer)
		g_cache.lighting = find_svc("Lighting");

	std::uint64_t chara = local_chara();
	if (g_Memory.IsValid(chara))
	{
		Cheat::Instance ch(chara);
		std::uint64_t root = 0, hum = 0;
		for (const auto& part : ch.GetChildren())
		{
			if (!hum && part.GetClassName() == "Humanoid")
				hum = part.address;
			if (!root && part.GetName() == "HumanoidRootPart")
				root = part.address;
			if (root && hum)
				break;
		}

		if (!root && hum)
		{
			std::uint64_t r = Cheat::Humanoid(hum).GetRootPartAddress();
			if (g_Memory.IsValid(r))
				root = r;
		}

		g_cache.root = root;
		g_cache.humanoid = hum;
		g_cache.character = chara;
	}

	else
	{
		g_cache.root = 0;
		g_cache.humanoid = 0;
		g_cache.character = 0;
	}
}

}
}
}
