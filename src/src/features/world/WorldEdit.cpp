#include "pch.h"
#include "WorldEdit.h"
#include "WorldSlots.h"
#include "core/memory/Memory.h"
#include "app/Settings.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace Cheat {
namespace Features {
namespace WorldEdit {
namespace {

std::atomic<bool> g_run{ false };

std::thread g_th_shadow;
std::thread g_th_time;
std::thread g_th_amb;
std::thread g_th_out;
std::thread g_th_bri;
std::thread g_th_expo;
std::thread g_th_light;
std::thread g_th_fog;
std::thread g_th_env;
std::thread g_th_shift;
std::thread g_th_atmo;
std::thread g_th_sky;
std::thread g_th_bloom;
std::thread g_th_cc;
std::thread g_th_cg;
std::thread g_th_dof;
std::thread g_th_terr;
std::thread g_th_skybox;

std::atomic<std::uint64_t> g_lighting{ 0 };
std::atomic<std::uint64_t> g_light_tick{ 0 };

std::uint64_t lighting()
{
	const std::uint64_t now = GetTickCount64();
	const std::uint64_t last = g_light_tick.load();
	std::uint64_t cur = g_lighting.load();

	if (now - last >= 400 || !g_Memory.IsValid(cur))
	{
		cur = WorldSlots::FindLighting();
		g_lighting.store(cur);
		g_light_tick.store(now);
	}

	return cur;
}

void sleep_ms(int ms)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

template <typename Fn, typename OnFn>
void feat_loop(Fn tick, OnFn is_on)
{
	while (g_run.load())
	{
		const bool on = is_on();
		const std::uint64_t L = lighting();
		if (g_Memory.IsValid(L))
			tick(L, false);

		sleep_ms(on ? 1 : 24);
	}

	const std::uint64_t L = lighting();
	if (g_Memory.IsValid(L))
		tick(L, true);
}

void join_one(std::thread& th)
{
	if (th.joinable())
		th.join();
}

}

void Start()
{
	if (g_run.exchange(true))
		return;

	g_th_shadow = std::thread([] {
		feat_loop(WorldSlots::TickNoShadow, [] { return Cheat::g_Settings.world.no_shadow; });
	});
	g_th_time = std::thread([] {
		feat_loop(WorldSlots::TickTime, [] { return Cheat::g_Settings.world.time_changer; });
	});
	g_th_amb = std::thread([] {
		feat_loop(WorldSlots::TickAmbient, [] { return Cheat::g_Settings.world.ambient; });
	});
	g_th_out = std::thread([] {
		feat_loop(WorldSlots::TickOutdoor, [] { return Cheat::g_Settings.world.outdoor; });
	});
	g_th_bri = std::thread([] {
		feat_loop(WorldSlots::TickBrightness, [] { return Cheat::g_Settings.world.brightness; });
	});
	g_th_expo = std::thread([] {
		feat_loop(WorldSlots::TickExposure, [] { return Cheat::g_Settings.world.exposure_on; });
	});
	g_th_light = std::thread([] {
		feat_loop(WorldSlots::TickLight, [] { return Cheat::g_Settings.world.light; });
	});
	g_th_fog = std::thread([] {
		feat_loop(WorldSlots::TickFog, [] { return Cheat::g_Settings.world.fog; });
	});
	g_th_env = std::thread([] {
		feat_loop(WorldSlots::TickEnv, [] { return Cheat::g_Settings.world.env; });
	});
	g_th_shift = std::thread([] {
		feat_loop(WorldSlots::TickColorShift, [] { return Cheat::g_Settings.world.color_shift; });
	});
	g_th_atmo = std::thread([] {
		feat_loop(WorldSlots::TickAtmosphere, [] { return Cheat::g_Settings.world.atmosphere; });
	});
	g_th_sky = std::thread([] {
		feat_loop(WorldSlots::TickSky, [] { return Cheat::g_Settings.world.sky; });
	});
	g_th_bloom = std::thread([] {
		feat_loop(WorldSlots::TickBloom, [] { return Cheat::g_Settings.world.bloom; });
	});
	g_th_cc = std::thread([] {
		feat_loop(WorldSlots::TickColorCorr, [] { return Cheat::g_Settings.world.color_corr; });
	});
	g_th_cg = std::thread([] {
		feat_loop(WorldSlots::TickColorGrade, [] { return Cheat::g_Settings.world.color_grade; });
	});
	g_th_dof = std::thread([] {
		feat_loop(WorldSlots::TickDof, [] { return Cheat::g_Settings.world.dof; });
	});
	g_th_terr = std::thread([] {
		feat_loop(WorldSlots::TickTerrain, [] { return Cheat::g_Settings.world.terrain; });
	});
	g_th_skybox = std::thread([] {
		feat_loop(WorldSlots::TickSkyboxChanger, [] { return Cheat::g_Settings.world.skybox_changer; });
	});
}

void Stop()
{
	if (!g_run.exchange(false))
		return;

	join_one(g_th_shadow);
	join_one(g_th_time);
	join_one(g_th_amb);
	join_one(g_th_out);
	join_one(g_th_bri);
	join_one(g_th_expo);
	join_one(g_th_light);
	join_one(g_th_fog);
	join_one(g_th_env);
	join_one(g_th_shift);
	join_one(g_th_atmo);
	join_one(g_th_sky);
	join_one(g_th_bloom);
	join_one(g_th_cc);
	join_one(g_th_cg);
	join_one(g_th_dof);
	join_one(g_th_terr);
	join_one(g_th_skybox);
}

} // namespace WorldEdit
} // namespace Features
} // namespace Cheat
