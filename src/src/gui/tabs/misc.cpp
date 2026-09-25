#include "pch.h"
#include "misc.h"
#include "helpers.h"

#include "imgui.h"
#include "widgets/widgets.h"
#include "app/Settings.h"
#include "features/visuals/KillEffects.h"
#include "features/misc/HitSounds.h"
#include "features/world/WorldSlots.h"

#include <vector>

namespace
{
	using ng_tabs::begin_column;
	using ng_tabs::begin_columns;
	using ng_tabs::begin_section;
	using ng_tabs::end_column;
	using ng_tabs::end_section;
	using ng_tabs::panel_gap;
	using ng_tabs::row_checkbox;
	using ng_tabs::row_checkbox_color;
	using ng_tabs::row_checkbox_more;
	using ng_tabs::row_color;
	using ng_tabs::row_combo;
	using ng_tabs::row_multicombo;
	using ng_tabs::row_slider_f;
	using ng_tabs::row_slider_i;
	using ng_tabs::section_header;

	std::vector<const char*> names(const char* const* a, int n)
	{
		return std::vector<const char*>(a, a + n);
	}

	static const std::vector<const char*> k_tm = { "default", "futuristic" };
}

// -----------------------------------------------------------------------------
// World page (reference): LIGHTING | WORLD + CAMERA + PERFORMANCE
// -----------------------------------------------------------------------------
void ng_tabs::draw_world_page()
{
	using namespace Cheat;

	auto& w = g_Settings.world;
	auto& m = g_Settings.misc;

	float left_w = 0.f, right_w = 0.f, h = 0.f;
	begin_columns(&left_w, &right_w, &h);

	begin_column("##wld_l", left_w, h);
	{
		section_header("LIGHTING");
		begin_section("##wld_light");
		{
			row_checkbox_color("ambient", &w.ambient, w.ambient_col);
			row_checkbox_color("outdoor", &w.outdoor, w.outdoor_col);

			row_checkbox_more("brightness", &w.brightness);
			if (w.brightness)
				row_slider_f("bri value", &w.brightness_val, 0.f, 20.f, "%.1f");

			row_checkbox_more("exposure", &w.exposure_on);
			if (w.exposure_on)
				row_slider_f("exposure val", &w.exposure, -5.f, 5.f, "%.2f");

			row_checkbox_color("light", &w.light, w.light_col);
			if (w.light)
			{
				row_slider_f("light dir x", &w.light_dir[0], -1.f, 1.f, "%.2f");
				row_slider_f("light dir y", &w.light_dir[1], -1.f, 1.f, "%.2f");
				row_slider_f("light dir z", &w.light_dir[2], -1.f, 1.f, "%.2f");
			}

			row_checkbox_color("fog", &w.fog, w.fog_color);
			if (w.fog)
			{
				row_slider_f("fog start", &w.fog_start, 0.f, 100.f, "%.1f");
				row_slider_f("fog end", &w.fog_end, 0.f, 2000.f, "%.0f");
			}

			row_checkbox_more("env scale", &w.env);
			if (w.env)
			{
				row_slider_f("env diffuse", &w.env_diffuse, 0.f, 2.f, "%.2f");
				row_slider_f("env specular", &w.env_specular, 0.f, 2.f, "%.2f");
			}

			row_checkbox_color("color shift", &w.color_shift, w.shift_top);
			if (w.color_shift)
			{
				row_color("shift top", w.shift_top);
				row_color("shift bottom", w.shift_bot);
			}

			row_checkbox_more("color corr", &w.color_corr);
			if (w.color_corr)
			{
				row_color("cc tint", w.cc_tint);
				row_slider_f("cc bri", &w.cc_bri, -1.f, 1.f, "%.2f");
				row_slider_f("cc contrast", &w.cc_con, -1.f, 1.f, "%.2f");
			}

			row_checkbox_more("color grade", &w.color_grade);
			if (w.color_grade)
				row_combo("tonemapper", &w.tonemapper, k_tm);

			row_checkbox_more("bloom", &w.bloom);
			if (w.bloom)
			{
				row_slider_f("bloom inten", &w.bloom_intensity, 0.f, 5.f, "%.2f");
				row_slider_f("bloom size", &w.bloom_size, 0.f, 56.f, "%.1f");
				row_slider_f("bloom thr", &w.bloom_threshold, 0.f, 3.f, "%.2f");
			}

			row_checkbox_more("dof", &w.dof);
			if (w.dof)
			{
				row_slider_f("dof far", &w.dof_far, 0.f, 1.f, "%.2f");
				row_slider_f("dof near", &w.dof_near, 0.f, 1.f, "%.2f");
				row_slider_f("dof focus", &w.dof_focus, 0.f, 200.f, "%.1f");
				row_slider_f("dof radius", &w.dof_radius, 0.f, 200.f, "%.1f");
			}

			row_checkbox_more("atmosphere", &w.atmosphere);
			if (w.atmosphere)
			{
				row_slider_f("density", &w.atmo_density, 0.f, 1.f, "%.2f");
				row_slider_f("haze", &w.atmo_haze, 0.f, 10.f, "%.2f");
				row_slider_f("glare", &w.atmo_glare, 0.f, 10.f, "%.2f");
				row_slider_f("atmo offset", &w.atmo_offset, 0.f, 1.f, "%.2f");
				row_color("atmo color", w.atmo_color);
				row_color("atmo decay", w.atmo_decay);
			}

			row_checkbox_more("sky", &w.sky);
			if (w.sky)
			{
				row_slider_f("sun size", &w.sun_angular, 0.f, 60.f, "%.1f");
				row_slider_f("moon size", &w.moon_angular, 0.f, 60.f, "%.1f");
				row_slider_f("orient x", &w.sky_orient_xyz[0], -180.f, 180.f, "%.1f");
				row_slider_f("orient y", &w.sky_orient_xyz[1], -180.f, 180.f, "%.1f");
				row_slider_f("orient z", &w.sky_orient_xyz[2], -180.f, 180.f, "%.1f");
			}

			row_checkbox_more("terrain", &w.terrain);
			if (w.terrain)
			{
				row_slider_f("grass len", &w.grass_len, 0.f, 1.f, "%.2f");
				row_color("grass color", w.grass_col);
				row_color("materials color", w.water_col);
				row_slider_f("water refl", &w.water_refl, 0.f, 1.f, "%.2f");
				row_slider_f("water trans", &w.water_trans, 0.f, 1.f, "%.2f");
			}
		}
		end_section();
	}
	end_column();

	ImGui::SameLine(0.f, panel_gap);

	begin_column("##wld_r", right_w, h);
	{
		section_header("WORLD");
		begin_section("##wld_world");
		{
			row_checkbox_more("time changer", &w.time_changer);
			if (w.time_changer)
				row_slider_f("clock time", &w.clock_time, 0.f, 24.f, "%.1f");

			w.skybox_mode = 1;
			row_checkbox_more("skybox changer", &w.skybox_changer);
			if (w.skybox_changer)
			{
				row_combo("preset", &w.skybox_preset,
					names(Features::WorldSlots::SkyboxPresetNames(), Features::WorldSlots::SkyboxPresetCount()));
			}

			row_checkbox("no shadow", &w.no_shadow);
			row_checkbox("teamcheck", &m.teamcheck);
			row_checkbox("arsenal flick fix", &m.arsenal_flick_fix);
		}
		end_section();

		section_header("CAMERA");
		begin_section("##wld_cam");
		{
			row_checkbox_more("fov changer", &m.fov);
			if (m.fov)
				row_slider_f("fov value", &m.fov_value, 10.f, 120.f, "%.0f");

			row_checkbox_more("crosshair", &g_Settings.crosshair.enabled);
			if (g_Settings.crosshair.enabled)
			{
				row_slider_f("cross length", &g_Settings.crosshair.length, 1.f, 40.f, "%.1f");
				row_slider_f("cross gap", &g_Settings.crosshair.gap, 0.f, 30.f, "%.1f");
				row_slider_f("cross thick", &g_Settings.crosshair.thickness, 1.f, 6.f, "%.1f");
				row_checkbox("cross spin", &g_Settings.crosshair.spin);
				if (g_Settings.crosshair.spin)
					row_slider_f("spin speed", &g_Settings.crosshair.spin_speed, -360.f, 360.f, "%.0f");
				row_checkbox("cross outline", &g_Settings.crosshair.outline);
				row_checkbox("cross dot", &g_Settings.crosshair.dot);
				if (g_Settings.crosshair.dot)
					row_slider_f("dot size", &g_Settings.crosshair.dot_size, 1.f, 8.f, "%.1f");
				row_color("cross color", g_Settings.crosshair.color);
				row_color("outline color", g_Settings.crosshair.outline_color);
			}
		}
		end_section();

		section_header("PERFORMANCE");
		begin_section("##wld_perf");
		{
			row_checkbox_more("fps unlocker", &m.fps_unlock);
			if (m.fps_unlock)
				row_slider_i("fps cap", &m.fps_cap, 10, 1000);
		}
		end_section();
	}
	end_column();
}

