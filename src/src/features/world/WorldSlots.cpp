#include "pch.h"
#include "WorldSlots.h"
#include "WorldOwn.h"
#include "LightingInvalidate.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "core/globals/Globals.h"
#include "app/Settings.h"
#include "features/lua/vm/InstanceCreate.h"

#include <string>

#undef GetClassName

namespace Cheat {
namespace Features {
namespace WorldSlots {
namespace {

Color3 col3(const float c[4])
{
	return Color3(c[0], c[1], c[2]);
}

float clampf(float v, float lo, float hi)
{
	if (v < lo) v = lo;
	if (v > hi) v = hi;
	return v;
}

std::uint64_t child_class(std::uint64_t parent, const char* cls)
{
	if (!g_Memory.IsValid(parent) || !cls)
		return 0;

	Instance p(parent);
	for (const auto& c : p.GetChildren())
	{
		if (c.GetClassName() == cls)
			return c.address;
	}

	return 0;
}

void invalidate_sky()
{
	std::uint64_t rv = LightingInvalidate::render_view();
	if (!rv)
		return;
	g_Memory.Write<std::uint8_t>(rv + ::RenderView::SkyValid, 0);
}

// айди подтверждённо рабочие — те же, что в scripts/custom_skybox.lua
struct SkyboxPreset
{
	const char* name;
	std::uint64_t bk, dn, ft, lf, rt, up;
};

constexpr SkyboxPreset k_skybox_presets[] = {
	{ "Night",        15536110634ull, 15536112543ull, 15536116141ull, 15536114370ull, 15536118762ull, 15536117282ull },
	{ "Evening",      16136021536ull, 16136025360ull, 16136021536ull, 16136021536ull, 16136021536ull, 16136023362ull },
	{ "Morning",      6444884337ull,  6444884785ull,  6444884337ull,  6444884337ull,  6444884337ull,  6412503613ull },
	{ "Galaxy",       159454299ull,   159454296ull,   159454293ull,   159454286ull,   159454300ull,   159454288ull },
	{ "Pink Sky",     12635309703ull, 12635311686ull, 12635312870ull, 12635313718ull, 12635315817ull, 12635316856ull },
	{ "Vaporwave",    8631780182ull,  8631784904ull,  8631769834ull,  8631777199ull,  8631735555ull,  8631782345ull },
	{ "Red Night",    401664839ull,   401664862ull,   401664960ull,   401664881ull,   401664901ull,   401664936ull },
	{ "Sunset",       323494035ull,   323494368ull,   323494130ull,   323494252ull,   323494067ull,   323493360ull },
	{ "Blue",         135483466ull,   135483484ull,   135483461ull,   135483495ull,   135483499ull,   135483475ull },
	{ "Purple Haze",  8107841671ull,  6444884785ull,  8107841671ull,  8107841671ull,  8107841671ull,  8107849791ull },
	{ "Pink Fade",    11427769401ull, 11427770685ull, 11427769401ull, 11427769401ull, 11427769401ull, 11427771954ull },
};
constexpr int k_skybox_preset_count = (int)(sizeof(k_skybox_presets) / sizeof(k_skybox_presets[0]));

std::string skybox_url(std::uint64_t id)
{
	return "rbxassetid://" + std::to_string(id);
}

// charm's raw std::string write: buffer @+0, length @+0x10, capacity @+0x18.
// Also stamps the Content wrapper (kind=1, scheme=0) right before the string
// so the engine re-parses the new asset url instead of using cached scheme.
void write_skybox_str(std::uint64_t addr, const std::string& val)
{
	if (!addr || !g_Memory.IsValid(addr) || val.empty())
		return;

	const auto current_len = g_Memory.Read<std::int64_t>(addr + 0x18);
	std::uint64_t buf = current_len >= 16
		? g_Memory.Read<std::uint64_t>(addr)
		: addr;
	if (!buf || !g_Memory.IsValid(buf))
		return;

	for (size_t i = 0; i < val.size() && i < 200; i++)
		g_Memory.Write<char>(buf + i, val[i]);
	g_Memory.Write<char>(buf + val.size(), '\0');
	g_Memory.Write<std::int64_t>(addr + 0x10, static_cast<std::int64_t>(val.size()));

	// Content = { int32 kind; int16 scheme; std::string uri } -> kind=1,
	// scheme=0 so the engine re-parses the new rbxassetid:// url.
	const std::uint64_t content = addr - 0x10;
	g_Memory.Write<std::int32_t>(content, 1);
	g_Memory.Write<std::int16_t>(content + 0x08, 0);
}


void write_mat_rgb(std::uint64_t base, std::uintptr_t off, const float c[4])
{
	if (!g_Memory.IsValid(base))
		return;

	float r = c[0], g = c[1], b = c[2];
	if (r < 0.f) r = 0.f;
	if (r > 1.f) r = 1.f;
	if (g < 0.f) g = 0.f;
	if (g > 1.f) g = 1.f;
	if (b < 0.f) b = 0.f;
	if (b > 1.f) b = 1.f;

	std::uint8_t rgb[3];
	rgb[0] = (std::uint8_t)(r * 255.f);
	rgb[1] = (std::uint8_t)(g * 255.f);
	rgb[2] = (std::uint8_t)(b * 255.f);
	g_Memory.WriteRaw((uintptr_t)(base + off), rgb, 3);
}

bool clock_is_double(std::uint64_t lighting)
{
	const std::uint64_t addr = lighting + ::Lighting::ClockTime;
	const float cur_f = g_Memory.Read<float>(addr);
	const double cur_d = g_Memory.Read<double>(addr);
	const bool f_ok = cur_f >= 0.f && cur_f <= 24.f;
	const bool d_ok = cur_d >= 0.0 && cur_d <= 24.0;
	return d_ok && !f_ok;
}

void write_clock_time(std::uint64_t lighting, float t)
{
	const std::uint64_t addr = lighting + ::Lighting::ClockTime;
	if (clock_is_double(lighting))
		LightingInvalidate::force_write<double>(addr, (double)t);

	else
		LightingInvalidate::force_write<float>(addr, t);
}

}

std::uint64_t FindLighting()
{
	if (!g_Memory.IsValid(Cheat::Globals::InstanceDataModel.address))
		return 0;

	for (const auto& c : Cheat::Globals::InstanceDataModel.GetChildren())
	{
		if (c.GetClassName() == "Lighting")
			return c.address;
	}

	return 0;
}

void TickNoShadow(std::uint64_t lighting, bool force_off)
{
	// ::Lighting::GlobalShadows was removed — no-shadow disabled.
	(void)lighting;
	(void)force_off;
}

void TickTime(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static bool as_double = false;
	static float bak_clock = 12.f;
	static Vector3 bak_sun{};
	static Vector3 bak_moon{};
	static Vector3 bak_gt{};
	static Vector3 bak_gb{};
	static const WorldOwn::Field fields[] = {
		WorldOwn::Field::Clock,
		WorldOwn::Field::Sun,
		WorldOwn::Field::Moon,
		WorldOwn::Field::GradTop,
		WorldOwn::Field::GradBot,
	};

	const bool on = Cheat::g_Settings.world.time_changer && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Time, fields, 5);
			as_double = clock_is_double(lighting);
			const std::uint64_t addr = lighting + ::Lighting::ClockTime;
			if (as_double)
				bak_clock = (float)g_Memory.Read<double>(addr);

