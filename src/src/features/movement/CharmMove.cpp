#include "pch.h"
#include "CharmMove.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "renderer/Renderer.h"
#include "app/Settings.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>

#undef GetClassName

namespace {

// ---------- shared helpers ----------

bool roblox_focused()
{
	const HWND wnd = Cheat::Renderer::GetGameHwnd();
	return wnd && ::IsWindow(wnd) && ::GetForegroundWindow() == wnd;
}

bool key_gate(int key, int mode, bool& tog, bool& was)
{
	// always / no bind -> always on
	if (mode == 2 || key == 0)
		return true;

	bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
	if (mode == 1)
	{
		if (down && !was)
			tog = !tog;
		was = down;
		return tog;
	}

	was = down;
	return down;
}

std::uint64_t world_instance()
{
	if (!Cheat::Globals::Workspace || !g_Memory.IsValid(Cheat::Globals::Workspace->address))
		return 0;
	return g_Memory.Read<std::uint64_t>(
		Cheat::Globals::Workspace->address + ::Workspace::World);
}

// ---------- Gravity override ----------

std::atomic<bool> g_grav_run{ false };
std::thread g_grav_th;

void gravity_loop()
{
	bool overriden = false;
	float backup = 0.0f;

	while (g_grav_run.load(std::memory_order_relaxed))
	{
		const bool on = Cheat::g_Settings.misc.gravity;
		// charm skips gravity override while fly is engaged (fly zeroes it)
		const bool blocked = Cheat::g_Settings.misc.fly;
		std::uint64_t world = world_instance();

		if (!on || blocked || !world)
		{
			if (overriden && world)
				g_Memory.Write<float>(world + ::World::Gravity, backup);
			overriden = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		if (!overriden)
		{
			backup = g_Memory.Read<float>(world + ::World::Gravity);
			overriden = true;
		}

		float val = Cheat::g_Settings.misc.gravity_value;
		if (std::fabs(g_Memory.Read<float>(world + ::World::Gravity) - val) > 0.01f)
			g_Memory.Write<float>(world + ::World::Gravity, val);

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	std::uint64_t world = world_instance();
	if (overriden && world)
		g_Memory.Write<float>(world + ::World::Gravity, backup);
}

// ---------- Tickrate manipulation ----------

std::atomic<bool> g_tr_run{ false };
std::thread g_tr_th;

void tickrate_loop()
{
	bool was_applied = false;
	constexpr float kRestoreTickrate = 240.0f;

	while (g_tr_run.load(std::memory_order_relaxed))
	{
		const bool on = Cheat::g_Settings.misc.tickrate;
		std::uint64_t world = world_instance();

		if (!on || !world)
		{
			// restoring to 240 when turned off (as requested)
			if (was_applied && world)
				g_Memory.Write<float>(world + ::World::worldStepsPerSec, kRestoreTickrate);
			was_applied = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		float val = Cheat::g_Settings.misc.tickrate_value;
		if (val < 1.0f) val = 1.0f;
		if (val > 2000.0f) val = 2000.0f;
		if (std::fabs(g_Memory.Read<float>(world + ::World::worldStepsPerSec) - val) > 0.01f)
		{
			g_Memory.Write<float>(world + ::World::worldStepsPerSec, val);
			was_applied = true;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::uint64_t world = world_instance();
	if (was_applied && world)
		g_Memory.Write<float>(world + ::World::worldStepsPerSec, kRestoreTickrate);
}

} // namespace

void Cheat::Features::CharmMove::GravityStart()
{
	if (g_grav_run.load())
		return;
	g_grav_run.store(true);
	g_grav_th = std::thread(gravity_loop);
}

void Cheat::Features::CharmMove::GravityStop()
{
	g_grav_run.store(false);
	if (g_grav_th.joinable())
		g_grav_th.join();
}

void Cheat::Features::CharmMove::TickrateStart()
{
	if (g_tr_run.load())
		return;
	g_tr_run.store(true);
	g_tr_th = std::thread(tickrate_loop);
}

void Cheat::Features::CharmMove::TickrateStop()
{
	g_tr_run.store(false);
	if (g_tr_th.joinable())
		g_tr_th.join();
}

