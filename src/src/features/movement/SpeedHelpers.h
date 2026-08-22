#pragma once

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "renderer/Renderer.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>

#undef GetClassName

namespace Cheat {
namespace Features {
namespace Speed {
namespace helpers {

enum class ws_mode
{
	position = 0,
	humanoid
};

inline bool chat_focused()
{
	if (!Cheat::Globals::InstanceDataModel.address)
		return false;

	static std::uint64_t s_bar = 0;
	static std::uint64_t s_dm = 0;

	if (s_dm != Cheat::Globals::InstanceDataModel.address || !g_Memory.IsValid(s_bar))
	{
		s_dm = Cheat::Globals::InstanceDataModel.address;
		s_bar = 0;
		for (const auto& c : Cheat::Globals::InstanceDataModel.GetChildren())
		{
			if (c.GetName() != "TextChatService" && c.GetClassName() != "TextChatService")
				continue;

			for (const auto& cfg : c.GetChildren())
			{
				if (cfg.GetName() == "ChatInputBarConfiguration")
				{
					s_bar = cfg.address;
					break;
				}
			}
			break;
		}
	}

	if (!g_Memory.IsValid(s_bar))
		return false;

	// Offsets::Chat::IsFocused is not in geeg — disabled; treat chat as not focused.
	return false;
}

inline bool roblox_focused()
{
	const HWND wnd = Cheat::Renderer::GetGameHwnd();
	return wnd && IsWindow(wnd) && GetForegroundWindow() == wnd;
}

inline bool key_gate(int key, int mode, bool& tog, bool& was)
{
	// always / нет бинда — всегда on
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

inline std::uint64_t local_chara()
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

inline bool resolve_local(std::uint64_t& out_hrp, std::uint64_t& out_prim, std::uint64_t& out_hum)
{
	out_hrp = 0;
	out_prim = 0;
	out_hum = 0;

	std::uint64_t chara = local_chara();
	if (!chara)
		return false;

	for (const auto& c : Cheat::Instance(chara).GetChildren())
	{
		if (!out_hum && c.GetClassName() == "Humanoid")
			out_hum = c.address;

		if (!out_hrp && c.GetName() == "HumanoidRootPart")
		{
			out_hrp = c.address;
			out_prim = g_Memory.Read<std::uint64_t>(out_hrp + ::BasePart::Primitive);
		}
	}

	return g_Memory.IsValid(out_hrp) && g_Memory.IsValid(out_prim) && g_Memory.IsValid(out_hum);
}

inline float read_ws(std::uint64_t hum)
{
	if (!g_Memory.IsValid(hum))
		return 16.f;

	float v = g_Memory.Read<float>(hum + ::Humanoid::Walkspeed);
	if (!std::isfinite(v))
		return 16.f;

	return v;
}

inline bool write_ws(std::uint64_t hum, float spd)
{
	if (!g_Memory.IsValid(hum) || !std::isfinite(spd))
		return false;

	g_Memory.Write<float>(hum + ::Humanoid::Walkspeed, spd);
	g_Memory.Write<float>(hum + ::Humanoid::WalkspeedCheck, spd);
	return true;
}

inline bool move_vel(std::uint64_t hum, float spd, Vector3& out)
{
	out = {};
	if (!g_Memory.IsValid(hum) || spd <= 0.0f)
		return false;

	Vector3 md = Cheat::Humanoid(hum).GetMoveDirection();
	Vector3 dir(md.x, 0.0f, md.z);
	float ls = dir.LengthSquared();
	if (!std::isfinite(ls) || ls < 1e-4f)
		return false;

	float len = std::sqrt(ls);
	if (!std::isfinite(len) || len < 1e-4f)
		return false;

	if (len > 1.0f)
		dir /= len;

	out = dir * spd;
	return true;
}

inline bool write_xz(std::uint64_t addr, float x, float z)
{
	if (!addr)
		return false;
	g_Memory.Write<float>(addr, x);
	g_Memory.Write<float>(addr + sizeof(float) * 2, z);
	return true;
}

inline bool set_xz_pos(std::uint64_t prim, float x, float z)
{
	if (!prim)
		return false;

	bool ok = false;
	ok |= write_xz(prim + ::Primitive::Position, x, z);

	// Offsets::Primitive::Properties / PropertyPosition are not in geeg — that
	// extra XZ write to the internal properties blob is disabled. The Position
	// write above stands.
	return ok;
}

inline bool step_xz(std::uint64_t prim, const Vector3& hvel, float dt, int reps)
{
	if (dt <= 0.0f || !g_Memory.IsValid(prim))
		return false;

	if (reps < 1) reps = 1;
	if (reps > 100) reps = 100;

	float step = dt / (float)reps;
	bool ok = false;
	for (int i = 0; i < reps; ++i)
	{
		Vector3 pos = g_Memory.Read<Vector3>(prim + ::Primitive::Position);
		float nx = pos.x + hvel.x * step;
		float nz = pos.z + hvel.z * step;
		ok |= set_xz_pos(prim, nx, nz);
	}
	return ok;
}

} // namespace helpers
} // namespace Speed
} // namespace Features
} // namespace Cheat