			else
				bak_clock = g_Memory.Read<float>(addr);

			bak_sun = g_Memory.Read<Vector3>(lighting + ::Lighting::SunPosition);
			bak_moon = g_Memory.Read<Vector3>(lighting + ::Lighting::MoonPosition);
			bak_gt = g_Memory.Read<Vector3>(lighting + ::Lighting::GradientTop);
			bak_gb = g_Memory.Read<Vector3>(lighting + ::Lighting::GradientBottom);
			was = true;
		}

		float t = Cheat::g_Settings.world.clock_time;
		if (t < 0.f) t = 0.f;
		if (t > 24.f) t = 24.f;

		constexpr float PI = 3.14159265f;
		const float sun_angle = (t / 24.f - 0.25f) * 2.f * PI;
		const float sun_y = std::sin(sun_angle);
		const float sun_x = 0.f;
		const float sun_z = -std::cos(sun_angle);
		const Vector3 sun_pos(sun_x, sun_y, sun_z);
		const Vector3 moon_pos(-sun_x, -sun_y, -sun_z);

		const float h = sun_y;
		Vector3 grad_top, grad_bot;

		if (h > 0.15f)
		{
			grad_top = Vector3(0.35f, 0.65f, 0.95f);
			grad_bot = Vector3(0.75f, 0.85f, 0.95f);
		}

		else if (h > 0.0f)
		{
			const float st = h / 0.15f;
			grad_top = Vector3(0.15f + st * 0.2f, 0.15f + st * 0.5f, 0.25f + st * 0.7f);
			grad_bot = Vector3(0.95f, 0.45f + st * 0.4f, 0.25f + st * 0.7f);
		}