// -----------------------------------------------------------------------------
// hit feedback rows - drawn on the Extras page
// -----------------------------------------------------------------------------
void ng_tabs::draw_hitfx_rows()
{
	using namespace Cheat;

	auto& cfg = g_Settings.aim.active();
	static const std::vector<const char*> hb_parts = { "head", "hrp", "torso", "arms", "legs" };
	static const std::vector<const char*> hb_viz = { "2d", "3d", "3d filled" };

	row_checkbox_more("hitchance", &cfg.hitchance_enabled);
	if (cfg.hitchance_enabled)
		row_slider_f("hit chance", &cfg.hitchance, 1.f, 100.f, "%.0f");

	row_checkbox_more("hitbox expander", &g_Settings.hitbox.enabled);
	if (g_Settings.hitbox.enabled)
	{
		row_combo("hb part", &g_Settings.hitbox.part, hb_parts);
		row_slider_f("hb scale", &g_Settings.hitbox.scale, 1.f, 20.f, "%.1f");
		row_checkbox("hb visualize", &g_Settings.hitbox.visualize);
		if (g_Settings.hitbox.visualize)
		{
			row_combo("hb viz mode", &g_Settings.hitbox.viz_mode, hb_viz);
			row_color("hb color", g_Settings.hitbox.viz_color);
		}
	}

	row_checkbox_more("kill effects", &g_Settings.killfx.enabled);
	if (g_Settings.killfx.enabled)
	{
		if (g_Settings.killfx.effect < 0 ||
		    g_Settings.killfx.effect >= Visuals::KillEffects::EffectNameCount())
			g_Settings.killfx.effect = 0;

		row_combo("kill fx", &g_Settings.killfx.effect,
			names(Visuals::KillEffects::EffectNames(), Visuals::KillEffects::EffectNameCount()));
	}

	row_checkbox_more("hitmarkers", &g_Settings.hitmarker.enabled);
	if (g_Settings.hitmarker.enabled)
	{
		row_slider_f("marker size", &g_Settings.hitmarker.size, 0.5f, 2.5f, "%.2f");
		row_slider_f("marker time", &g_Settings.hitmarker.duration, 0.2f, 1.5f, "%.2f");
	}

	row_checkbox_more("hitsounds", &g_Settings.hitsound.enabled);
	if (g_Settings.hitsound.enabled)
	{
		if (g_Settings.hitsound.index < 0 ||
		    g_Settings.hitsound.index >= Features::HitSounds::Count())
			g_Settings.hitsound.index = 0;

		if (row_combo("hitsound", &g_Settings.hitsound.index,
			names(Features::HitSounds::Names(), Features::HitSounds::Count())))
		{
			Features::HitSounds::Play(g_Settings.hitsound.index);
		}
		row_slider_f("hitsound volume", &g_Settings.hitsound.volume, 0.f, 100.f, "%.0f");
	}

	row_checkbox_more("hit data", &g_Settings.hitdata.enabled);
	if (g_Settings.hitdata.enabled)
	{
		row_multicombo("##data_lines", "data lines", g_Settings.hitdata.modes,
			names(Visuals::KillEffects::HitDataModeNames(), Visuals::KillEffects::HitDataModeNameCount()));
		row_slider_f("data size", &g_Settings.hitdata.size, 10.f, 28.f, "%.0f");
		row_slider_f("data time", &g_Settings.hitdata.duration, 0.4f, 2.5f, "%.2f");
	}
}

