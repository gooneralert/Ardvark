#include "pch.h"
#include "Misc.h"
#include "features/movement/Fly.h"
#include "features/movement/Speed.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "features/world/WorldEdit.h"
#include "features/local/LocalMods.h"
#include "features/local/Btools.h"
#include "features/local/ThirdPerson.h"
#include "features/misc/HitboxExpander.h"
#include "app/Settings.h"
#include <Windows.h>
#include <timeapi.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>

#pragma comment(lib, "winmm.lib")

#undef GetClassName

namespace {

struct MiscCache {
    std::uint64_t datamodel = 0;
    std::uint64_t lighting = 0;
    std::uint64_t root = 0;
    std::uint64_t humanoid = 0;
    std::uint64_t character = 0;
    std::uint64_t last_refresh = 0;
};
MiscCache g_cache;

template <typename T>
bool write_if_new(std::uint64_t addr, T value)
{
	T cur = g_Memory.Read<T>(addr);
	if (cur == value)
		return false;
	g_Memory.Write<T>(addr, value);
	return true;
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

	if (!g_Memory.IsValid(g_cache.lighting))
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

bool cam_basis(Vector3& fwd, Vector3& right, Vector3& up)
{
	if (!Cheat::Globals::Workspace)
		return false;

	auto cam = Cheat::Globals::Workspace->GetCurrentCamera();
	if (!cam)
		return false;

	Cheat::Camera camera(cam->address);
	Matrix4x4 r = camera.GetRotation();

	fwd = Vector3(-r.m[0][2], -r.m[1][2], -r.m[2][2]);
	right = Vector3(r.m[0][0], r.m[1][0], r.m[2][0]);
	up = Vector3(r.m[0][1], r.m[1][1], r.m[2][1]);
	return true;
}

// фрикамера через CameraOffset (костыль, но живёт)
void freecam_tick(float dt)
{
	static bool s_active = false;
	static bool s_prev = false;
	static bool s_on = false;
	static Vector3 s_saved_off;
	static bool s_saved_plat = false;
	static bool s_saved_autorot = true;
	static Vector3 s_off;
	static Vector3 s_world;

	const auto& m = Cheat::g_Settings.misc;
	int key = m.freecam_key;

	bool want = false;
	if (m.freecam_mode == 2)
	{
		want = true;
		s_prev = false;
	}

	else if (key != 0)
	{
		bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
		if (m.freecam_mode == 1)
		{
			if (down && !s_prev)
				s_active = !s_active;
			want = s_active;
		}

		else
		{
			want = down;
		}
		s_prev = down;
	}

	else
	{
		s_active = false;
	}

	std::uint64_t hum = g_cache.humanoid;

	if (!want || !g_Memory.IsValid(hum))
	{
		if (s_on && g_Memory.IsValid(hum))
		{
			g_Memory.Write<Vector3>(hum + ::Humanoid::CameraOffset, s_saved_off);
			g_Memory.Write<bool>(hum + ::Humanoid::PlatformStand, s_saved_plat);
			g_Memory.Write<bool>(hum + ::Humanoid::AutoRotate, s_saved_autorot);
		}
		s_on = false;
		return;
	}

	if (!s_on)
	{
		s_on = true;
		s_saved_off = g_Memory.Read<Vector3>(hum + ::Humanoid::CameraOffset);
		s_saved_plat = g_Memory.Read<bool>(hum + ::Humanoid::PlatformStand);
		s_saved_autorot = g_Memory.Read<bool>(hum + ::Humanoid::AutoRotate);
		s_off = Vector3(0.f, 0.f, 0.f);
		s_world = Vector3(0.f, 0.f, 0.f);
	}

	g_Memory.Write<bool>(hum + ::Humanoid::PlatformStand, true);
	g_Memory.Write<bool>(hum + ::Humanoid::AutoRotate, false);

	if (g_Memory.IsValid(g_cache.root))
	{
		Cheat::BasePart root(g_cache.root);
		root.SetAssemblyLinearVelocity(Vector3(0.f, 0.f, 0.f));
	}

	Vector3 fwd, right, up;
	if (!cam_basis(fwd, right, up))
		return;

	Vector3 world_up(0.f, 1.f, 0.f);

	Vector3 dir(0.f, 0.f, 0.f);
	if (GetAsyncKeyState('W') & 0x8000) dir += fwd;
	if (GetAsyncKeyState('S') & 0x8000) dir -= fwd;
	if (GetAsyncKeyState('D') & 0x8000) dir += right;
	if (GetAsyncKeyState('A') & 0x8000) dir -= right;
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) dir += world_up;
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) dir -= world_up;