		else
		{
			float nt = (-h) / 0.3f;
			if (nt > 1.0f) nt = 1.0f;
			const float inv = 1.0f - nt;
			grad_top = Vector3(0.05f + inv * 0.1f, 0.05f + inv * 0.1f, 0.12f + inv * 0.13f);
			grad_bot = Vector3(0.08f + inv * 0.07f, 0.08f + inv * 0.07f, 0.15f + inv * 0.1f);
		}

		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::Clock))
		{
			write_clock_time(lighting, t);
			dirty = true;
		}

		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::Sun))
			dirty |= LightingInvalidate::write_if_changed<Vector3>(
				lighting + ::Lighting::SunPosition, sun_pos);

		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::Moon))
			dirty |= LightingInvalidate::write_if_changed<Vector3>(
				lighting + ::Lighting::MoonPosition, moon_pos);

		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::GradTop))
			dirty |= LightingInvalidate::write_if_changed<Vector3>(
				lighting + ::Lighting::GradientTop, grad_top);

		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::GradBot))
			dirty |= LightingInvalidate::write_if_changed<Vector3>(
				lighting + ::Lighting::GradientBottom, grad_bot);
	}

	else if (was)
	{
		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::Clock))
		{
			const std::uint64_t addr = lighting + ::Lighting::ClockTime;
			if (as_double)
				LightingInvalidate::force_write<double>(addr, (double)bak_clock);

			else
				LightingInvalidate::force_write<float>(addr, bak_clock);
			dirty = true;
		}

		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::Sun))
			LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::SunPosition, bak_sun);

		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::Moon))
			LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::MoonPosition, bak_moon);

		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::GradTop))
			LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::GradientTop, bak_gt);

		if (WorldOwn::Owns(WorldOwn::Feat::Time, WorldOwn::Field::GradBot))
			LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::GradientBottom, bak_gb);

		WorldOwn::Release(WorldOwn::Feat::Time, fields, 5);
		was = false;
		dirty = true;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickAmbient(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static Color3 bak{};
	static const WorldOwn::Field fields[] = { WorldOwn::Field::Amb };
	const bool on = Cheat::g_Settings.world.ambient && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Ambient, fields, 1);
			bak = g_Memory.Read<Color3>(lighting + ::Lighting::Ambient);
			was = true;
		}

		if (WorldOwn::Owns(WorldOwn::Feat::Ambient, WorldOwn::Field::Amb))
		{
			dirty |= LightingInvalidate::write_if_changed<Color3>(
				lighting + ::Lighting::Ambient, col3(Cheat::g_Settings.world.ambient_col));
		}
	}

	else if (was)
	{
		if (WorldOwn::Owns(WorldOwn::Feat::Ambient, WorldOwn::Field::Amb))
		{
			LightingInvalidate::force_write<Color3>(lighting + ::Lighting::Ambient, bak);
			dirty = true;
		}

		WorldOwn::Release(WorldOwn::Feat::Ambient, fields, 1);
		was = false;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickOutdoor(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static Color3 bak{};
	static const WorldOwn::Field fields[] = { WorldOwn::Field::Outdoor };
	const bool on = Cheat::g_Settings.world.outdoor && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Outdoor, fields, 1);
			bak = g_Memory.Read<Color3>(lighting + ::Lighting::OutdoorAmbient);
			was = true;
		}

		if (WorldOwn::Owns(WorldOwn::Feat::Outdoor, WorldOwn::Field::Outdoor))
		{
			dirty |= LightingInvalidate::write_if_changed<Color3>(
				lighting + ::Lighting::OutdoorAmbient, col3(Cheat::g_Settings.world.outdoor_col));
		}
	}

	else if (was)
	{
		if (WorldOwn::Owns(WorldOwn::Feat::Outdoor, WorldOwn::Field::Outdoor))
		{
			LightingInvalidate::force_write<Color3>(lighting + ::Lighting::OutdoorAmbient, bak);
			dirty = true;
		}

		WorldOwn::Release(WorldOwn::Feat::Outdoor, fields, 1);
		was = false;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickBrightness(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static float bak = 1.f;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::Bri };
	const bool on = Cheat::g_Settings.world.brightness && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Brightness, fields, 1);
			bak = g_Memory.Read<float>(lighting + ::Lighting::Brightness);
			was = true;
		}

		if (WorldOwn::Owns(WorldOwn::Feat::Brightness, WorldOwn::Field::Bri))
		{
			float v = Cheat::g_Settings.world.brightness_val;
			if (v < 0.f) v = 0.f;
			if (v > 20.f) v = 20.f;
			dirty |= LightingInvalidate::write_if_changed<float>(
				lighting + ::Lighting::Brightness, v);
		}
	}

	else if (was)
	{
		if (WorldOwn::Owns(WorldOwn::Feat::Brightness, WorldOwn::Field::Bri))
		{
			LightingInvalidate::force_write<float>(lighting + ::Lighting::Brightness, bak);
			dirty = true;
		}

		WorldOwn::Release(WorldOwn::Feat::Brightness, fields, 1);
		was = false;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickExposure(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static float bak = 0.f;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::Expo };
	const bool on = Cheat::g_Settings.world.exposure_on && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Exposure, fields, 1);
			bak = g_Memory.Read<float>(lighting + ::Lighting::ExposureCompensation);
			was = true;
		}

		if (WorldOwn::Owns(WorldOwn::Feat::Exposure, WorldOwn::Field::Expo))
		{
			float v = Cheat::g_Settings.world.exposure;
			if (v < -5.f) v = -5.f;
			if (v > 5.f) v = 5.f;
			dirty |= LightingInvalidate::write_if_changed<float>(
				lighting + ::Lighting::ExposureCompensation, v);
		}
	}

	else if (was)
	{
		if (WorldOwn::Owns(WorldOwn::Feat::Exposure, WorldOwn::Field::Expo))
		{
			LightingInvalidate::force_write<float>(
				lighting + ::Lighting::ExposureCompensation, bak);
			dirty = true;
		}

		WorldOwn::Release(WorldOwn::Feat::Exposure, fields, 1);
		was = false;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickLight(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static Color3 bak_col{};
	static Vector3 bak_dir{};
	static const WorldOwn::Field fields[] = {
		WorldOwn::Field::LightCol,
		WorldOwn::Field::LightDir,
	};
	const bool on = Cheat::g_Settings.world.light && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Light, fields, 2);
			bak_col = g_Memory.Read<Color3>(lighting + ::Lighting::LightColor);
			bak_dir = g_Memory.Read<Vector3>(lighting + ::Lighting::LightDirection);
			was = true;
		}

		const auto& w = Cheat::g_Settings.world;
		if (WorldOwn::Owns(WorldOwn::Feat::Light, WorldOwn::Field::LightCol))
		{
			dirty |= LightingInvalidate::write_if_changed<Color3>(
				lighting + ::Lighting::LightColor, col3(w.light_col));
		}

		if (WorldOwn::Owns(WorldOwn::Feat::Light, WorldOwn::Field::LightDir))
		{
			Vector3 d(w.light_dir[0], w.light_dir[1], w.light_dir[2]);
			dirty |= LightingInvalidate::write_if_changed<Vector3>(
				lighting + ::Lighting::LightDirection, d);
		}
	}

	else if (was)
	{
		if (WorldOwn::Owns(WorldOwn::Feat::Light, WorldOwn::Field::LightCol))
			LightingInvalidate::force_write<Color3>(lighting + ::Lighting::LightColor, bak_col);

		if (WorldOwn::Owns(WorldOwn::Feat::Light, WorldOwn::Field::LightDir))
			LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::LightDirection, bak_dir);

		WorldOwn::Release(WorldOwn::Feat::Light, fields, 2);
		was = false;
		dirty = true;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickFog(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static float bak_s = 0.f;
	static float bak_e = 100000.f;
	static Color3 bak_c{};
	static const WorldOwn::Field fields[] = {
		WorldOwn::Field::FogStart,
		WorldOwn::Field::FogEnd,
		WorldOwn::Field::FogCol,
	};
	const bool on = Cheat::g_Settings.world.fog && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Fog, fields, 3);
			bak_s = g_Memory.Read<float>(lighting + ::Lighting::FogStart);
			bak_e = g_Memory.Read<float>(lighting + ::Lighting::FogEnd);
			bak_c = g_Memory.Read<Color3>(lighting + ::Lighting::FogColor);
			was = true;
		}

		const auto& w = Cheat::g_Settings.world;
		float start = w.fog_start;
		if (start < 0.f) start = 0.f;
		if (start > 100.f) start = 100.f;
		float end = w.fog_end;
		if (end > 2000.f) end = 2000.f;
		if (end < start + 1.f) end = start + 1.f;

		if (WorldOwn::Owns(WorldOwn::Feat::Fog, WorldOwn::Field::FogStart))
			dirty |= LightingInvalidate::write_if_changed<float>(
				lighting + ::Lighting::FogStart, start);

		if (WorldOwn::Owns(WorldOwn::Feat::Fog, WorldOwn::Field::FogEnd))
			dirty |= LightingInvalidate::write_if_changed<float>(
				lighting + ::Lighting::FogEnd, end);

		if (WorldOwn::Owns(WorldOwn::Feat::Fog, WorldOwn::Field::FogCol))
		{
			dirty |= LightingInvalidate::write_if_changed<Color3>(
				lighting + ::Lighting::FogColor, col3(w.fog_color));
		}
	}

	else if (was)
	{
		if (WorldOwn::Owns(WorldOwn::Feat::Fog, WorldOwn::Field::FogStart))
			LightingInvalidate::force_write<float>(lighting + ::Lighting::FogStart, bak_s);

		if (WorldOwn::Owns(WorldOwn::Feat::Fog, WorldOwn::Field::FogEnd))
			LightingInvalidate::force_write<float>(lighting + ::Lighting::FogEnd, bak_e);

		if (WorldOwn::Owns(WorldOwn::Feat::Fog, WorldOwn::Field::FogCol))
			LightingInvalidate::force_write<Color3>(lighting + ::Lighting::FogColor, bak_c);

		WorldOwn::Release(WorldOwn::Feat::Fog, fields, 3);
		was = false;
		dirty = true;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickEnv(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static float bak_d = 1.f;
	static float bak_s = 1.f;
	static const WorldOwn::Field fields[] = {
		WorldOwn::Field::EnvDiff,
		WorldOwn::Field::EnvSpec,
	};
	const bool on = Cheat::g_Settings.world.env && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Env, fields, 2);
			bak_d = g_Memory.Read<float>(lighting + ::Lighting::EnvironmentDiffuseScale);
			bak_s = g_Memory.Read<float>(lighting + ::Lighting::EnvironmentSpecularScale);
			was = true;
		}

		const auto& w = Cheat::g_Settings.world;
		if (WorldOwn::Owns(WorldOwn::Feat::Env, WorldOwn::Field::EnvDiff))
		{
			dirty |= LightingInvalidate::write_if_changed<float>(
				lighting + ::Lighting::EnvironmentDiffuseScale, clampf(w.env_diffuse, 0.f, 2.f));
		}

		if (WorldOwn::Owns(WorldOwn::Feat::Env, WorldOwn::Field::EnvSpec))
		{
			dirty |= LightingInvalidate::write_if_changed<float>(
				lighting + ::Lighting::EnvironmentSpecularScale, clampf(w.env_specular, 0.f, 2.f));
		}
	}

	else if (was)
	{
		if (WorldOwn::Owns(WorldOwn::Feat::Env, WorldOwn::Field::EnvDiff))
			LightingInvalidate::force_write<float>(
				lighting + ::Lighting::EnvironmentDiffuseScale, bak_d);

		if (WorldOwn::Owns(WorldOwn::Feat::Env, WorldOwn::Field::EnvSpec))
			LightingInvalidate::force_write<float>(
				lighting + ::Lighting::EnvironmentSpecularScale, bak_s);

		WorldOwn::Release(WorldOwn::Feat::Env, fields, 2);
		was = false;
		dirty = true;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickColorShift(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static Color3 bak_t{};
	static Color3 bak_b{};
	static const WorldOwn::Field fields[] = {
		WorldOwn::Field::ShiftTop,
		WorldOwn::Field::ShiftBot,
	};
	const bool on = Cheat::g_Settings.world.color_shift && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::ColorShift, fields, 2);
			bak_t = g_Memory.Read<Color3>(lighting + ::Lighting::ColorShift_Top);
			bak_b = g_Memory.Read<Color3>(lighting + ::Lighting::ColorShift_Bottom);
			was = true;
		}

		const auto& w = Cheat::g_Settings.world;
		if (WorldOwn::Owns(WorldOwn::Feat::ColorShift, WorldOwn::Field::ShiftTop))
		{
			dirty |= LightingInvalidate::write_if_changed<Color3>(
				lighting + ::Lighting::ColorShift_Top, col3(w.shift_top));
		}

		if (WorldOwn::Owns(WorldOwn::Feat::ColorShift, WorldOwn::Field::ShiftBot))
		{
			dirty |= LightingInvalidate::write_if_changed<Color3>(
				lighting + ::Lighting::ColorShift_Bottom, col3(w.shift_bot));
		}
	}

	else if (was)
	{
		if (WorldOwn::Owns(WorldOwn::Feat::ColorShift, WorldOwn::Field::ShiftTop))
			LightingInvalidate::force_write<Color3>(lighting + ::Lighting::ColorShift_Top, bak_t);

		if (WorldOwn::Owns(WorldOwn::Feat::ColorShift, WorldOwn::Field::ShiftBot))
			LightingInvalidate::force_write<Color3>(lighting + ::Lighting::ColorShift_Bottom, bak_b);

		WorldOwn::Release(WorldOwn::Feat::ColorShift, fields, 2);
		was = false;
		dirty = true;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickAtmosphere(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::Atmo };
	const bool on = Cheat::g_Settings.world.atmosphere && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Atmosphere, fields, 1);
			was = true;
		}

		if (!WorldOwn::Owns(WorldOwn::Feat::Atmosphere, WorldOwn::Field::Atmo))
			return;

		std::uint64_t atmo = child_class(lighting, "Atmosphere");
		if (!g_Memory.IsValid(atmo))
			return;

		const auto& w = Cheat::g_Settings.world;
		dirty |= LightingInvalidate::write_if_changed<float>(
			atmo + ::Atmosphere::Density, clampf(w.atmo_density, 0.f, 1.f));
		dirty |= LightingInvalidate::write_if_changed<float>(
			atmo + ::Atmosphere::Haze, clampf(w.atmo_haze, 0.f, 10.f));
		dirty |= LightingInvalidate::write_if_changed<float>(
			atmo + ::Atmosphere::Glare, clampf(w.atmo_glare, 0.f, 10.f));
		dirty |= LightingInvalidate::write_if_changed<float>(
			atmo + ::Atmosphere::Offset, clampf(w.atmo_offset, 0.f, 1.f));
		dirty |= LightingInvalidate::write_if_changed<Color3>(
			atmo + ::Atmosphere::Color, col3(w.atmo_color));
		dirty |= LightingInvalidate::write_if_changed<Color3>(
			atmo + ::Atmosphere::Decay, col3(w.atmo_decay));
	}

	else if (was)
	{
		WorldOwn::Release(WorldOwn::Feat::Atmosphere, fields, 1);
		was = false;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickSky(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::Sky };
	const bool on = Cheat::g_Settings.world.sky && !force_off;
	bool sky_dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Sky, fields, 1);
			was = true;
		}

		if (!WorldOwn::Owns(WorldOwn::Feat::Sky, WorldOwn::Field::Sky))
			return;

		std::uint64_t sky = g_Memory.Read<std::uint64_t>(lighting + ::Lighting::Sky);
		if (!g_Memory.IsValid(sky))
			sky = child_class(lighting, "Sky");

		if (!g_Memory.IsValid(sky))
			return;

		const auto& w = Cheat::g_Settings.world;
		sky_dirty |= LightingInvalidate::write_if_changed<float>(
			sky + ::Sky::SunAngularSize, clampf(w.sun_angular, 0.f, 60.f));
		sky_dirty |= LightingInvalidate::write_if_changed<float>(
			sky + ::Sky::MoonAngularSize, clampf(w.moon_angular, 0.f, 60.f));
		Vector3 o(w.sky_orient_xyz[0], w.sky_orient_xyz[1], w.sky_orient_xyz[2]);
		sky_dirty |= LightingInvalidate::write_if_changed<Vector3>(
			sky + ::Sky::SkyboxOrientation, o);
	}

	else if (was)
	{
		WorldOwn::Release(WorldOwn::Feat::Sky, fields, 1);
		was = false;
	}

	if (sky_dirty)
		invalidate_sky();
}

