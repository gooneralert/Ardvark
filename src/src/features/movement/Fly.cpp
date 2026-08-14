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
std::thread g_fly_th;

// Faithful port of FoulzExternal flight.cs:
//   method 0 = "Position" -> accumulate position, write Position + zero velocity
//   method 1 = "Velocity" -> write AssemblyLinearVelocity (full 3D camera basis)
void fly_loop()
{
	Vector3 fly_pos{};
	bool has_fly_pos = false;
	bool tog = false;
	bool was_key = false;

	fly_snap cached{};
	auto last_res = clock::now() - std::chrono::seconds(1);
	const auto clock0 = clock::now();
	double prev = 0.0;

	auto elapsed_s = [&]() -> double
	{
		return std::chrono::duration<double>(clock::now() - clock0).count();
	};

	while (g_fly_run.load(std::memory_order_relaxed))
	{
		if (!Cheat::g_Settings.misc.fly)
		{
			tog = false;
			was_key = false;
			has_fly_pos = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		const bool focus = roblox_focused();
		const bool can_tog = focus || Cheat::g_Settings.misc.fly_key == 0;
		bool key_on = false;
		if (can_tog)
		{
			key_on = key_gate(
				Cheat::g_Settings.misc.fly_key,
				Cheat::g_Settings.misc.fly_key_mode,
				tog,
				was_key);
		}

		if (!key_on)
		{
			prev = elapsed_s();
			has_fly_pos = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}

		const auto now = clock::now();
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

		if (cached.address == 0 || !cached.prim || !cached.cam)
		{
			prev = elapsed_s();
			has_fly_pos = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			continue;
		}

		// timing (Math.Clamp like flight.cs)
		double now_s = elapsed_s();
		float dt = (float)(now_s - prev);
		if (dt < 0.0001f) dt = 0.0001f;
		if (dt > 0.05f) dt = 0.05f;
		prev = now_s;

		mat3 rot{};
		if (!raw_read(cached.cam + ::Camera::Rotation, &rot, sizeof(rot)))
		{
			prev = elapsed_s();
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			continue;
		}

		// camera look projected onto the XZ plane (yaw only)
		const float lookX = -rot._13;   // -r02
		const float lookZ = -rot._33;   // -r22
		const float yawLen = std::sqrt(lookX * lookX + lookZ * lookZ);

		Vector3 forward, right;
		if (yawLen > 0.001f)
		{
			forward = Vector3(lookX / yawLen, 0.0f, lookZ / yawLen);
			right = Vector3(-forward.z, 0.0f, forward.x);
		}
		else
		{
			forward = Vector3(0.0f, 0.0f, -1.0f);
			right = Vector3(1.0f, 0.0f, 0.0f);
		}
		const Vector3 worldUp(0.0f, 1.0f, 0.0f);

		if (!has_fly_pos)
		{
			fly_pos = peek<Vector3>(cached.prim + ::Primitive::Position);
			has_fly_pos = true;
		}

		const float speed = Cheat::g_Settings.misc.fly_speed;
		auto held = [](int vk) -> bool
		{
			return (GetAsyncKeyState(vk) & 0x8000) != 0;
		};

		Vector3 moveDir{};
		if (focus)
		{
			if (held(0x57)) moveDir += forward;   // W
			if (held(0x53)) moveDir -= forward;   // S
			if (held(0x41)) moveDir -= right;     // A
			if (held(0x44)) moveDir += right;     // D
			if (held(0x20)) moveDir += worldUp;   // Space
			if (held(0xA0)) moveDir -= worldUp;   // LShift
		}

		const bool moving = moveDir.Length() > 0.001f;
		const int method = Cheat::g_Settings.misc.fly_method;

		if (method == 0)
		{
			// Position-based (accumulate position)
			if (moving)
			{
				moveDir.Normalize();
				fly_pos = fly_pos + moveDir * speed * dt;
			}
			poke(cached.prim + ::Primitive::Position, fly_pos);
			Vector3 zero{};
			poke(cached.prim + ::Primitive::AssemblyLinearVelocity, zero);
		}
		else
		{
			// Velocity-based
			Vector3 velocity{};
			if (moving)
			{
				moveDir.Normalize();
				velocity = moveDir * speed;
			}
			poke(cached.prim + ::Primitive::AssemblyLinearVelocity, velocity);

			// full 3D camera direction override so WASD works when looking up/down
			Vector3 fullForward(-rot._13, -rot._23, -rot._33);
			Vector3 fullRight(rot._11, rot._21, rot._31);
			Vector3 fullUp(rot._12, rot._22, rot._32);
			Vector3 vel3d{};
			if (focus)
			{
				if (held(0x57)) vel3d += fullForward;   // W
				if (held(0x53)) vel3d -= fullForward;   // S
				if (held(0x41)) vel3d -= fullRight;     // A
				if (held(0x44)) vel3d += fullRight;     // D
				if (held(0x20)) vel3d += fullUp;        // Space
				if (held(0xA0)) vel3d -= fullUp;        // Shift
			}
			if (vel3d.Length() > 0.001f)
			{
				vel3d.Normalize();
				poke(cached.prim + ::Primitive::AssemblyLinearVelocity, vel3d * speed);
			}
		}

		Vector3 zero{};
		poke(cached.prim + ::Primitive::AssemblyAngularVelocity, zero);

		// fake shift lock: rotate character to face camera yaw
		if (yawLen > 0.001f)
		{
			mat3 cr;
			cr._11 = -forward.z; cr._12 = 0.0f; cr._13 = -forward.x;
			cr._21 = 0.0f;        cr._22 = 1.0f; cr._23 = 0.0f;
			cr._31 =  forward.x;  cr._32 = 0.0f; cr._33 = -forward.z;
			poke(cached.prim + ::Primitive::Rotation, cr);
		}

		// noclip while flying
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
}

} // anonymous namespace

void Cheat::Features::Fly::Start()
{
	if (g_fly_run.load())
		return;
	g_fly_run.store(true);
	g_fly_th = std::thread(fly_loop);
}

void Cheat::Features::Fly::Stop()
{
	g_fly_run.store(false);
	if (g_fly_th.joinable())
		g_fly_th.join();
}
