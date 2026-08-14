#include "aim.h"
#include "helpers.h"
#include "../widgets/child.h"
#include "../widgets/checkbox.h"
#include "../widgets/keybind.h"
#include "../widgets/label_color.h"
#include "app/Settings.h"
#include "features/games/PhantomForces.h"
#include "features/visuals/havoc/HavocWorldEsp.h"
#include <imgui.h>

void ng_tabs::draw_aim_tab()
{
	using namespace Cheat;

	// левый child всегда mouse/camera, не silent
	Settings::AimbotConfig& cfg =
		(g_Settings.aim.type == 1) ? g_Settings.aim.camera : g_Settings.aim.mouse;
	Settings::AimbotConfig& scfg = g_Settings.aim.silent;

	float side_gap = 10.f;
	float avail_w = ImGui::GetContentRegionAvail().x;
	float avail_h = ImGui::GetContentRegionAvail().y;
	float cw = (avail_w - side_gap) * 0.5f;

	if (ng::child_begin("##aim_child1", "aim", cw, avail_h, 10.f, true))
	{
		row_keybind("##aim_kb", "aim key", &g_Settings.aim.bind, &g_Settings.aim.bind_mode);
		gap();

		static const char* k_types[] = { "mouse", "camera", "off" };
		row_select("##aim_type", "type", &g_Settings.aim.type, k_types, 3);
		gap();

		if (g_Settings.aim.type == 0 || g_Settings.aim.type == 1)
		{
			row_slider("##smooth_x", "smoothness x", &cfg.smooth_x, 0.1f, 5.0f);
			gap();
			row_slider("##smooth_y", "smoothness y", &cfg.smooth_y, 0.1f, 5.0f);
			gap();
		}

		static const char* k_target[] = {
			"fov center", "distance", "lowest hp"
		};
		row_select("##target_select", "target select", &cfg.target_select, k_target, 3);
		gap();

		row_cb_color("fov check", &cfg.fov_enabled, cfg.fov_color, "aim_fov_color");
		gap();

		if (cfg.fov_enabled)
		{
			static const char* k_fov_style[] = { "circle", "filled" };
			row_select("##fov_style", "fov style", &cfg.fov_style, k_fov_style, 2);
			gap();

			row_cb_color("fov outline", &cfg.fov_outline,
			             cfg.fov_outline_color, "aim_fov_outline_color");
			gap();

			row_slider("##fov_size", "fov size", &cfg.fov_size, 10.0f, 600.0f);
			gap();
		}

		static const char* k_pos[] = { "center", "mouse" };
		row_select("##aim_pos", "aim pos", &cfg.fov_position, k_pos, 2);
		gap();

		row_cb_color("aim tracer", &cfg.tracer, cfg.tracer_color, "aim_tracer_color");
		gap();

		pad();
		ng::checkbox("distance check", &cfg.distance_check);
		gap();

		if (Visuals::HavocWorldEsp::IsActivePlace())
		{
			float hi = Visuals::HavocWorldEsp::MaxRangeStuds();
			float d = cfg.max_distance;
			if (d < 50.f) d = 50.f;
			if (d > hi) d = hi;
			cfg.max_distance = d;
			row_slider("##max_dist_havoc", "max distance (400m cap)", &cfg.max_distance,
			           50.0f, hi);
		}

		else
		{
			row_slider("##max_dist", "max distance", &cfg.max_distance, 50.0f, 5000.0f);
		}
		gap();

		pad();
		ng::checkbox("visible only", &cfg.visible_only);
		gap();

		pad();
		ng::checkbox("dead check", &cfg.dead_check);
		gap();

		if (Visuals::HavocWorldEsp::IsActivePlace())
		{
			pad();
			ng::checkbox("target bots", &g_Settings.aim.target_bots);
			gap();
		}

		pad();
		ng::checkbox("humanize", &cfg.humanize);
		gap();

		if (cfg.humanize)
		{
			row_slider("##reaction", "reaction", &cfg.reaction_ms, 0.0f, 400.0f);
			gap();
		}

		pad();
		ng::checkbox("sticky target", &cfg.sticky);
		gap();

		if (cfg.sticky)
		{
			row_slider("##sticky_fov", "sticky fov", &cfg.sticky_fov_scale, 1.0f, 4.0f);
			gap();
		}

		pad();
		ng::checkbox("prediction", &cfg.prediction);
		gap();

		if (cfg.prediction)
		{
			row_slider("##bullet_speed", "bullet speed", &cfg.bullet_speed,
			           100.0f, 5000.0f);
			gap();
		}
	}
	ng::child_end();

	ImGui::SameLine(0.f, side_gap);

	if (ng::child_begin("##aim_child2", "silent aim", cw, avail_h, 10.f, true))
	{
		row_keybind("##silent_kb", "silent key",
		            &g_Settings.aim.silent_bind, &g_Settings.aim.silent_bind_mode);
		gap();

		const bool pf_aim = Games::PhantomForces::IsActivePlace();

		if (pf_aim)
		{
			// обычные silent'ы в PF не работают — слот под кастом позже
			g_Settings.aim.silent_method = Settings::SILENT_PHANTOM;
			static const char* k_silent_pf[] = { "phantom forces" };
			int pf_idx = 0;
			row_select("##silent_pf", "silent method", &pf_idx, k_silent_pf, 1);
		}

		else
		{
			if (g_Settings.aim.silent_method == Settings::SILENT_PHANTOM)
				g_Settings.aim.silent_method = Settings::SILENT_RAYCAST;

			static const char* k_silent[] = {
				"viewport", "mouse", "raycast", "magic bullet"
			};
			int method = g_Settings.aim.silent_method;
			if (method < 0 || method >= Settings::SILENT_PHANTOM)
				method = Settings::SILENT_RAYCAST;
			row_select("##silent_method", "silent method", &method, k_silent,
			           Settings::SILENT_PHANTOM);
			g_Settings.aim.silent_method = method;
		}
		gap();

		if (!pf_aim &&
		    g_Settings.aim.silent_method == Settings::SILENT_RAYCAST)
		{
			pad();
			ng::checkbox("force magic bullet", &g_Settings.aim.force_magic_bullet);
			ng::keybind("##force_magic_kb", &g_Settings.aim.force_magic_key,
			            &g_Settings.aim.force_magic_mode,
			            g_Settings.aim.force_magic_bullet);
			gap();
		}

		static const char* k_starget[] = {
			"fov center", "distance", "lowest hp"
		};
		row_select("##silent_target", "target select", &scfg.target_select, k_starget, 3);
		gap();

		row_cb_color("fov check", &scfg.fov_enabled, scfg.fov_color, "silent_fov_color");
		gap();

		if (scfg.fov_enabled)
		{
			static const char* k_sfov_style[] = { "circle", "filled" };
			row_select("##silent_fov_style", "fov style", &scfg.fov_style, k_sfov_style, 2);
			gap();

			row_cb_color("fov outline", &scfg.fov_outline,
			             scfg.fov_outline_color, "silent_fov_outline_color");
			gap();

			row_slider("##silent_fov_size", "fov size", &scfg.fov_size, 10.0f, 600.0f);
			gap();
		}

		static const char* k_spos[] = { "center", "mouse" };
		row_select("##silent_pos", "aim pos", &scfg.fov_position, k_spos, 2);
		gap();

		row_cb_color("aim tracer", &scfg.tracer, scfg.tracer_color, "silent_tracer_color");
		gap();

		pad();
		ng::checkbox("distance check", &scfg.distance_check);
		gap();

		if (Visuals::HavocWorldEsp::IsActivePlace())
		{
			float hi = Visuals::HavocWorldEsp::MaxRangeStuds();
			float d = scfg.max_distance;
			if (d < 50.f) d = 50.f;
			if (d > hi) d = hi;
			scfg.max_distance = d;
			row_slider("##silent_max_havoc", "max distance (400m cap)",
			           &scfg.max_distance, 50.0f, hi);
		}

		else
		{
			row_slider("##silent_max", "max distance", &scfg.max_distance, 50.0f, 5000.0f);
		}
		gap();

		pad();
		ng::checkbox("visible only", &scfg.visible_only);
		gap();

		pad();
		ng::checkbox("dead check", &scfg.dead_check);
		gap();

		pad();
		ng::checkbox("hitchance", &scfg.hitchance_enabled);
		gap();

		if (scfg.hitchance_enabled)
		{
			row_slider("##silent_hc", "hit chance", &scfg.hitchance, 1.0f, 100.0f);
			gap();
		}

		pad();
		ng::checkbox("humanize", &scfg.humanize);
		gap();

		if (scfg.humanize)
		{
			row_slider("##silent_reaction", "reaction", &scfg.reaction_ms, 0.0f, 400.0f);
			gap();
		}

		pad();
		ng::checkbox("sticky target", &scfg.sticky);
		gap();

		if (scfg.sticky)
		{
			row_slider("##silent_sticky", "sticky fov", &scfg.sticky_fov_scale, 1.0f, 4.0f);
			gap();
		}

		pad();
		ng::checkbox("prediction", &scfg.prediction);
		gap();

		if (scfg.prediction)
		{
			row_slider("##silent_bs", "bullet speed", &scfg.bullet_speed, 100.0f, 5000.0f);
			gap();
		}
	}
	ng::child_end();
}
