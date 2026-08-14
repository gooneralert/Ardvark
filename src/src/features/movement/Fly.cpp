#include "pch.h"
#include "Fly.h"
#include "FlyHelpers.h"

#include "app/Settings.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>

#undef GetClassName

namespace {

using clock = std::chrono::steady_clock;
using namespace Cheat::Features::Fly::helpers;

std::atomic<bool> g_fly_run{ false };
std::atomic<bool> g_grav_run{ false };
std::atomic<bool> g_fly_on{ false };
std::thread g_fly_th;
std::thread g_grav_th;

// velocity луп, гравитация в другом треде
void fly_loop()
{
	bool was_on = false;
	Vector3 cur_vel{};

	bool tog = false;
	bool was_key = false;

	fly_snap cached{};
	auto last_res = clock::now() - std::chrono::seconds(1);

	while (g_fly_run.load(std::memory_order_relaxed))
	{
		auto now = clock::now();

		if (!Cheat::g_Settings.misc.fly)
		{
			tog = false;
			was_key = false;
			if (was_on && cached.prim)
			{
				zero_vel(cached.prim);
				cur_vel = {};
			}
			g_fly_on.store(false, std::memory_order_relaxed);
			was_on = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		bool focus = roblox_focused();
		bool can_tog = focus || Cheat::g_Settings.misc.fly_key == 0;

		bool key_on = false;
		if (can_tog)
		{
			key_on = key_gate(
				Cheat::g_Settings.misc.fly_key,
				Cheat::g_Settings.misc.fly_key_mode,
				tog,
				was_key);
		}

		if (!cached.hrp || !cached.cam)
		{
			if ((now - last_res) > std::chrono::milliseconds(50))
			{
				cached = grab_local();
				last_res = now;
			}
		}

		else if ((now - last_res) > std::chrono::milliseconds(250))
		{
			touch_ptrs(cached);
			last_res = now;
		}

		bool flying = Cheat::g_Settings.misc.fly && key_on;
		bool idle = !flying;
		g_fly_on.store(flying, std::memory_order_relaxed);

		if (cached.address == 0 || !cached.prim || !cached.cam)
		{
			if (was_on && cached.prim)
			{
				zero_vel(cached.prim);
				cur_vel = {};
			}

			g_fly_on.store(false, std::memory_order_relaxed);
			was_on = false;

			if (idle)
				std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		mat3 rot{};
		if (!raw_read(cached.cam + ::Camera::Rotation, &rot, sizeof(rot)))
		{
			if (was_on && cached.prim)
			{
				zero_vel(cached.prim);
				cur_vel = {};
			}
			g_fly_on.store(false, std::memory_order_relaxed);
			was_on = false;
			if (idle)
				std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		Vector3 fwd, right;
		// ── match external flight.cs ──────────────────────────────
		// Horizontal movement uses the camera YAW projected onto the XZ
		// plane (safe when looking straight up/down); vertical is separate
		// (Space up / Shift down). Mirrors the working FoulzExternal impl.
		const float lookX = -rot._13;              // -r02
		const float lookZ = -rot._33;              // -r22
		const float yawLen = std::sqrt(lookX * lookX + lookZ * lookZ);

		if (yawLen > 0.001f)
		{
			fwd = Vector3(lookX / yawLen, 0.0f, lookZ / yawLen);
			right = Vector3(-fwd.z, 0.0f, fwd.x);  // right = forward cross worldUp
		}
		else
		{
			fwd = Vector3(0.0f, 0.0f, -1.0f);
			right = Vector3(1.0f, 0.0f, 0.0f);
		}
		const Vector3 worldUp(0.0f, 1.0f, 0.0f);

		if (!flying)
		{
			if (was_on)
			{
				zero_vel(cached.prim);
				cur_vel = {};
			}

			was_on = false;

			if (idle)
				std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		was_on = true;

		kb_layout lay = detect_layout(Cheat::Renderer::GetGameHwnd());
		int fwd_vk = lay == kb_layout::azerty ? 'Z' : 'W';
		int left_vk = lay == kb_layout::azerty ? 'Q' : 'A';

		auto held = [](int vk) -> bool
		{
			return (GetAsyncKeyState(vk) & 0x8000) != 0;
		};

		Vector3 moveDir{};
		float spd = Cheat::g_Settings.misc.fly_speed;
		if (spd < 0.f) spd = 0.f;

		if (focus || Cheat::g_Settings.misc.fly_key == 0)
		{
			if (held(fwd_vk))    moveDir += fwd;               // W  forward
			if (held('S'))       moveDir -= fwd;               // S  backward
			if (held(left_vk))   moveDir -= right;             // A  strafe left
			if (held('D'))       moveDir += right;             // D  strafe right
			if (held(VK_SPACE))  moveDir += worldUp;           // Space  up
			if (held(VK_LSHIFT)) moveDir -= worldUp;           // Shift  down
		}

		Vector3 want{};
		if (moveDir.LengthSquared() > 1e-6f)
		{
			moveDir.Normalize();
			want = moveDir * spd;
		}

		cur_vel = want;
		set_vel(cached.prim, cur_vel); // linear velocity + zero angular

		// ── fake shift lock: rotate char to face camera yaw ────────
		// Removes the dependency on real shift lock, so flight is smooth
		// both in AND out of shift lock (matches external flight.cs).
		if (yawLen > 0.001f)
		{
			mat3 cr;
			cr._11 = -fwd.z; cr._12 = 0.0f; cr._13 = -fwd.x;
			cr._21 = 0.0f;   cr._22 = 1.0f; cr._23 = 0.0f;
			cr._31 =  fwd.x; cr._32 = 0.0f; cr._33 = -fwd.z;
			poke(cached.prim + ::Primitive::Rotation, cr);
		}

		// ── noclip while flying: clear CanCollide so the physics engine
		// doesn't damp the velocity against the floor / geometry. ──
		{
			std::uint8_t flags = peek<std::uint8_t>(cached.prim + ::Primitive::Flags);
			std::uint8_t clean = (std::uint8_t)(flags & ~(std::uint8_t)::PrimitiveFlags::CanCollide);
			if (flags != clean)
				poke<std::uint8_t>(cached.prim + ::Primitive::Flags, clean);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	{
		auto s = grab_local();
		if (s.address != 0 && s.prim)
			zero_vel(s.prim);
	}
	g_fly_on.store(false, std::memory_order_relaxed);
}

// гравитацию в ноль пока летаем
void grav_loop()
{
	bool overriden = false;
	float backup = 0.0f;

	while (g_grav_run.load(std::memory_order_relaxed))
	{
		if (!Cheat::g_Settings.misc.fly)
		{
			if (overriden &&
				Cheat::Globals::Workspace &&
				Cheat::Globals::Workspace->address)
			{
				auto world = peek<std::uint64_t>(
					Cheat::Globals::Workspace->address + ::Workspace::World);
				if (world)
					poke(world + ::World::Gravity, backup);
				overriden = false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		bool flying = g_fly_on.load(std::memory_order_relaxed);
		bool can =
			Cheat::Globals::Workspace &&
			Cheat::Globals::Workspace->address;

		if (flying && can)
		{
			auto world = peek<std::uint64_t>(
				Cheat::Globals::Workspace->address + ::Workspace::World);
			if (world)
			{
				if (!overriden)
				{
					backup = peek<float>(world + ::World::Gravity);
					overriden = true;
				}
				float z = 0.0f;
				poke(world + ::World::Gravity, z);
			}
		}

		else if (!flying && overriden && can)
		{
			auto world = peek<std::uint64_t>(
				Cheat::Globals::Workspace->address + ::Workspace::World);
			if (world)
				poke(world + ::World::Gravity, backup);
			overriden = false;
		}

		if (flying)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		else
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}

	if (overriden &&
		Cheat::Globals::Workspace &&
		Cheat::Globals::Workspace->address)
	{
		auto world = peek<std::uint64_t>(
			Cheat::Globals::Workspace->address + ::Workspace::World);
		if (world)
			poke(world + ::World::Gravity, backup);
	}
}

}

void Cheat::Features::Fly::Start()
{
	if (!g_fly_run.load(std::memory_order_relaxed))
	{
		g_fly_run = true;
		g_fly_th = std::thread(fly_loop);
	}

	if (!g_grav_run.load(std::memory_order_relaxed))
	{
		g_grav_run = true;
		g_grav_th = std::thread(grav_loop);
	}
}

void Cheat::Features::Fly::Stop()
{
	g_fly_run.store(false, std::memory_order_relaxed);
	g_grav_run.store(false, std::memory_order_relaxed);
	if (g_fly_th.joinable())
		g_fly_th.join();
	if (g_grav_th.joinable())
		g_grav_th.join();
}
