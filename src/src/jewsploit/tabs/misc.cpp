#include "misc.h"
#include "helpers.h"
#include "../widgets/child.h"
#include "../widgets/checkbox.h"
#include "../widgets/keybind.h"
#include "../widgets/label_color.h"
#include "app/Settings.h"
#include "features/visuals/KillEffects.h"
#include "features/misc/HitSounds.h"

#include <imgui.h>

void ng_tabs::draw_misc_tab()
{
	using namespace Cheat;

	float side_gap = 10.f;
	float avail_w = ImGui::GetContentRegionAvail().x;
	float avail_h = ImGui::GetContentRegionAvail().y;
	float cw = (avail_w - side_gap) * 0.5f;

	Settings::AimbotConfig& cfg = g_Settings.aim.active();

	ng::child_begin("##misc_child1", "world", cw, avail_h, 10.f, true);

	auto& w = Cheat::g_Settings.world;

	static const char* k_tm[] = { "default", "futuristic" };

	pad();
	ng::checkbox("teamcheck", &Cheat::g_Settings.misc.teamcheck);
	gap();
	pad();
	ng::checkbox("no shadow", &w.no_shadow);
	gap();
	pad();
	ng::checkbox("time changer", &w.time_changer);
	row_slider("clock time", "clock time", &w.clock_time, 0.f, 24.f, w.time_changer);
	gap();

	pad();
	ng::checkbox("ambient", &w.ambient);
	pad();
	ng::label_color("world_ambient", "ambient col", w.ambient_col, w.ambient);
	gap();
	pad();
	ng::checkbox("outdoor", &w.outdoor);
	pad();
	ng::label_color("world_outdoor", "outdoor col", w.outdoor_col, w.outdoor);
	gap();
	pad();
	ng::checkbox("brightness", &w.brightness);
	row_slider("bri value", "bri value", &w.brightness_val, 0.f, 20.f, w.brightness);
	gap();
	pad();
	ng::checkbox("exposure", &w.exposure_on);
	row_slider("exposure val", "exposure val", &w.exposure, -5.f, 5.f, w.exposure_on);
	gap();
	pad();
	ng::checkbox("light", &w.light);
	pad();
	ng::label_color("world_light", "light color", w.light_col, w.light);
	row_slider("light dir x", "light dir x", &w.light_dir[0], -1.f, 1.f, w.light);
	row_slider("light dir y", "light dir y", &w.light_dir[1], -1.f, 1.f, w.light);
	row_slider("light dir z", "light dir z", &w.light_dir[2], -1.f, 1.f, w.light);
	gap();

	row_cb_color("fog", &w.fog, w.fog_color, "world_fog_color");
	row_slider("fog start", "fog start", &w.fog_start, 0.f, 100.f, w.fog);
	row_slider("fog end", "fog end", &w.fog_end, 0.f, 2000.f, w.fog);
	gap();

	pad();
	ng::checkbox("env scale", &w.env);
	row_slider("env diffuse", "env diffuse", &w.env_diffuse, 0.f, 2.f, w.env);
	row_slider("env specular", "env specular", &w.env_specular, 0.f, 2.f, w.env);
	gap();

	row_cb_color("color shift", &w.color_shift, w.shift_top, "world_shift_top");
	pad();
	ng::label_color("world_shift_bot", "shift bot", w.shift_bot, w.color_shift);
	gap();

	pad();
	ng::checkbox("atmosphere", &w.atmosphere);
	row_slider("density", "density", &w.atmo_density, 0.f, 1.f, w.atmosphere);
	row_slider("haze", "haze", &w.atmo_haze, 0.f, 10.f, w.atmosphere);
	row_slider("glare", "glare", &w.atmo_glare, 0.f, 10.f, w.atmosphere);
	row_slider("atmo offset", "atmo offset", &w.atmo_offset, 0.f, 1.f, w.atmosphere);
	pad();
	ng::label_color("world_atmo_col", "atmo color", w.atmo_color, w.atmosphere);
	pad();
	ng::label_color("world_atmo_dec", "atmo decay", w.atmo_decay, w.atmosphere);
	gap();

	pad();
	ng::checkbox("sky", &w.sky);
	row_slider("sun size", "sun size", &w.sun_angular, 0.f, 60.f, w.sky);
	row_slider("moon size", "moon size", &w.moon_angular, 0.f, 60.f, w.sky);
	row_slider("orient x", "orient x", &w.sky_orient_xyz[0], -180.f, 180.f, w.sky);
	row_slider("orient y", "orient y", &w.sky_orient_xyz[1], -180.f, 180.f, w.sky);
	row_slider("orient z", "orient z", &w.sky_orient_xyz[2], -180.f, 180.f, w.sky);
	gap();

	pad();
	ng::checkbox("bloom", &w.bloom);
	row_slider("bloom inten", "bloom inten", &w.bloom_intensity, 0.f, 5.f, w.bloom);
	row_slider("bloom size", "bloom size", &w.bloom_size, 0.f, 56.f, w.bloom);
	row_slider("bloom thr", "bloom thr", &w.bloom_threshold, 0.f, 3.f, w.bloom);
	gap();

	row_cb_color("color corr", &w.color_corr, w.cc_tint, "world_cc_tint");
	row_slider("cc bri", "cc bri", &w.cc_bri, -1.f, 1.f, w.color_corr);
	row_slider("cc contrast", "cc contrast", &w.cc_con, -1.f, 1.f, w.color_corr);
	gap();

	pad();
	ng::checkbox("color grade", &w.color_grade);
	row_select("##tonemap", "tonemapper", &w.tonemapper, k_tm, 2, w.color_grade);
	gap();

	pad();
	ng::checkbox("dof", &w.dof);
	row_slider("dof far", "dof far", &w.dof_far, 0.f, 1.f, w.dof);
	row_slider("dof near", "dof near", &w.dof_near, 0.f, 1.f, w.dof);
	row_slider("dof focus", "dof focus", &w.dof_focus, 0.f, 200.f, w.dof);
	row_slider("dof radius", "dof radius", &w.dof_radius, 0.f, 200.f, w.dof);
	gap();

	pad();
	ng::checkbox("terrain", &w.terrain);
	row_slider("grass len", "grass len", &w.grass_len, 0.f, 1.f, w.terrain);
	pad();
	ng::label_color("world_grass_col", "grass color", w.grass_col, w.terrain);
	pad();
	ng::label_color("world_water", "materials color", w.water_col, w.terrain);
	row_slider("water refl", "water refl", &w.water_refl, 0.f, 1.f, w.terrain);
	row_slider("water trans", "water trans", &w.water_trans, 0.f, 1.f, w.terrain);
	gap();

	pad();
	ng::checkbox("fps unlocker", &Cheat::g_Settings.misc.fps_unlock);
	row_slider_i("fps cap", "fps cap", &Cheat::g_Settings.misc.fps_cap, 60, 1000, Cheat::g_Settings.misc.fps_unlock);
	gap();
	pad();
	ng::checkbox("fov changer", &Cheat::g_Settings.misc.fov);
	row_slider("fov value", "fov value", &Cheat::g_Settings.misc.fov_value, 10.f, 120.f, Cheat::g_Settings.misc.fov);
	gap();
	pad();
	ng::checkbox("crosshair", &Cheat::g_Settings.crosshair.enabled);

	if (Cheat::g_Settings.crosshair.enabled)
	{
		gap();
		row_slider("cross length", "cross length", &Cheat::g_Settings.crosshair.length, 1.f, 40.f, true);
		gap();
		row_slider("cross gap", "cross gap", &Cheat::g_Settings.crosshair.gap, 0.f, 30.f, true);
		gap();
		row_slider("cross thick", "cross thick", &Cheat::g_Settings.crosshair.thickness, 1.f, 6.f, true);
		gap();
		pad();
		ng::checkbox("cross spin", &Cheat::g_Settings.crosshair.spin);
		row_slider("spin speed", "spin speed", &Cheat::g_Settings.crosshair.spin_speed, -360.f, 360.f, Cheat::g_Settings.crosshair.spin);
		gap();
		pad();
		ng::checkbox("cross outline", &Cheat::g_Settings.crosshair.outline);
		gap();
		pad();
		ng::checkbox("cross dot", &Cheat::g_Settings.crosshair.dot);
		row_slider("dot size", "dot size", &Cheat::g_Settings.crosshair.dot_size, 1.f, 8.f, Cheat::g_Settings.crosshair.dot);
		gap();
		pad();
		ng::label_color("crosshair_color", "cross color", Cheat::g_Settings.crosshair.color);
		gap();
		pad();
		ng::label_color("crosshair_outline_color", "outline color", Cheat::g_Settings.crosshair.outline_color);
	}

	ng::child_end();

	ImGui::SameLine(0.f, side_gap);

	ng::child_begin("##misc_child2", "local", cw, avail_h, 10.f, true);

	static const char* k_ws_modes[] = { "position", "humanoid" };

	pad();
	ng::checkbox("walkspeed", &Cheat::g_Settings.misc.walkspeed);
	gap();
	row_keybind("ws key", "ws key", &Cheat::g_Settings.misc.walkspeed_key, &Cheat::g_Settings.misc.walkspeed_key_mode);
	gap();
	row_select("ws mode", "ws mode", &Cheat::g_Settings.misc.walkspeed_mode, k_ws_modes, 2);
	gap();
	row_slider("ws value", "ws value", &Cheat::g_Settings.misc.walkspeed_value, 1.f, 500.f, true);
	gap();
	pad();
	ng::checkbox("jump power", &Cheat::g_Settings.misc.jump);
	gap();
	row_slider("jump value", "jump value", &Cheat::g_Settings.misc.jump_power, 50.f, 500.f, true);
	gap();
	pad();
	ng::checkbox("fly", &Cheat::g_Settings.misc.fly);
	gap();
	row_keybind("fly key", "fly key", &Cheat::g_Settings.misc.fly_key, &Cheat::g_Settings.misc.fly_key_mode);
	gap();
	row_slider("fly speed", "fly speed", &Cheat::g_Settings.misc.fly_speed, 5.f, 1000.f, true);
	gap();
	row_keybind("freecam", "freecam", &Cheat::g_Settings.misc.freecam_key, &Cheat::g_Settings.misc.freecam_mode);
	gap();
	row_slider("freecam speed", "freecam speed", &Cheat::g_Settings.misc.freecam_speed, 10.f, 300.f, true);
	gap();
	row_slider("freecam sens", "freecam sens", &Cheat::g_Settings.misc.freecam_sens, 0.05f, 1.f, true);
	gap();
	pad();
	ng::checkbox("third person", &Cheat::g_Settings.misc.third_person);
	ng::keybind("##third_person_kb", &Cheat::g_Settings.misc.third_person_key, &Cheat::g_Settings.misc.third_person_mode, Cheat::g_Settings.misc.third_person);
	gap();
	row_slider("camera back/up", "camera back/up", &Cheat::g_Settings.misc.third_person_distance, 1.f, 120.f, true);
	gap();
	pad();
	ng::checkbox("noclip", &Cheat::g_Settings.misc.noclip);
	gap();
	pad();
	ng::checkbox("infinite jump", &Cheat::g_Settings.misc.inf_jump);
	gap();

	pad();
	ng::checkbox("hitchance", &cfg.hitchance_enabled);
	gap();

	if (cfg.hitchance_enabled)
	{
		row_slider("##hitchance", "hit chance", &cfg.hitchance, 1.0f, 100.0f);
		gap();
	}

	pad();
	ng::checkbox("hitbox expander", &g_Settings.hitbox.enabled);
	gap();

	if (g_Settings.hitbox.enabled)
	{
		static const char* hb_parts[] = {
			"head", "hrp", "torso", "arms", "legs"
		};
		row_select("##hb_part", "hb part", &g_Settings.hitbox.part, hb_parts,
		           Settings::HB_PART_COUNT);
		gap();

		row_slider("##hb_scale", "hb scale", &g_Settings.hitbox.scale, 1.0f, 20.0f);
		gap();

		pad();
		ng::checkbox("hb visualize", &g_Settings.hitbox.visualize);
		gap();

		if (g_Settings.hitbox.visualize)
		{
			static const char* hb_viz[] = { "2d", "3d", "3d filled" };
			row_select("##hb_viz", "hb viz mode", &g_Settings.hitbox.viz_mode, hb_viz, 3);
			gap();

			ImGui::SetCursorPosX(12.f);
			ng::label_color("hitbox_viz_color", "hb color", g_Settings.hitbox.viz_color);
			gap();
		}
	}

	pad();
	ng::checkbox("kill effects", &g_Settings.killfx.enabled);
	gap();

	if (g_Settings.killfx.enabled)
	{
		if (g_Settings.killfx.effect < 0 ||
		    g_Settings.killfx.effect >= Visuals::KillEffects::EffectNameCount())
			g_Settings.killfx.effect = 0;

		row_select("##kill_fx", "kill fx", &g_Settings.killfx.effect,
		           Visuals::KillEffects::EffectNames(),
		           Visuals::KillEffects::EffectNameCount());
		gap();
	}

	pad();
	ng::checkbox("hitmarkers", &g_Settings.hitmarker.enabled);
	gap();

	if (g_Settings.hitmarker.enabled)
	{
		row_slider("##marker_size", "marker size", &g_Settings.hitmarker.size, 0.5f, 2.5f);
		gap();
		row_slider("##marker_time", "marker time", &g_Settings.hitmarker.duration, 0.2f, 1.5f);
		gap();
	}

	pad();
	ng::checkbox("hitsounds", &g_Settings.hitsound.enabled);
	gap();

	if (g_Settings.hitsound.enabled)
	{
		if (g_Settings.hitsound.index < 0 ||
		    g_Settings.hitsound.index >= Features::HitSounds::Count())
			g_Settings.hitsound.index = 0;

		if (row_select("##hitsound", "hitsound", &g_Settings.hitsound.index,
		               Features::HitSounds::Names(),
		               Features::HitSounds::Count()))
		{
			Features::HitSounds::Play(g_Settings.hitsound.index);
		}
		gap();

		row_slider("##hitsound_vol", "hitsound volume", &g_Settings.hitsound.volume,
		           0.0f, 100.0f);
		gap();
	}

	pad();
	ng::checkbox("hit data", &g_Settings.hitdata.enabled);
	gap();

	if (g_Settings.hitdata.enabled)
	{
		row_dropdown("##data_lines", "data lines", g_Settings.hitdata.modes,
		             Visuals::KillEffects::HitDataModeNames(),
		             Visuals::KillEffects::HitDataModeNameCount());
		gap();

		row_slider("##data_size", "data size", &g_Settings.hitdata.size, 10.0f, 28.0f);
		gap();
		row_slider("##data_time", "data time", &g_Settings.hitdata.duration, 0.4f, 2.5f);
		gap();
	}

	ng::child_end();
}
