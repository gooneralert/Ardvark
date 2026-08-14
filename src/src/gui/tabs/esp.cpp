#include "pch.h"
#include "esp.h"
#include "helpers.h"
#include "../widgets/widgets.h"
#include "imgui.h"
#include "app/Settings.h"
#include "app/Graphics.h"
#include "features/visuals/boxfill/BoxFill.h"
#include "features/visuals/ShaderChams.h"
#include "features/visuals/MeshDxShader.h"
#include "features/visuals/MeshChams.h"
#include "features/visuals/HavocWorldEsp.h"
#include "features/games/PhantomForces.h"
#include <vector>

namespace
{
	static std::vector<const char*> names_vec(const char* const* names, int n)
	{
		return std::vector<const char*>(names, names + n);
	}

	static void clamp_i(int* v, int lo, int hi)
	{
		if (!v) return;
		if (*v < lo) *v = lo;
		if (*v > hi) *v = hi;
	}
}

void ng_tabs::draw_esp_tab()
{
	using namespace Cheat;

	g_Settings.esp.flags = false;
	g_Settings.esp.bot_esp[Settings::BOT_FLAGS] = false;

	float left_w = 0.f, right_w = 0.f, h = 0.f;
	begin_columns(&left_w, &right_w, &h);

	begin_panel("##esp_child1", left_w, h);
	{
		row_checkbox("enabled", &g_Settings.esp.enabled);
		row_checkbox("draw local", &g_Settings.esp.draw_local);
		row_checkbox("preview", &g_Settings.misc.esp_preview);

		row_checkbox_color("bounding box", &g_Settings.esp.box, g_Settings.esp.box_color);
		if (!g_Settings.esp.box)
			g_Settings.esp.box_fill = false;

		if (g_Settings.esp.box)
		{
			if (g_Settings.esp.box_fill_mode == 1)
				row_checkbox("box fill", &g_Settings.esp.box_fill);
			else
				row_checkbox_color("box fill", &g_Settings.esp.box_fill, g_Settings.esp.box_fill_color);

			if (g_Settings.esp.box_fill)
			{
				static const std::vector<const char*> k_fill_modes = { "color", "image" };
				row_combo("fill type", &g_Settings.esp.box_fill_mode, k_fill_modes);

				if (g_Settings.esp.box_fill_mode == 1)
				{
					static const std::vector<const char*> k_fill_images = {
						"lebrone", "peter", "stewie", "2minion",
						"on_baby", "pink_floyd", "didi_bop", "herobrine",
						"vape", "dok", "panta",
						"charlie-binladen", "kostya", "loshad",
						"elsa", "vovka", "egor", "diddy_kirk", "a-kirk",
						"i_cant_fly", "patriot", "agatha_kirk"
					};
					row_combo("fill image", &g_Settings.esp.box_fill_image, k_fill_images);
					clamp_i(&g_Settings.esp.box_fill_image, 0, 21);

					row_slider_f("fill opacity", &g_Settings.esp.box_fill_image_alpha, 0.0f, 1.0f, "%.2f");
					row_checkbox("remove background", &g_Settings.esp.box_fill_remove_bg);
					Visuals::BoxFill::SetRemoveBg(Core::g_Device, g_Settings.esp.box_fill_remove_bg);
				}
			}
		}

		row_checkbox_color("name", &g_Settings.esp.name, g_Settings.esp.name_color);
		if (g_Settings.esp.name)
		{
			static const std::vector<const char*> k_name_modes = { "display name", "username" };
			row_combo("name type", &g_Settings.esp.name_mode, k_name_modes);
		}

		row_checkbox_color("skeleton", &g_Settings.esp.skeleton, g_Settings.esp.skeleton_color);
		if (g_Settings.esp.skeleton)
		{
			static const std::vector<const char*> k_skel_types = {
				"funny skeleton", "anton", "unfunny_skeleton", "egor"
			};
			row_combo("skeleton type", &g_Settings.esp.skeleton_type, k_skel_types);
			clamp_i(&g_Settings.esp.skeleton_type, 0, 3);
		}

		if (g_Settings.esp.chams_mode != 3 && g_Settings.esp.chams_mode != 4)
		{
			pad();
			widgets::checkbox_color2(
				"chams",
				&g_Settings.esp.chams,
				g_Settings.esp.chams_outline_color,
				g_Settings.esp.chams_fill_color
			);
		}
		else
		{
			row_checkbox("chams", &g_Settings.esp.chams);
		}

		if (g_Settings.esp.chams)
		{
			static const std::vector<const char*> k_chams_modes = {
				"box", "box filled", "clipper", "shader", "mesh"
			};
			row_combo("chams mode", &g_Settings.esp.chams_mode, k_chams_modes);
			clamp_i(&g_Settings.esp.chams_mode, 0, 4);

			if (g_Settings.esp.chams_mode == 3)
			{
				row_combo("shader", &g_Settings.esp.chams_shader,
				          names_vec(Visuals::ShaderChams::StyleNames(),
				                    Visuals::ShaderChams::StyleNameCount()));
			}

			if (g_Settings.esp.chams_mode == 4)
			{
				static const std::vector<const char*> k_mesh_style = { "flat", "shader" };
				row_combo("mesh style", &g_Settings.esp.mesh_chams_style, k_mesh_style);
				clamp_i(&g_Settings.esp.mesh_chams_style, 0, 1);

				if (g_Settings.esp.mesh_chams_style == 1)
				{
					row_combo("mesh shader", &g_Settings.esp.mesh_chams_dx_mode,
					          names_vec(Visuals::MeshDxShader::ModeNames(),
					                    Visuals::MeshDxShader::ModeNameCount()));
					clamp_i(&g_Settings.esp.mesh_chams_dx_mode, 0,
					        Visuals::MeshDxShader::ModeNameCount() - 1);
				}

				row_checkbox_color("mesh outline", &g_Settings.esp.mesh_chams_outline,
				                   g_Settings.esp.mesh_chams_outline_color);

				if (g_Settings.esp.mesh_chams_outline)
				{
					row_combo("outline shader", &g_Settings.esp.mesh_chams_outline_style,
					          names_vec(Visuals::MeshChams::OutlineStyleNames(),
					                    Visuals::MeshChams::OutlineStyleNameCount()));
					clamp_i(&g_Settings.esp.mesh_chams_outline_style, 0,
					        Visuals::MeshChams::OutlineStyleNameCount() - 1);

					row_slider_f("outline fade", &g_Settings.esp.mesh_chams_outline_fade,
					             0.35f, 3.0f, "%.2f");
				}

				row_checkbox_color("occluded", &g_Settings.esp.mesh_chams_occlusion,
				                   g_Settings.esp.mesh_chams_occluded_color);

				if (g_Settings.esp.mesh_chams_occlusion)
				{
					row_combo("occluded shader", &g_Settings.esp.mesh_chams_occluded_dx_mode,
					          names_vec(Visuals::MeshDxShader::ModeNames(),
					                    Visuals::MeshDxShader::ModeNameCount()));
					clamp_i(&g_Settings.esp.mesh_chams_occluded_dx_mode, 0,
					        Visuals::MeshDxShader::ModeNameCount() - 1);
				}
			}
		}

		row_checkbox("engine chams", &g_Settings.esp.engine_chams);
		if (g_Settings.esp.engine_chams)
		{
			static const std::vector<const char*> engine_styles = {
				"default", "ghost", "simple wireframe", "colored frame",
				"colored", "smoke no shadow", "smoke", "invisible",
			};
			row_combo("engine style", &g_Settings.esp.engine_chams_style, engine_styles);
			clamp_i(&g_Settings.esp.engine_chams_style, 0, 7);

			int st = g_Settings.esp.engine_chams_style;
			const bool use_picker = (st == 1 || st == 2 || st == 5 || st == 6);

			if (use_picker)
			{
				g_Settings.esp.engine_chams_color[3] = 1.f;
				pad();
				widgets::color_edit("##engine_chams_col", g_Settings.esp.engine_chams_color);
			}
			else if (st == 3 || st == 4)
			{
				static const std::vector<const char*> engine_colors = {
					"red", "green", "orange", "blue", "pink", "cyan", "white"
				};
				row_combo("engine color", &g_Settings.esp.engine_ghost_color_idx, engine_colors);
			}
		}

		const bool pf_place = Games::PhantomForces::IsActivePlace();
		const bool havoc_place = Visuals::HavocWorldEsp::IsActivePlace();

		if (pf_place)
		{
			g_Settings.esp.healthbar = false;
			g_Settings.esp.health_text = false;
			g_Settings.esp.distance = false;
			g_Settings.esp.tool = false;
			g_Settings.esp.flags = false;
			g_Settings.esp.dead_check = false;
			g_Settings.esp.body_corpse = false;
		}

		if (!pf_place)
		{
			row_checkbox("health bar", &g_Settings.esp.healthbar);
			row_checkbox("health text", &g_Settings.esp.health_text);
			row_checkbox_color("distance", &g_Settings.esp.distance, g_Settings.esp.distance_color);
			row_checkbox_color("tool", &g_Settings.esp.tool, g_Settings.esp.tool_color);
			g_Settings.esp.flags = false;
		}

		if (havoc_place)
		{
			g_Settings.esp.body_corpse = false;

			row_checkbox("dead check", &g_Settings.esp.dead_check);
			row_checkbox("bots", &g_Settings.esp.bots);

			if (g_Settings.esp.bots)
			{
				auto& be = g_Settings.esp.bot_esp;

				{
					float d = g_Settings.esp.bot_max_distance;
					if (d < 50.f) d = 50.f;
					if (d > 400.f) d = 400.f;
					g_Settings.esp.bot_max_distance = d;
					row_slider_f("bot max distance (m)", &g_Settings.esp.bot_max_distance,
					             50.f, 400.f, "%.0f");
				}

				row_checkbox_color("bot box", &be[Settings::BOT_BOX],
				                   g_Settings.esp.bot_box_color);
				row_checkbox_color("bot name", &be[Settings::BOT_NAME],
				                   g_Settings.esp.bot_name_color);
				row_checkbox_color("bot skeleton", &be[Settings::BOT_SKELETON],
				                   g_Settings.esp.bot_skeleton_color);

				if (g_Settings.esp.bot_chams_mode != 3)
				{
					pad();
					widgets::checkbox_color2(
						"bot chams",
						&be[Settings::BOT_CHAMS],
						g_Settings.esp.bot_chams_outline_color,
						g_Settings.esp.bot_chams_fill_color
					);
				}
				else
				{
					row_checkbox("bot chams", &be[Settings::BOT_CHAMS]);
				}

				if (be[Settings::BOT_CHAMS])
				{
					static const std::vector<const char*> k_bot_chams_modes = {
						"box", "box filled", "clipper", "shader"
					};
					if (g_Settings.esp.bot_chams_mode > 3)
						g_Settings.esp.bot_chams_mode = 3;
					row_combo("bot chams mode", &g_Settings.esp.bot_chams_mode, k_bot_chams_modes);

					if (g_Settings.esp.bot_chams_mode == 3)
					{
						row_combo("bot shader", &g_Settings.esp.bot_chams_shader,
						          names_vec(Visuals::ShaderChams::StyleNames(),
						                    Visuals::ShaderChams::StyleNameCount()));
					}
				}

				row_checkbox("bot health bar", &be[Settings::BOT_HEALTHBAR]);
				row_checkbox("bot health text", &be[Settings::BOT_HEALTH_TEXT]);
				row_checkbox_color("bot distance", &be[Settings::BOT_DISTANCE],
				                   g_Settings.esp.bot_distance_color);
				row_checkbox_color("bot tool", &be[Settings::BOT_TOOL],
				                   g_Settings.esp.bot_tool_color);
				be[Settings::BOT_FLAGS] = false;
			}

			row_checkbox_color("corpses", &g_Settings.esp.corpses, g_Settings.esp.corpse_color);
			row_checkbox_color("ground loot", &g_Settings.esp.ground_loot,
			                   g_Settings.esp.ground_loot_color);

			if (g_Settings.esp.ground_loot)
			{
				row_checkbox("loot chams", &g_Settings.esp.loot_chams);
				if (g_Settings.esp.loot_chams)
				{
					row_combo("loot shader", &g_Settings.esp.loot_chams_shader,
					          names_vec(Visuals::ShaderChams::StyleNames(),
					                    Visuals::ShaderChams::StyleNameCount()));
				}

				static const std::vector<const char*> k_loot_filters = {
					"weapons", "mags", "ammo", "attachments", "medical",
					"valuables", "tools", "electronics", "households",
					"documents", "other"
				};
				row_multicombo("##loot_filters", "loot filters",
				               g_Settings.esp.loot_filter, k_loot_filters);
			}

			row_checkbox_color("containers", &g_Settings.esp.containers,
			                   g_Settings.esp.containers_color);

			if (g_Settings.esp.containers)
			{
				row_checkbox("container chams", &g_Settings.esp.containers_chams);
				if (g_Settings.esp.containers_chams)
				{
					row_combo("crate shader", &g_Settings.esp.containers_chams_shader,
					          names_vec(Visuals::ShaderChams::StyleNames(),
					                    Visuals::ShaderChams::StyleNameCount()));
				}
			}
		}
		else if (!pf_place)
		{
			g_Settings.esp.bots = true;
			g_Settings.esp.corpses = false;
			g_Settings.esp.ground_loot = false;
			g_Settings.esp.containers = false;

			if (!g_Settings.esp.body_corpse)
				row_checkbox("dead check", &g_Settings.esp.dead_check);
			else
				g_Settings.esp.dead_check = false;

			if (!g_Settings.esp.dead_check)
			{
				row_checkbox_color("body corpse", &g_Settings.esp.body_corpse,
				                   g_Settings.esp.corpse_color);
			}
			else
			{
				g_Settings.esp.body_corpse = false;
			}
		}

		row_checkbox_color("tracer", &g_Settings.esp.tracer, g_Settings.esp.tracer_color);
		if (g_Settings.esp.tracer)
		{
			static const std::vector<const char*> k_tracer_origin = {
				"bottom", "center", "mouse", "top"
			};
			row_combo("tracer origin", &g_Settings.esp.tracer_origin, k_tracer_origin);
		}

		row_checkbox_color("china hat", &g_Settings.esp.china_hat, g_Settings.esp.china_hat_color);
		if (g_Settings.esp.china_hat)
		{
			row_slider_f("hat height", &g_Settings.esp.china_hat_height, 0.3f, 3.0f, "%.2f");
			row_slider_f("hat radius", &g_Settings.esp.china_hat_radius, 0.4f, 3.5f, "%.2f");
		}

		row_checkbox_color("hit chams", &g_Settings.esp.hit_chams, g_Settings.esp.hit_chams_color);
		if (g_Settings.esp.hit_chams)
			row_slider_f("hit fade", &g_Settings.esp.hit_chams_duration, 0.1f, 3.0f, "%.2f");

		row_checkbox_color("offscreen arrows", &g_Settings.esp.offscreen_arrows,
		                   g_Settings.esp.arrow_color);
		if (g_Settings.esp.offscreen_arrows)
		{
			static const std::vector<const char*> k_arrow_info = {
				"name", "distance", "health", "tool"
			};
			row_multicombo("##arrow_info", "arrow info",
			               g_Settings.esp.arrow_info, k_arrow_info);
			row_slider_f("arrow size", &g_Settings.esp.arrow_size, 6.0f, 32.0f, "%.0f");
			row_slider_f("arrow radius", &g_Settings.esp.arrow_radius, 40.0f, 500.0f, "%.0f");
		}
	}
	end_panel();

	ImGui::SameLine(0.f, panel_gap);

	begin_panel("##esp_child2", right_w, h);
	{
		static const std::vector<const char*> k_esp_fonts = {
			"fredoka one", "tahoma bold", "proggy clean", "visitor", "verdana", "imgui"
		};
		row_combo("esp font", &g_Settings.esp.font, k_esp_fonts);
		row_slider_f("esp font size", &g_Settings.esp.font_size, 8.0f, 24.0f, "%.0f");

		static const std::vector<const char*> k_box_modes = { "bounding", "corner", "3d" };
		row_combo("box style", &g_Settings.esp.box_mode, k_box_modes);

		static const std::vector<const char*> k_bound_types = { "parts", "mesh" };
		row_combo("bounding type", &g_Settings.esp.bounding_type, k_bound_types);
		clamp_i(&g_Settings.esp.bounding_type, 0, 1);

		static const std::vector<const char*> k_esp_outline = { "skeleton", "box" };
		row_multicombo("##esp_outline", "esp outline",
		               g_Settings.esp.esp_outline, k_esp_outline);

		if (g_Settings.esp.skeleton_type == 0)
			row_slider_f("skeleton thickness", &g_Settings.esp.skeleton_thickness, 1.0f, 6.0f, "%.1f");

		row_slider_f("box thickness", &g_Settings.esp.box_thickness, 0.5f, 6.0f, "%.1f");

		if (Visuals::HavocWorldEsp::IsActivePlace())
		{
			pad();
			ImGui::TextUnformatted("distance: meters (max 400)");
		}
		else
		{
			static const std::vector<const char*> k_dist_units = { "studs", "meters" };
			const int prev_unit = g_Settings.esp.distance_unit;
			row_combo("distance unit", &g_Settings.esp.distance_unit, k_dist_units);
			if (prev_unit != g_Settings.esp.distance_unit)
			{
				float k = 0.28f;
				if (g_Settings.esp.distance_unit == 1)
					g_Settings.esp.max_distance *= k;
				else if (k > 0.f)
					g_Settings.esp.max_distance /= k;
			}

			row_checkbox("distance check", &g_Settings.esp.distance_check);

			{
				bool meters = g_Settings.esp.distance_unit == 1;
				float lo = meters ? 10.f : 50.f;
				float hi = meters ? 1400.f : 5000.f;
				float d = g_Settings.esp.max_distance;
				if (d < lo) d = lo;
				if (d > hi) d = hi;
				g_Settings.esp.max_distance = d;
				row_slider_f(meters ? "max distance (meters)" : "max distance (studs)",
				             &g_Settings.esp.max_distance, lo, hi, "%.0f");
			}
		}
	}
	end_panel();
}