	float spd = m.freecam_speed;
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
		spd *= 3.0f;

	if (dir.LengthSquared() > 0.01f)
		s_world += dir.Normalized() * (spd * dt);

	Vector3 cr_r(1.f, 0.f, 0.f), cr_u(0.f, 1.f, 0.f), cr_b(0.f, 0.f, 1.f);
	if (g_Memory.IsValid(g_cache.root))
	{
		Cheat::BasePart root(g_cache.root);
		Matrix4x4 cr = root.GetRotation();
		cr_r = Vector3(cr.m[0][0], cr.m[1][0], cr.m[2][0]);
		cr_u = Vector3(cr.m[0][1], cr.m[1][1], cr.m[2][1]);
		cr_b = Vector3(cr.m[0][2], cr.m[1][2], cr.m[2][2]);
	}

	Vector3 local(
		s_world.Dot(cr_r),
		s_world.Dot(cr_u),
		s_world.Dot(cr_b));

	s_off = s_saved_off + local;
	g_Memory.Write<Vector3>(hum + ::Humanoid::CameraOffset, s_off);
}

}

// fps / fov / jump / freecam / мир
void Cheat::Features::Misc::Tick(float dt)
{
	if (!Cheat::Globals::InstanceDataModel.address)
		return;

	const auto& m = Cheat::g_Settings.misc;

	refresh_cache();

	{ /* fps unlock */
		static std::uint64_t s_sched = 0;
		static bool s_delay = false;
		static double s_orig = 0.0;
		static bool s_got_orig = false;
		static double s_last = 0.0;

		if (s_sched == 0)
		{
			static const uintptr_t base = g_Memory.GetModuleBase();
			if (base)
			{
				std::uint64_t sched = g_Memory.Read<std::uint64_t>(
					base + ::TaskScheduler::Pointer);
				if (g_Memory.IsValid(sched) &&
					g_Memory.IsWritable(sched + ::TaskScheduler::MaxFps, sizeof(double)))
				{
					double cur = g_Memory.Read<double>(
						sched + ::TaskScheduler::MaxFps);
					// иногда delay иногда fps, угадываем по диапазону
					if (cur > 0.0 && cur <= 1.0)
					{
						s_sched = sched;
						s_delay = true;
					}

					else if (cur >= 10.0 && cur <= 100000.0)
					{
						s_sched = sched;
						s_delay = false;
					}
				}
			}
		}

		if (s_sched)
		{
			std::uint64_t addr = s_sched + ::TaskScheduler::MaxFps;
			if (m.fps_unlock)
			{
				if (!s_got_orig)
				{
					s_orig = g_Memory.Read<double>(addr);
					s_got_orig = true;
				}

				int cap = m.fps_cap;
				if (cap < 30) cap = 30;

				double target = s_delay ? (1.0 / (double)cap) : (double)cap;
				if (target != s_last &&
					g_Memory.IsWritable(addr, sizeof(double)))
				{
					g_Memory.Write<double>(addr, target);
					s_last = target;
				}
			}

			else if (s_got_orig)
			{
				if (g_Memory.IsWritable(addr, sizeof(double)))
					g_Memory.Write<double>(addr, s_orig);
				s_got_orig = false;
				s_last = 0.0;
			}
		}
	}

	{ /* fov */
		static bool s_got_fov = false;
		static float s_fov_bak = 70.0f;

		if (m.fov && Cheat::Globals::Workspace)
		{
			auto cam = Cheat::Globals::Workspace->GetCurrentCamera();
			if (cam)
			{
				std::uint64_t addr = cam->address + ::Camera::FieldOfView;
				float cur = g_Memory.Read<float>(addr);

				// < 3.2 = radians
				bool rad = cur > 0.0f && cur < 3.2f;
				float target = rad ? m.fov_value * (3.14159265f / 180.0f) : m.fov_value;

				if (!s_got_fov)
				{
					s_fov_bak = rad ? cur * (180.0f / 3.14159265f) : cur;
					s_got_fov = true;
				}

				if (std::fabs(cur - target) > 0.0005f)
					g_Memory.Write<float>(addr, target);
			}
		}

		else if (s_got_fov && Cheat::Globals::Workspace)
		{
			if (auto cam = Cheat::Globals::Workspace->GetCurrentCamera())
			{
				std::uint64_t addr = cam->address + ::Camera::FieldOfView;
				float cur = g_Memory.Read<float>(addr);
				bool rad = cur > 0.0f && cur < 3.2f;
				float restore = rad ? s_fov_bak * (3.14159265f / 180.0f) : s_fov_bak;
				g_Memory.Write<float>(addr, restore);
			}
			s_got_fov = false;
		}
	}

	if (m.jump && g_Memory.IsValid(g_cache.humanoid))
	{
		g_Memory.Write<bool>(g_cache.humanoid + ::Humanoid::UseJumpPower, true);
		g_Memory.Write<float>(g_cache.humanoid + ::Humanoid::JumpPower, m.jump_power);
	}

	freecam_tick(dt);
	ThirdPerson::Tick();

	LocalMods::Noclip(g_cache.character, m.noclip);
	LocalMods::InfiniteJump(g_cache.humanoid, m.inf_jump);
	Btools::Tick();
	HitboxExpander::Tick();
}

