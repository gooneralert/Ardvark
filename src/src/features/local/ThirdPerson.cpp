#include "pch.h"
#include "ThirdPerson.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

#include <Windows.h>

#undef GetClassName

namespace {

Vector3 g_old_off{};
bool g_got_old = false;
bool g_on = false;
bool g_key_was = false;
bool g_key_tog = false;
std::uint64_t g_hum = 0;

std::uint64_t find_local_hum()
{
	if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
		return 0;

	auto lp = g_Memory.Read<std::uint64_t>(
		Cheat::Globals::Players->address + ::Player::LocalPlayer);
	if (!g_Memory.IsValid(lp))
		return 0;

	auto chara = g_Memory.Read<std::uint64_t>(lp + ::Player::ModelInstance);
	if (!g_Memory.IsValid(chara))
		return 0;

	Cheat::Instance ch(chara);
	for (const auto& c : ch.GetChildren())
	{
		if (c.GetClassName() == "Humanoid")
			return c.address;
	}
	return 0;
}

bool resolve_want()
{
	auto& m = Cheat::g_Settings.misc;
	int key = m.third_person_key;

	if (m.third_person_mode == 2)
	{
		g_key_was = false;
		m.third_person = true;
		return true;
	}

	if (key != 0)
	{
		bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
		bool want = false;

		if (m.third_person_mode == 1)
		{
			if (down && !g_key_was)
				g_key_tog = !g_key_tog;
			want = g_key_tog;
		}

		else
		{
			want = down;
		}

		g_key_was = down;
		m.third_person = want;
		return want;
	}

	g_key_was = false;
	g_key_tog = false;
	return m.third_person;
}

void poke_offset(std::uint64_t hum, const Vector3& off)
{
	if (!g_Memory.IsValid(hum))
		return;
	g_Memory.Write<Vector3>(hum + ::Humanoid::CameraOffset, off);
}

void bail_out()
{
	if (!g_on)
		return;

	auto hum = g_Memory.IsValid(g_hum) ? g_hum : find_local_hum();
	if (g_got_old)
		poke_offset(hum, g_old_off);

	g_on = false;
	g_got_old = false;
	g_hum = 0;
}

}

bool Cheat::Features::ThirdPerson::NeedsTick()
{
	return g_on || Cheat::g_Settings.misc.third_person ||
		Cheat::g_Settings.misc.third_person_key != 0;
}

void Cheat::Features::ThirdPerson::Restore()
{
	bail_out();
	g_key_was = false;
	g_key_tog = false;
}

void Cheat::Features::ThirdPerson::Tick()
{
	bool want = resolve_want();
	std::uint64_t hum = find_local_hum();

	if (!want || !g_Memory.IsValid(hum))
	{
		bail_out();
		return;
	}

	if (!g_on || g_hum != hum)
	{
		g_old_off = g_Memory.Read<Vector3>(hum + ::Humanoid::CameraOffset);
		g_got_old = true;
		g_hum = hum;
		g_on = true;
	}

	// CameraOffset: +Z назад, +Y вверх. один слайдер крутит оба
	float dist = Cheat::g_Settings.misc.third_person_distance;
	if (dist < 0.5f) dist = 0.5f;
	if (dist > 120.f) dist = 120.f;

	poke_offset(hum, Vector3(
		g_old_off.x,
		g_old_off.y + dist,
		g_old_off.z + dist));
}