void TickBloom(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::Bloom };
	const bool on = Cheat::g_Settings.world.bloom && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Bloom, fields, 1);
			was = true;
		}

		if (!WorldOwn::Owns(WorldOwn::Feat::Bloom, WorldOwn::Field::Bloom))
			return;

		std::uint64_t bloom = child_class(lighting, "BloomEffect");
		if (!g_Memory.IsValid(bloom))
			return;

		const auto& w = Cheat::g_Settings.world;
		dirty |= LightingInvalidate::write_if_changed<std::uint8_t>(
			bloom + ::BloomEffect::Enabled, 1);
		dirty |= LightingInvalidate::write_if_changed<float>(
			bloom + ::BloomEffect::Intensity, clampf(w.bloom_intensity, 0.f, 5.f));
		dirty |= LightingInvalidate::write_if_changed<float>(
			bloom + ::BloomEffect::Size, clampf(w.bloom_size, 0.f, 56.f));
		dirty |= LightingInvalidate::write_if_changed<float>(
			bloom + ::BloomEffect::Threshold, clampf(w.bloom_threshold, 0.f, 3.f));
	}

	else if (was)
	{
		WorldOwn::Release(WorldOwn::Feat::Bloom, fields, 1);
		was = false;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickColorCorr(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::ColorCorr };
	const bool on = Cheat::g_Settings.world.color_corr && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::ColorCorr, fields, 1);
			was = true;
		}

		if (!WorldOwn::Owns(WorldOwn::Feat::ColorCorr, WorldOwn::Field::ColorCorr))
			return;

		std::uint64_t cc = child_class(lighting, "ColorCorrectionEffect");
		if (!g_Memory.IsValid(cc))
			return;

		const auto& w = Cheat::g_Settings.world;
		dirty |= LightingInvalidate::write_if_changed<std::uint8_t>(
			cc + ::ColorCorrectionEffect::Enabled, 1);
		dirty |= LightingInvalidate::write_if_changed<float>(
			cc + ::ColorCorrectionEffect::Brightness, w.cc_bri);
		dirty |= LightingInvalidate::write_if_changed<float>(
			cc + ::ColorCorrectionEffect::Contrast, w.cc_con);
		dirty |= LightingInvalidate::write_if_changed<Color3>(
			cc + ::ColorCorrectionEffect::TintColor, col3(w.cc_tint));
	}

	else if (was)
	{
		WorldOwn::Release(WorldOwn::Feat::ColorCorr, fields, 1);
		was = false;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickColorGrade(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::ColorGrade };
	const bool on = Cheat::g_Settings.world.color_grade && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::ColorGrade, fields, 1);
			was = true;
		}

		if (!WorldOwn::Owns(WorldOwn::Feat::ColorGrade, WorldOwn::Field::ColorGrade))
			return;

		std::uint64_t cg = child_class(lighting, "ColorGradingEffect");
		if (!g_Memory.IsValid(cg))
			return;

		int tm = Cheat::g_Settings.world.tonemapper;
		if (tm < 0) tm = 0;
		if (tm > 1) tm = 1;
		dirty |= LightingInvalidate::write_if_changed<std::uint8_t>(
			cg + ::ColorGradingEffect::Enabled, 1);
		dirty |= LightingInvalidate::write_if_changed<std::int32_t>(
			cg + ::ColorGradingEffect::TonemapperPreset, tm);
	}

	else if (was)
	{
		WorldOwn::Release(WorldOwn::Feat::ColorGrade, fields, 1);
		was = false;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickDof(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::Dof };
	const bool on = Cheat::g_Settings.world.dof && !force_off;
	bool dirty = false;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Dof, fields, 1);
			was = true;
		}

		if (!WorldOwn::Owns(WorldOwn::Feat::Dof, WorldOwn::Field::Dof))
			return;

		std::uint64_t dof = child_class(lighting, "DepthOfFieldEffect");
		if (!g_Memory.IsValid(dof))
			return;

		const auto& w = Cheat::g_Settings.world;
		dirty |= LightingInvalidate::write_if_changed<std::uint8_t>(
			dof + ::DepthOfFieldEffect::Enabled, 1);
		dirty |= LightingInvalidate::write_if_changed<float>(
			dof + ::DepthOfFieldEffect::FarIntensity, w.dof_far);
		dirty |= LightingInvalidate::write_if_changed<float>(
			dof + ::DepthOfFieldEffect::NearIntensity, w.dof_near);
		dirty |= LightingInvalidate::write_if_changed<float>(
			dof + ::DepthOfFieldEffect::FocusDistance, w.dof_focus);
		dirty |= LightingInvalidate::write_if_changed<float>(
			dof + ::DepthOfFieldEffect::InFocusRadius, w.dof_radius);
	}

	else if (was)
	{
		WorldOwn::Release(WorldOwn::Feat::Dof, fields, 1);
		was = false;
	}

	if (dirty)
		LightingInvalidate::invalidate();
}

