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
	using ng_tabs::begin_column;
	using ng_tabs::begin_columns;
	using ng_tabs::begin_section;
	using ng_tabs::end_column;
	using ng_tabs::end_section;
	using ng_tabs::expand_row;
	using ng_tabs::panel_gap;
	using ng_tabs::row_checkbox;
	using ng_tabs::row_checkbox_color;
	using ng_tabs::row_checkbox_keybind;
	using ng_tabs::row_checkbox_more;
	using ng_tabs::row_checkbox_expand;
	using ng_tabs::row_combo;
	using ng_tabs::row_combo_more;
	using ng_tabs::row_keybind;
	using ng_tabs::row_multicombo;
	using ng_tabs::row_placeholder;
	using ng_tabs::row_slider_f;
	using ng_tabs::section_header;

	// (a using-declaration can't name a nested type - typedef instead)
	typedef Cheat::Settings::AimbotConfig AimbotConfig;

	static const std::vector<const char*> k_part_names = {
		"head", "upper torso", "lower torso", "hrp",
		"left hand", "right hand", "left foot", "right foot"
	};
	static const std::vector<const char*> k_target = { "fov center", "distance", "lowest hp" };
	static const std::vector<const char*> k_part_sel = { "cycle", "random", "closest" };
	static const std::vector<const char*> k_pos = { "center", "mouse" };

	// target selection rows - shared by the Aimbot and Silent pages
	void cfg_target(AimbotConfig& cfg)
	{
		row_combo("target select", &cfg.target_select, k_target);
		if (row_multicombo("##parts", "parts", cfg.parts, k_part_names))
			cfg.SyncTiersFromParts();
		row_combo("part select", &cfg.part_select, k_part_sel);
	}

	// visibility / validity checks
	void cfg_checks(AimbotConfig& cfg)
	{
		row_checkbox_more("visible only", &cfg.visible_only);
		if (cfg.visible_only)
			Cheat::g_Settings.misc.raycast_engine = true;   // the raycast engine is required
		row_checkbox("dead check", &cfg.dead_check);

		row_checkbox_more("distance check", &cfg.distance_check);
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
	}

	// fov / aim position / tracer
	void cfg_selection(AimbotConfig& cfg)
	{
		static const std::vector<const char*> k_fov_style = { "circle", "filled" };

		row_checkbox_color("fov check", &cfg.fov_enabled, cfg.fov_color);
		if (cfg.fov_enabled)
		{
			row_combo("fov style", &cfg.fov_style, k_fov_style);
			row_checkbox_color("fov outline", &cfg.fov_outline, cfg.fov_outline_color);
			row_slider_f("fov size", &cfg.fov_size, 10.0f, 600.0f, "%.0f");
		}

		row_combo("aim pos", &cfg.fov_position, k_pos);
		row_checkbox_color("aim tracer", &cfg.tracer, cfg.tracer_color);
	}

	// everything the reference parks in its "OTHER" column
	void cfg_advanced(AimbotConfig& cfg, bool show_smooth, bool show_hitchance)
	{
		if (show_smooth)
		{
			static const std::vector<const char*> k_curve = {
				"linear", "exponential", "spring", "bezier"
			};
			row_checkbox_more("smoothing", &cfg.smooth_enabled);
			if (cfg.smooth_enabled)
			{
				row_combo("aim curve", &cfg.aim_curve, k_curve);
				row_slider_f("sensitivity", &cfg.aim_sensitivity, 0.05f, 3.0f, "%.2f");
				row_slider_f("smoothness x", &cfg.smooth_x, 0.1f, 5.0f, "%.2f");
				row_slider_f("smoothness y", &cfg.smooth_y, 0.1f, 5.0f, "%.2f");
			}
		}

		row_checkbox_more("sticky target", &cfg.sticky);
		if (cfg.sticky)
			row_slider_f("sticky fov", &cfg.sticky_fov_scale, 1.0f, 4.0f, "%.2f");

		row_checkbox_more("prediction", &cfg.prediction);
		if (cfg.prediction)
			row_slider_f("bullet speed", &cfg.bullet_speed, 100.0f, 5000.0f, "%.0f");

		row_checkbox_more("humanize", &cfg.humanize);
		if (cfg.humanize)
			row_slider_f("reaction", &cfg.reaction_ms, 0.0f, 400.0f, "%.0f");

		if (show_hitchance)
		{
			row_checkbox_more("hitchance", &cfg.hitchance_enabled);
			if (cfg.hitchance_enabled)
				row_slider_f("hit chance", &cfg.hitchance, 1.0f, 100.0f, "%.0f");
		}
	}
}

