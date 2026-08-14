#include "pch.h"
#include "Speed.h"
#include "SpeedHelpers.h"

#include "app/Settings.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

#undef GetClassName

namespace {

using namespace Cheat::Features::Speed::helpers;

std::atomic<bool> g_run{ false };
std::thread g_th;

// спид крч либо ws либо позицию дёргаем
void speed_loop()
{
	bool was_on = false;
	bool got_bak = false;
	std::uint64_t bak_hum = 0;
	float bak_ws = 16.f;
	bool tog = false;
	bool was_key = false;
	auto last = std::chrono::steady_clock::now();

	auto clear_bak = [&]()
	{
		got_bak = false;
		bak_hum = 0;
		bak_ws = 16.f;
	};

	auto restore_ws = [&]()
	{
		if (got_bak && g_Memory.IsValid(bak_hum))
			write_ws(bak_hum, bak_ws);
		clear_bak();
	};

	auto backup_ws = [&](std::uint64_t hum)
	{
		if (!g_Memory.IsValid(hum))
			return;

		bool changed = !got_bak || bak_hum != hum;
		if (!changed)
			return;

		restore_ws();
		bak_ws = read_ws(hum);
		bak_hum = hum;
		got_bak = true;
	};

	while (g_run.load())
	{
		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - last).count();
		if (dt < 0.0001f)
			dt = 0.0001f;
		last = now;

		bool in_chat = chat_focused();
		bool can_tog = roblox_focused() && !in_chat;

		if (!Cheat::g_Settings.misc.walkspeed)
		{
			tog = false;
			was_key = false;
			if (was_on)
				restore_ws();
			was_on = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		bool key_on = false;
		if (can_tog || Cheat::g_Settings.misc.walkspeed_key == 0)
		{
			key_on = key_gate(
				Cheat::g_Settings.misc.walkspeed_key,
				Cheat::g_Settings.misc.walkspeed_key_mode,
				tog,
				was_key);
		}

		else if (in_chat && Cheat::g_Settings.misc.walkspeed_key)
		{
			GetAsyncKeyState(Cheat::g_Settings.misc.walkspeed_key);
			key_on = false;
		}

		float ws_val = Cheat::g_Settings.misc.walkspeed_value;
		if (ws_val < 1.f) ws_val = 1.f;
		if (ws_val > 500.f) ws_val = 500.f;
		Cheat::g_Settings.misc.walkspeed_value = ws_val;

		int ws_mode_i = Cheat::g_Settings.misc.walkspeed_mode;
		if (ws_mode_i < 0) ws_mode_i = 0;
		if (ws_mode_i > 1) ws_mode_i = 1;
		Cheat::g_Settings.misc.walkspeed_mode = ws_mode_i;

		if (!key_on)
		{
			if (was_on)
				restore_ws();
			was_on = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		std::uint64_t hrp = 0, prim = 0, hum = 0;
		bool ok = resolve_local(hrp, prim, hum);
		auto mode = (ws_mode)Cheat::g_Settings.misc.walkspeed_mode;
		float spd = Cheat::g_Settings.misc.walkspeed_value;

		if (!ok)
		{
			if (was_on)
				restore_ws();
			was_on = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		backup_ws(hum);

		// 0 = pos spam, 1 = чистый walkspeed
		if (mode == ws_mode::humanoid)
		{
			write_ws(hum, spd);
		}

		else
		{
			write_ws(hum, 0.1f);

			Vector3 want{};
			if (move_vel(hum, spd, want))
			{
				float cdt = dt;
				if (cdt < 0.001f) cdt = 0.001f;
				if (cdt > 0.05f) cdt = 0.05f;
				step_xz(prim, want, cdt, 16);
			}
		}

		was_on = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	restore_ws();
}

}

void Cheat::Features::Speed::Start()
{
	if (g_run.load())
		return;

	g_run.store(true);
	g_th = std::thread(speed_loop);
}

void Cheat::Features::Speed::Stop()
{
	g_run.store(false);
	if (g_th.joinable())
		g_th.join();
}