void TickTerrain(std::uint64_t lighting, bool force_off)
{
	(void)lighting;
	static bool was = false;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::Terrain };
	const bool on = Cheat::g_Settings.world.terrain && !force_off;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::Terrain, fields, 1);
			was = true;
		}

		if (!WorldOwn::Owns(WorldOwn::Feat::Terrain, WorldOwn::Field::Terrain))
			return;

		std::uint64_t ws = 0;
		if (g_Memory.IsValid(Cheat::Globals::InstanceDataModel.address))
		{
			for (const auto& c : Cheat::Globals::InstanceDataModel.GetChildren())
			{
				if (c.GetClassName() == "Workspace")
				{
					ws = c.address;
					break;
				}
			}
		}

		std::uint64_t terr = 0;
		if (g_Memory.IsValid(ws))
			terr = child_class(ws, "Terrain");

		if (!g_Memory.IsValid(terr))
			return;

		const auto& w = Cheat::g_Settings.world;
		LightingInvalidate::write_if_changed<float>(
			terr + ::Terrain::GrassLength, clampf(w.grass_len, 0.f, 1.f));

		std::uint64_t mc = g_Memory.Read<std::uint64_t>(terr + ::Terrain::MaterialColors);
		if (g_Memory.IsValid(mc) && mc > 0x10000)
			write_mat_rgb(mc, ::MaterialColors::Grass, w.grass_col);

		LightingInvalidate::write_if_changed<Color3>(
			terr + ::Terrain::WaterColor, col3(w.water_col));
		LightingInvalidate::write_if_changed<float>(
			terr + ::Terrain::WaterReflectance, clampf(w.water_refl, 0.f, 1.f));
		LightingInvalidate::write_if_changed<float>(
			terr + ::Terrain::WaterTransparency, clampf(w.water_trans, 0.f, 1.f));
	}

	else if (was)
	{
		WorldOwn::Release(WorldOwn::Feat::Terrain, fields, 1);
		was = false;
	}
}

