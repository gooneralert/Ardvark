#include "pch.h"
#include "CharMods.h"
#include "AnimPacks.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#undef GetClassName

namespace {

using clock = std::chrono::steady_clock;

// Build the Roblox asset URL for an animation id.
std::string anim_url(std::uint64_t id)
{
	return "http://www.roblox.com/asset/?id=" + std::to_string(id);
}

// charm's WriteString: writes a std::string (Roblox content string layout:
// buffer ptr @+0, length @+0x10, capacity @+0x18) into an existing string.
void write_string(std::uint64_t address, const std::string& value)
{
	if (!address || !g_Memory.IsValid(address) || value.empty())
		return;

	const auto current_len = g_Memory.Read<std::int64_t>(address + 0x18);
	std::uint64_t buf = current_len >= 16
		? g_Memory.Read<std::uint64_t>(address)
		: address;
	if (!buf || !g_Memory.IsValid(buf))
		return;

	for (size_t i = 0; i < value.size() && i < 200; i++)
		g_Memory.Write<char>(buf + i, value[i]);
	g_Memory.Write<char>(buf + value.size(), '\0');
	g_Memory.Write<std::int64_t>(address + 0x10, static_cast<std::int64_t>(value.size()));
}

std::uint64_t local_chara()
{
	if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
		return 0;
	std::uint64_t lp = g_Memory.Read<std::uint64_t>(
		Cheat::Globals::Players->address + ::Player::LocalPlayer);
	if (!g_Memory.IsValid(lp))
		return 0;
	std::uint64_t c = g_Memory.Read<std::uint64_t>(
		lp + ::Player::ModelInstance);
	return g_Memory.IsValid(c) ? c : 0;
}

std::shared_ptr<Cheat::Instance> find_child(std::uint64_t parent, const char* name)
{
	for (const auto& c : Cheat::Instance(parent).GetChildren())
	{
		if (c.GetName() == name)
			return std::make_shared<Cheat::Instance>(c.address);
	}
	return nullptr;
}

std::string read_string(std::uint64_t addr)
{
	if (!g_Memory.IsValid(addr))
		return {};
	const auto len = g_Memory.Read<std::int64_t>(addr + 0x10);
	if (len < 0 || len > 300)
		return {};
	std::uint64_t buf = len >= 16 ? g_Memory.Read<std::uint64_t>(addr) : addr;
	if (!g_Memory.IsValid(buf))
		return {};
	std::string out(static_cast<std::size_t>(len), '\0');
	for (std::int64_t i = 0; i < len; i++)
		out[static_cast<std::size_t>(i)] = g_Memory.Read<char>(buf + i);
	return out;
}

struct HeadlessCache
{
	std::uint64_t chara = 0;
	bool have = false;
	float trans = 0.0f;
};
HeadlessCache g_headless_cache;

struct KorbloxCache
{
	std::uint64_t chara = 0;
	bool have_trans = false;
	float foot_t = 0.0f;
	float leg_t = 0.0f;
	bool have_mesh = false;
	std::string mesh;
};
KorbloxCache g_korblox_cache;

void apply_animation_pack()
{
	std::uint64_t chara = local_chara();
	if (!chara)
		return;

	auto animate = find_child(chara, "Animate");
	if (!animate || !animate->address)
		return;

	const int pack = Cheat::g_Settings.misc.anim_pack;
	if (pack < 0 || pack >= Cheat::Features::CharMods::kAnimPackCount)
		return;

	const Cheat::Features::CharMods::AnimPackDef& def =
		Cheat::Features::CharMods::kAnimPacks[pack];

	for (int i = 0; i < def.count; i++)
	{
		const Cheat::Features::CharMods::AnimSlotDef& s = def.slots[i];
		auto g = find_child(animate->address, s.group);
		if (!g || !g->address)
			continue;
		auto a = find_child(g->address, s.anim);
		if (!a || !a->address)
			continue;
		write_string(a->address + ::Misc::AnimationId, anim_url(s.id));
	}
}

void fake_headless()
{
	std::uint64_t chara = local_chara();
	if (!chara)
		return;

	auto head = find_child(chara, "Head");
	if (!head || !head->address)
		return;

	Cheat::BasePart hp(head->address);
	if (!g_headless_cache.have || g_headless_cache.chara != chara)
	{
		g_headless_cache.chara = chara;
		g_headless_cache.trans = hp.GetTransparency();
		g_headless_cache.have = true;
	}
	hp.SetTransparency(1.0f);
}

void reset_headless()
{
	std::uint64_t chara = local_chara();
	if (!chara)
		return;

	auto head = find_child(chara, "Head");
	if (head && head->address)
	{
		Cheat::BasePart hp(head->address);
		if (g_headless_cache.have && g_headless_cache.chara == chara)
			hp.SetTransparency(g_headless_cache.trans);
		else
			hp.SetTransparency(0.0f);
	}
	g_headless_cache = {};
}

void korblox()
{
	std::uint64_t chara = local_chara();
	if (!chara)
		return;

	const bool first = g_korblox_cache.chara != chara;
	if (first)
		g_korblox_cache = {};

	auto set_trans = [&](const char* name, bool capture)
	{
		auto p = find_child(chara, name);
		if (p && p->address)
		{
			Cheat::BasePart bp(p->address);
			if (capture)
			{
				const float t = bp.GetTransparency();
				if (std::string(name) == "RightFoot") g_korblox_cache.foot_t = t;
				else g_korblox_cache.leg_t = t;
			}
			bp.SetTransparency(1.0f);
		}
	};

	// resolve the upper-leg mesh field (write to the right class offset)
	std::uint64_t mesh_field = 0;
	auto leg = find_child(chara, "RightUpperLeg");
	if (leg && leg->address)
	{
		if (leg->GetClassName() == "MeshPart")
			mesh_field = leg->address + ::MeshPart::MeshId;
		else
		{
			for (const auto& c : leg->GetChildren())
			{
				const std::string cn = c.GetClassName();
				if (cn == "SpecialMesh" || cn == "FileMesh")
				{
					mesh_field = c.address + ::SpecialMesh::MeshId;
					break;
				}
			}
		}
	}

	if (first)
	{
		g_korblox_cache.chara = chara;
		g_korblox_cache.have_trans = true;
		g_korblox_cache.have_mesh = (mesh_field != 0);
		if (mesh_field)
			g_korblox_cache.mesh = read_string(mesh_field);
	}

	set_trans("RightFoot", first);
	set_trans("RightLowerLeg", first);

	if (mesh_field)
		write_string(mesh_field, "https://assetdelivery.roblox.com/v1/asset/?id=9598310133");
}

void reset_korblox()
{
	std::uint64_t chara = local_chara();
	if (!chara)
		return;

	const bool use_cache = g_korblox_cache.have_trans && g_korblox_cache.chara == chara;

	auto set_restore = [&](const char* name, float val)
	{
		auto p = find_child(chara, name);
		if (p && p->address)
			Cheat::BasePart(p->address).SetTransparency(val);
	};
	set_restore("RightFoot", use_cache ? g_korblox_cache.foot_t : 0.0f);
	set_restore("RightLowerLeg", use_cache ? g_korblox_cache.leg_t : 0.0f);

	std::uint64_t mesh_field = 0;
	auto leg = find_child(chara, "RightUpperLeg");
	if (leg && leg->address)
	{
		if (leg->GetClassName() == "MeshPart")
			mesh_field = leg->address + ::MeshPart::MeshId;
		else
		{
			for (const auto& c : leg->GetChildren())
			{
				const std::string cn = c.GetClassName();
				if (cn == "SpecialMesh" || cn == "FileMesh")
				{
					mesh_field = c.address + ::SpecialMesh::MeshId;
					break;
				}
			}
		}
	}
	if (mesh_field && use_cache && g_korblox_cache.have_mesh)
		write_string(mesh_field, g_korblox_cache.mesh);

	g_korblox_cache = {};
}

std::atomic<bool> g_run{ false };
std::thread g_th;

void charmods_loop()
{
	auto last_anim = clock::now() - std::chrono::seconds(5);

	while (g_run.load())
	{
		auto now = clock::now();

		if (Cheat::g_Settings.misc.reset_fake_headless)
		{
			reset_headless();
			Cheat::g_Settings.misc.reset_fake_headless = false;
		}

		if (Cheat::g_Settings.misc.reset_korblox)
		{
			reset_korblox();
			Cheat::g_Settings.misc.reset_korblox = false;
		}

		if (Cheat::g_Settings.misc.anim_changer &&
			(now - last_anim) > std::chrono::milliseconds(1500))
		{
			apply_animation_pack();
			last_anim = now;
		}

		if (Cheat::g_Settings.misc.fake_headless)
			fake_headless();

		if (Cheat::g_Settings.misc.korblox)
			korblox();

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

} // anonymous namespace

void Cheat::Features::CharMods::Start()
{
	if (g_run.load())
		return;
	g_run.store(true);
	g_th = std::thread(charmods_loop);
}

void Cheat::Features::CharMods::Stop()
{
	g_run.store(false);
	if (g_th.joinable())
		g_th.join();
}

int Cheat::Features::CharMods::AnimPackCount()
{
	return kAnimPackCount;
}

const char* const* Cheat::Features::CharMods::AnimPackNames()
{
	static const char* names[kAnimPackCount] = {};
	static bool built = false;
	if (!built)
	{
		for (int i = 0; i < kAnimPackCount; i++)
			names[i] = kAnimPacks[i].name;
		built = true;
	}
	return names;
}