// -----------------------------------------------------------------------------
// Aimbot page - the reference's exact row set:
//   MAIN      : Enabled / Team Check / Visible Check / Health Check /
//               Sticky Aim / Rage Method / Resolver
//   SELECTION : Distance / Sensitivity / Hit Part / Aim Type
//   OTHER     : Prediction / Smoothing / Field Of View
// Rows the reference shows that we do not implement are kept as placeholders:
// they toggle and remember their state, they just do not drive anything yet.
// -----------------------------------------------------------------------------
namespace
{
	void aim_main(AimbotConfig& cfg)
	{
		// "Enabled" is the real master switch: on = an aim mode is selected,
		// off = type "off". Its sub-rows hold the key and the distance check.
		{
			bool on = Cheat::g_Settings.aim.type != 2;
			// the reference shows no key/distance rows here by default - they
			// live behind the marker so MAIN stays a clean list of toggles
			static bool s_enabled_open = false;
			row_checkbox_expand("Enabled", &on, &s_enabled_open);
			Cheat::g_Settings.aim.type = on ? 0 : 2;
			if (on && s_enabled_open)
			{
				row_keybind("##aim_kb", "aim key", &Cheat::g_Settings.aim.bind,
				            &Cheat::g_Settings.aim.bind_mode);
				row_checkbox("distance check", &cfg.distance_check);
			}
		}

		row_checkbox("Team Check", &Cheat::g_Settings.misc.teamcheck);

		row_checkbox_more("Visible Check", &cfg.visible_only);
		if (cfg.visible_only)
		{
			Cheat::g_Settings.misc.raycast_engine = true;   // the raycast engine is required
			row_checkbox("dead check", &cfg.dead_check);
		}

		row_placeholder("Health Check");
		row_placeholder("Rage Method");
		row_placeholder("Resolver");

		row_checkbox_more("Sticky Aim", &cfg.sticky);
		if (cfg.sticky)
			row_slider_f("sticky fov", &cfg.sticky_fov_scale, 1.0f, 4.0f, "%.2f");
	}

	void aim_selection(AimbotConfig& cfg)
	{
		if (Cheat::Visuals::HavocWorldEsp::IsActivePlace())
		{
			float hi = Cheat::Visuals::HavocWorldEsp::MaxRangeStuds();
			float d = cfg.max_distance;
			if (d < 50.f) d = 50.f;
			if (d > hi) d = hi;
			cfg.max_distance = d;
			row_slider_f("Distance", &cfg.max_distance, 50.0f, hi, "%.0f");
		}
		else
		{
			row_slider_f("Distance", &cfg.max_distance, 50.0f, 5000.0f, "%.0f");
		}

		row_slider_f("Sensitivity", &cfg.aim_sensitivity, 0.05f, 3.0f, "%.2f");

		// ----------------------------------------------------------------
		// The reference shows EXACTLY four rows here. Everything else we
		// need for targeting lives behind the "..." marker of the row that
		// owns it, so the panel matches line for line until you ask for it.
		// ----------------------------------------------------------------

		// Hit Part: the reference's single-select dropdown. We store body
		// parts as a multi-select, so picking one enables exactly that part;
		// the multi-select chips and the pick rule expand behind the marker.
		{
			int first = -1;
			for (int i = 0; i < (int)k_part_names.size(); ++i)
				if (cfg.parts[i]) { first = i; break; }
			int sel = (first < 0) ? 0 : first;

			static bool s_parts_open = false;
			if (row_combo_more("Hit Part", &sel, k_part_names, &s_parts_open))
			{
				for (int i = 0; i < (int)k_part_names.size(); ++i)
					cfg.parts[i] = (i == sel);
				cfg.SyncTiersFromParts();
			}

			if (s_parts_open)
			{
				if (row_multicombo("##parts", "parts", cfg.parts, k_part_names))
					cfg.SyncTiersFromParts();
				row_combo("part select", &cfg.part_select, k_part_sel);
			}
		}

		// Aim Type: everything about how the target is picked sits under it
		{
			static const std::vector<const char*> k_aim_type = { "mouse", "camera", "off" };
			static bool s_type_open = false;
			row_combo_more("Aim Type", &Cheat::g_Settings.aim.type, k_aim_type, &s_type_open);

			if (s_type_open)
			{
				row_combo("target select", &cfg.target_select, k_target);
				row_combo("aim pos", &cfg.fov_position, k_pos);
				if (Cheat::Visuals::HavocWorldEsp::IsActivePlace())
					row_checkbox("target bots", &Cheat::g_Settings.aim.target_bots);
			}
		}
	}