// пишет 6 граней Sky выбранным пресетом; тяжёлая операция (аллокация строки
// в процессе игры на грань) — делаем только при реальном изменении настроек,
// не каждый тик
void TickSkyboxChanger(std::uint64_t lighting, bool force_off)
{
	static bool was = false;
	static int applied_preset = -1;
	static int applied_mode = -1;
	static const WorldOwn::Field fields[] = { WorldOwn::Field::SkyboxChanger };
	const auto& w = Cheat::g_Settings.world;
	const bool on = w.skybox_changer && !force_off;

	if (!g_Memory.IsValid(lighting))
		return;

	if (on)
	{
		if (!was)
		{
			WorldOwn::Claim(WorldOwn::Feat::SkyboxChanger, fields, 1);
			was = true;
			applied_preset = -1;
			applied_mode = -1;
		}

		if (!WorldOwn::Owns(WorldOwn::Feat::SkyboxChanger, WorldOwn::Field::SkyboxChanger))
			return;

		// шейдерный скайбокс — заглушка, реализуется отдельно
		if (w.skybox_mode != 1)
			return;

		int preset = w.skybox_preset;
		if (preset < 0 || preset >= k_skybox_preset_count)
			preset = 0;

		if (preset == applied_preset && w.skybox_mode == applied_mode)
			return;

		std::uint64_t sky = g_Memory.Read<std::uint64_t>(lighting + ::Lighting::Sky);
		if (!g_Memory.IsValid(sky))
			sky = child_class(lighting, "Sky");

		if (!g_Memory.IsValid(sky))
		{
			std::uint64_t created = 0;
			if (!Cheat::Features::InstanceCreate::New("Sky", 0, &created) || !g_Memory.IsValid(created))
				return;
			sky = created;
		}

		// движок забирает текстуры в момент, когда Sky попадает в Lighting,
		// поэтому пишем грани, пока он снаружи, и парентим обратно в конце
		const SkyboxPreset& p = k_skybox_presets[preset];

		// The engine grabs the skybox textures when Sky is added to Lighting,
		// so we unparent first, write the 6 face strings in place (charm's raw
		// string write), then reparent so the new content is re-read.
		Cheat::Features::InstanceCreate::SetParent(sky, 0);

		write_skybox_str(sky + ::Sky::SkyboxBk, skybox_url(p.bk));
		write_skybox_str(sky + ::Sky::SkyboxDn, skybox_url(p.dn));
		write_skybox_str(sky + ::Sky::SkyboxFt, skybox_url(p.ft));
		write_skybox_str(sky + ::Sky::SkyboxLf, skybox_url(p.lf));
		write_skybox_str(sky + ::Sky::SkyboxRt, skybox_url(p.rt));
		write_skybox_str(sky + ::Sky::SkyboxUp, skybox_url(p.up));

		// make sure the Sky is parented back to Lighting
		Cheat::Features::InstanceCreate::SetParent(sky, lighting);

		// force the renderer to reload the skybox textures
		if (std::uint64_t rv = LightingInvalidate::render_view())
		{
			g_Memory.Write<std::uint8_t>(rv + ::RenderView::LightingValid, 0);
			g_Memory.Write<std::uint8_t>(rv + ::RenderView::SkyboxValid, 0);
		}

		applied_preset = preset;
		applied_mode = w.skybox_mode;
	}

	else if (was)
	{
		WorldOwn::Release(WorldOwn::Feat::SkyboxChanger, fields, 1);
		was = false;
		applied_preset = -1;
		applied_mode = -1;
	}
}

int SkyboxPresetCount()
{
	return k_skybox_preset_count;
}

const char* const* SkyboxPresetNames()
{
	static const char* names[k_skybox_preset_count] = {};
	static bool built = false;
	if (!built)
	{
		for (int i = 0; i < k_skybox_preset_count; ++i)
			names[i] = k_skybox_presets[i].name;
		built = true;
	}
	return names;
}

} // namespace WorldSlots
} // namespace Features
} // namespace Cheat
