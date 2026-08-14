#include "pch.h"
#include "HitboxExpander.h"
#include "HitboxJobs.h"

#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/player/PlayerHandler.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"

#include <cmath>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Cheat {
namespace Features {
namespace HitboxExpander {

namespace {

std::mutex g_mtx;
std::unordered_map<std::uint64_t, Vector3> g_orig; // оригинал size, ESP отсюда
std::unordered_set<std::uint64_t> g_active; // сейчас раздуты
bool g_was_on = false;

// без лока — вызывающий уже держит g_mtx или один
void RestoreOneLocked(std::uint64_t addr)
{
	auto it = g_orig.find(addr);
	if (it == g_orig.end())
	{
		return;
	}

	Vector3 sz = it->second;
	g_orig.erase(it);
	g_active.erase(addr);

	if (g_Memory.IsValid(addr))
	{
		BasePart(addr).SetSize(sz);
	}
}

void RestoreAll()
{
	std::unordered_map<std::uint64_t, Vector3> copy;
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		copy.swap(g_orig);
		g_active.clear();
	}

	for (const auto& [addr, sz] : copy)
	{
		if (g_Memory.IsValid(addr))
		{
			BasePart(addr).SetSize(sz);
		}
	}
}

void ApplyPart(std::uint64_t addr, float scale)
{
	if (!g_Memory.IsValid(addr))
	{
		return;
	}

	BasePart bp(addr);
	Vector3 cur = bp.GetSize();
	if (!std::isfinite(cur.x) || !std::isfinite(cur.y) || !std::isfinite(cur.z))
	{
		return;
	}

	Vector3 orig{};
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		auto it = g_orig.find(addr);
		if (it == g_orig.end())
		{
			// первый раз — текущий = оригинал (ещё не раздували)
			g_orig[addr] = cur;
			orig = cur;
		}

		else
		{
			orig = it->second;
		}
		g_active.insert(addr);
	}

	Vector3 want(
		orig.x * scale,
		orig.y * scale,
		orig.z * scale);

	float dx = cur.x - want.x;
	float dy = cur.y - want.y;
	float dz = cur.z - want.z;
	if (dx * dx + dy * dy + dz * dz < 0.0001f)
	{
		return;
	}

	bp.SetSize(want);
}

} // namespace

void CollectJobs(const PlayerCache& c, std::vector<PartJob>& out)
{
	auto& hb = g_Settings.hitbox;
	float scale = hb.scale;
	if (scale < 0.1f)
	{
		scale = 0.1f;
	}

	if (scale > 50.f)
	{
		scale = 50.f;
	}

	auto add = [&](const std::shared_ptr<Instance>& p)
	{
		if (!p || !g_Memory.IsValid(p->address))
		{
			return;
		}

		out.push_back({ p, scale });
	};

	if (hb.part == Settings::HB_HEAD)
	{
		add(c.head);
	}

	else if (hb.part == Settings::HB_HRP)
	{
		add(c.humanoidRootPart);
	}

	else if (hb.part == Settings::HB_TORSO)
	{
		add(c.upperTorso);
		add(c.lowerTorso);
	}

	else if (hb.part == Settings::HB_ARMS)
	{
		add(c.leftUpperArm);
		add(c.leftLowerArm);
		add(c.leftHand);
		add(c.rightUpperArm);
		add(c.rightLowerArm);
		add(c.rightHand);
	}

	else if (hb.part == Settings::HB_LEGS)
	{
		add(c.leftUpperLeg);
		add(c.leftLowerLeg);
		add(c.leftFoot);
		add(c.rightUpperLeg);
		add(c.rightLowerLeg);
		add(c.rightFoot);
	}
}

Vector3 SizeForEsp(std::uint64_t part_addr, const Vector3& current)
{
	if (!part_addr)
	{
		return current;
	}

	std::lock_guard<std::mutex> lk(g_mtx);
	auto it = g_orig.find(part_addr);
	if (it == g_orig.end())
	{
		return current;
	}

	return it->second;
}

void Tick()
{
	auto& hb = g_Settings.hitbox;
	if (!hb.enabled)
	{
		if (g_was_on)
		{
			RestoreAll();
			g_was_on = false;
		}

		return;
	}

	g_was_on = true;

	if (!g_Memory.IsAttached() || !Globals::Players)
	{
		return;
	}

	std::uint64_t lp = g_Memory.Read<std::uint64_t>(
		Globals::Players->address + ::Player::LocalPlayer);

	std::uint64_t local_team = 0;
	if (g_Settings.misc.teamcheck)
	{
		local_team = PlayerHandler::LocalTeamFolder();
	}

	std::unordered_set<std::uint64_t> keep;

	PlayerHandler::ForEachPlayer([&](const PlayerCache& c)
	{
		if (c.address == lp)
		{
			return;
		}

		if (g_Settings.misc.teamcheck && PlayerHandler::IsTeammate(c, local_team))
		{
			return;
		}

		std::vector<PartJob> jobs;
		CollectJobs(c, jobs);
		for (const PartJob& j : jobs)
		{
			ApplyPart(j.part->address, j.scale);
			keep.insert(j.part->address);
		}
	});

	// сняли галку с парты — рестор
	std::vector<std::uint64_t> drop;
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		for (std::uint64_t addr : g_active)
		{
			if (!keep.count(addr))
			{
				drop.push_back(addr);
			}
		}
	}

	{
		std::lock_guard<std::mutex> lk(g_mtx);
		for (std::uint64_t addr : drop)
		{
			RestoreOneLocked(addr);
		}
	}
}

void Shutdown()
{
	RestoreAll();
	g_was_on = false;
}

}
}
}