	void aim_other(AimbotConfig& cfg, bool show_smooth)
	{
		// the reference always shows these three, so the row set does not change
		// with the aim mode; `show_smooth` is kept for the call site only
		(void)show_smooth;

		static bool s_pred_open = false;
		static bool s_smooth_open = false;
		static bool s_fov_open = false;

		if (expand_row("Prediction", &s_pred_open, nullptr))
		{
			row_checkbox("prediction", &cfg.prediction);
			if (cfg.prediction)
				row_slider_f("bullet speed", &cfg.bullet_speed, 100.0f, 5000.0f, "%.0f");
		}

		if (expand_row("Smoothing", &s_smooth_open, nullptr))
		{
			static const std::vector<const char*> k_curve = {
				"linear", "exponential", "spring", "bezier"
			};
			row_checkbox("smoothing", &cfg.smooth_enabled);
			if (cfg.smooth_enabled)
			{
				row_combo("aim curve", &cfg.aim_curve, k_curve);
				row_slider_f("smoothness x", &cfg.smooth_x, 0.1f, 5.0f, "%.2f");
				row_slider_f("smoothness y", &cfg.smooth_y, 0.1f, 5.0f, "%.2f");

				row_checkbox_more("humanize", &cfg.humanize);
				if (cfg.humanize)
					row_slider_f("reaction", &cfg.reaction_ms, 0.0f, 400.0f, "%.0f");
			}
		}

		// the square on this row is the master switch, the rest expands
		if (expand_row("Field Of View", &s_fov_open, &cfg.fov_enabled))
		{
			static const std::vector<const char*> k_fov_style = { "circle", "filled" };
			row_combo("fov style", &cfg.fov_style, k_fov_style);
			row_checkbox_color("fov outline", &cfg.fov_outline, cfg.fov_outline_color);
			row_slider_f("fov size", &cfg.fov_size, 10.0f, 600.0f, "%.0f");
			row_checkbox_color("aim tracer", &cfg.tracer, cfg.tracer_color);
		}
	}
}

void ng_tabs::draw_aimbot_page()
{
	using namespace Cheat;

	Settings::AimbotConfig& cfg = g_Settings.aim.active();

	float left_w = 0.f, right_w = 0.f, h = 0.f;
	begin_columns(&left_w, &right_w, &h);

	begin_column("##aim_l", left_w, h);
	{
		section_header("MAIN");
		begin_section("##aim_main");
		{
			aim_main(cfg);
		}
		end_section();
	}
	end_column();

	ImGui::SameLine(0.f, panel_gap);

	begin_column("##aim_r", right_w, h);
	{
		section_header("SELECTION");
		begin_section("##aim_sel");
		{
			aim_selection(cfg);
		}
		end_section();

		section_header("OTHER");
		begin_section("##aim_other");
		{
			aim_other(cfg, g_Settings.aim.type == 0 || g_Settings.aim.type == 1);
		}
		end_section();
	}
	end_column();
}

// -----------------------------------------------------------------------------
// Silent page: MAIN + CHECKS | SELECTION + OTHER
// -----------------------------------------------------------------------------
void ng_tabs::draw_silent_page()
{
	using namespace Cheat;

	Settings::AimbotConfig& cfg = g_Settings.aim.silent;

	float left_w = 0.f, right_w = 0.f, h = 0.f;
	begin_columns(&left_w, &right_w, &h);

	begin_column("##sil_l", left_w, h);
	{
		section_header("MAIN");
		begin_section("##sil_main");
		{
			row_keybind("##silent_kb", "silent key",
			            &g_Settings.aim.silent_bind, &g_Settings.aim.silent_bind_mode);

			const bool pf_aim = Games::PhantomForces::IsActivePlace();
			const bool hybrid = g_Settings.misc.hybrid_mode;

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

				// raycast / magic bullet silent are hybrid-mode gated: without it
				// only viewport / mouse stay selectable
				if (!hybrid &&
				    (g_Settings.aim.silent_method == Settings::SILENT_RAYCAST ||
				     g_Settings.aim.silent_method == Settings::SILENT_MAGIC_BULLET))
					g_Settings.aim.silent_method = Settings::SILENT_VIEWPORT;

				static const char* k_names[] = { "viewport", "mouse", "raycast", "magic bullet" };
				static const int k_vals[] = {
					Settings::SILENT_VIEWPORT, Settings::SILENT_MOUSE,
					Settings::SILENT_RAYCAST,  Settings::SILENT_MAGIC_BULLET
				};
				const int count = hybrid ? 4 : 2;

				const int method = g_Settings.aim.silent_method;
				int sel = 0;
				for (int i = 0; i < count; i++)
				{
					if (k_vals[i] == method) { sel = i; break; }
				}

				std::vector<const char*> items(k_names, k_names + count);
				row_combo("silent method", &sel, items);
				g_Settings.aim.silent_method = k_vals[sel];
			}

			if (!pf_aim && hybrid && g_Settings.aim.silent_method == Settings::SILENT_RAYCAST)
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

			cfg_target(cfg);
		}
		end_section();

		section_header("CHECKS");
		begin_section("##sil_checks");
		{
			cfg_checks(cfg);
		}
		end_section();
	}
	end_column();

	ImGui::SameLine(0.f, panel_gap);

	begin_column("##sil_r", right_w, h);
	{
		section_header("SELECTION");
		begin_section("##sil_sel");
		{
			cfg_selection(cfg);
		}
		end_section();

		section_header("OTHER");
		begin_section("##sil_other");
		{
			cfg_advanced(cfg, false, true);
		}
		end_section();
	}
	end_column();
}