namespace {
    std::thread g_misc_th;
    std::atomic<bool> g_misc_run{ false };

    void misc_loop()
    {
		LARGE_INTEGER freq{}, prev{};
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&prev);

		timeBeginPeriod(1);

		while (g_misc_run.load())
		{
			LARGE_INTEGER now{};
			QueryPerformanceCounter(&now);
			float dt = (float)(double(now.QuadPart - prev.QuadPart) / double(freq.QuadPart));
			prev = now;
			if (dt <= 0.0f || dt > 0.1f)
				dt = 0.002f;

			const auto& m = Cheat::g_Settings.misc;
			bool busy =
				m.jump || m.noclip || m.inf_jump || m.fps_unlock || m.fov ||
				m.freecam_key != 0 || m.bTools || Cheat::Features::ThirdPerson::NeedsTick() ||
				Cheat::g_Settings.hitbox.enabled;

			static bool s_was = false;
			if (busy || s_was)
				Cheat::Features::Misc::Tick(dt);
			s_was = busy;

			std::this_thread::sleep_for(std::chrono::milliseconds(busy ? 1 : 32));
		}

		timeEndPeriod(1);
    }
}

void Cheat::Features::Misc::Start()
{
	if (g_misc_run.load())
		return;

	g_misc_run.store(true);
	g_misc_th = std::thread(misc_loop);
	Speed::Start();
	Fly::Start();
	WorldEdit::Start();
}

void Cheat::Features::Misc::Stop()
{
	g_misc_run.store(false);
	WorldEdit::Stop();
	Speed::Stop();
	Fly::Stop();
	if (g_misc_th.joinable())
		g_misc_th.join();
	ThirdPerson::Restore();
	HitboxExpander::Shutdown();
}
