#include "pch.h"
#include "aim.h"
#include "helpers.h"
#include "imgui.h"
#include "app/Settings.h"
#include "features/games/PhantomForces.h"
#include "features/visuals/HavocWorldEsp.h"
#include <vector>

namespace
{
	static const std::vector<const char*> k_part_names = {
		"head", "upper torso", "lower torso", "hrp",
		"left hand", "right hand", "left foot", "right foot"
	};

	void draw_aim_cfg(Cheat::Settings::AimbotConfig& cfg, bool show_smooth, bool show_hitchance, const char* id_prefix)
	{
		using namespace ng_tabs;
		ImGui::PushID(id_prefix);

		static const std::vector<const char*> k_target = {
			"fov center", "distance", "lowest hp"
		};
		static const std::vector<const char*> k_part_sel = {
			"cycle", "random", "closest"
		};
		static const std::vector<const char*> k_fov_style = { "circle", "filled" };
		static const std::vector<const char*> k_pos = { "center", "mouse" };

		if (show_smooth)
		{
			row_slider_f("smoothness x", &cfg.smooth_x, 0.1f, 5.0f, "%.2f");
			row_slider_f("smoothness y", &cfg.smooth_y, 0.1f, 5.0f, "%.2f");
		}

		row_combo("target select", &cfg.target_select, k_target);

		if (row_multicombo("##parts", "parts", cfg.parts, k_part_names))
			cfg.SyncTiersFromParts();

		row_combo("part select", &cfg.part_select, k_part_sel);

		row_checkbox_color("fov check", &cfg.fov_enabled, cfg.fov_color);
		if (cfg.fov_enabled)
		{
			row_combo("fov style", &cfg.fov_style, k_fov_style);
			row_checkbox_color("fov outline", &cfg.fov_outline, cfg.fov_outline_color);
			row_slider_f("fov size", &cfg.fov_size, 10.0f, 600.0f, "%.0f");
		}

		row_combo("aim pos", &cfg.fov_position, k_pos);
		row_checkbox_color("aim tracer", &cfg.tracer, cfg.tracer_color);

		row_checkbox("distance check", &cfg.distance_check);
		if (Cheat::Visuals::HavocWorldEsp::IsActivePlace())
		{
			float hi = Cheat::Visuals::HavocWorldEsp::MaxRangeStuds();
			float d = cfg.max_distance;
			if (d < 50.f) d = 50.f;
			if (d > hi) d = hi;
			cfg.max_distance = d;
			row_slider_f("max distance (400m cap)", &cfg.max_distance, 50.0f, hi, "%.0f");
		}
		else
		{
			row_slider_f("max distance", &cfg.max_distance, 50.0f, 5000.0f, "%.0f");
		}

		row_checkbox("visible only", &cfg.visible_only);
		row_checkbox("dead check", &cfg.dead_check);

		if (show_hitchance)
		{
			row_checkbox("hitchance", &cfg.hitchance_enabled);
			if (cfg.hitchance_enabled)
				row_slider_f("hit chance", &cfg.hitchance, 1.0f, 100.0f, "%.0f");
		}

		row_checkbox("humanize", &cfg.humanize);
		if (cfg.humanize)
			row_slider_f("reaction", &cfg.reaction_ms, 0.0f, 400.0f, "%.0f");

		row_checkbox("sticky target", &cfg.sticky);
		if (cfg.sticky)
			row_slider_f("sticky fov", &cfg.sticky_fov_scale, 1.0f, 4.0f, "%.2f");

		row_checkbox("prediction", &cfg.prediction);
		if (cfg.prediction)
			row_slider_f("bullet speed", &cfg.bullet_speed, 100.0f, 5000.0f, "%.0f");

		ImGui::PopID();
	}
}

void ng_tabs::draw_aim_tab()
{
	using namespace Cheat;

	Settings::AimbotConfig& cfg =
		(g_Settings.aim.type == 1) ? g_Settings.aim.camera : g_Settings.aim.mouse;
	Settings::AimbotConfig& scfg = g_Settings.aim.silent;

	float left_w = 0.f, right_w = 0.f, h = 0.f;
	begin_columns(&left_w, &right_w, &h);

	begin_panel("##aim_child1", left_w, h);
	{
		row_keybind("##aim_kb", "aim key", &g_Settings.aim.bind, &g_Settings.aim.bind_mode);

		static const std::vector<const char*> k_types = { "mouse", "camera", "off" };
		row_combo("type", &g_Settings.aim.type, k_types);

		draw_aim_cfg(cfg, g_Settings.aim.type == 0 || g_Settings.aim.type == 1, false, "aim");

		if (Visuals::HavocWorldEsp::IsActivePlace())
			row_checkbox("target bots", &g_Settings.aim.target_bots);
	}
	end_panel();

	ImGui::SameLine(0.f, panel_gap);

	begin_panel("##aim_child2", right_w, h);
	{
		row_keybind("##silent_kb", "silent key",
		            &g_Settings.aim.silent_bind, &g_Settings.aim.silent_bind_mode);

		const bool pf_aim = Games::PhantomForces::IsActivePlace();

		if (pf_aim)
		{
			g_Settings.aim.silent_method = Settings::SILENT_PHANTOM;
			static const std::vector<const char*> k_silent_pf = { "phantom forces" };
			int pf_idx = 0;
			row_combo("silent method", &pf_idx, k_silent_pf);
		}
		else
		{
			if (g_Settings.aim.silent_method == Settings::SILENT_PHANTOM)
				g_Settings.aim.silent_method = Settings::SILENT_RAYCAST;

			static const std::vector<const char*> k_silent = {
				"viewport", "mouse", "raycast", "magic bullet"
			};
			int method = g_Settings.aim.silent_method;
			if (method < 0 || method >= Settings::SILENT_PHANTOM)
				method = Settings::SILENT_RAYCAST;
			row_combo("silent method", &method, k_silent);
			g_Settings.aim.silent_method = method;
		}

		if (!pf_aim && g_Settings.aim.silent_method == Settings::SILENT_RAYCAST)
		{
			row_checkbox_keybind("force magic bullet", &g_Settings.aim.force_magic_bullet,
			                    &g_Settings.aim.force_magic_key);
			if (g_Settings.aim.force_magic_bullet)
			{
				static const std::vector<const char*> k_modes = { "hold", "toggle", "always" };
				if (g_Settings.aim.force_magic_mode < 0) g_Settings.aim.force_magic_mode = 0;
				if (g_Settings.aim.force_magic_mode > 2) g_Settings.aim.force_magic_mode = 2;
				row_combo("force magic mode", &g_Settings.aim.force_magic_mode, k_modes);
			}
		}

		draw_aim_cfg(scfg, false, true, "silent");
	}
	end_panel();
}
