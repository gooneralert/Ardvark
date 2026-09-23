#include "pch.h"
#include "customize.h"
#include "helpers.h"
#include "../liquid_ui.h"
#include "imgui.h"
#include "app/Settings.h"
#include <vector>

namespace
{
	static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

	// 16-swatch accent palette, ported from the example's Customize page -
	// picks write rgb into gui.accent and immediately re-tint the kit
	static void accent_palette(float accent[4])
	{
		ImGuiIO& io = ImGui::GetIO();
		static int sel = -1;
		struct AC { float r, g, b; };
		static const AC pal[16] = {
			{0.02f,0.48f,1.0f},{0.20f,0.78f,0.35f},{1.0f,0.58f,0.0f},{1.0f,0.27f,0.23f},
			{0.69f,0.32f,0.87f},{1.0f,0.18f,0.33f},{0.35f,0.34f,0.84f},{0.0f,0.78f,0.75f},
			{1.0f,0.80f,0.0f},{0.56f,0.79f,0.98f},{0.99f,0.45f,0.52f},{0.40f,0.47f,0.55f},
			{0.30f,0.85f,0.39f},{0.58f,0.44f,0.86f},{0.0f,0.60f,0.55f},{0.93f,0.31f,0.59f}
		};
		ImVec2 base = ImGui::GetCursorScreenPos();
		ImDrawList* sd = ImGui::GetWindowDrawList();
		const float sw = 30.0f, gp = 10.0f;
		for (int i = 0; i < 16; ++i)
		{
			int r = i / 8, c = i % 8;
			float cx = base.x + c * (sw + gp) + sw * 0.5f, cy = base.y + r * (sw + gp) + sw * 0.5f;
			ImGui::SetCursorScreenPos(ImVec2(cx - sw * 0.5f, cy - sw * 0.5f));
			ImGui::PushID(i);
			ImGui::InvisibleButton("sw", ImVec2(sw, sw));
			bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
			if (ImGui::IsItemDeactivated() && hov)
			{
				accent[0] = pal[i].r; accent[1] = pal[i].g; accent[2] = pal[i].b;
				sel = i;
			}
			Glass::Spring& ss = Glass::g->springs().Get((uint32_t)ImGui::GetID("sw"), 1, Glass::SpringStyle::Bouncy, 1.0f);
			ss.target = act ? 0.86f : (hov ? 1.18f : 1.0f); ss.Tick(io.DeltaTime);
			Glass::Spring& sr = Glass::g->springs().Get((uint32_t)ImGui::GetID("sw"), 2, Glass::SpringStyle::Bouncy, sel == i ? 1.0f : 0.0f);
			sr.target = (sel == i) ? 1.0f : 0.0f; sr.Tick(io.DeltaTime);
			ImGui::PopID();
			float rad = sw * 0.5f * clampf(ss.x, 0.8f, 1.25f);
			ImU32 col = IM_COL32((int)(pal[i].r * 255), (int)(pal[i].g * 255), (int)(pal[i].b * 255), 255);
			if (hov) sd->AddCircleFilled(ImVec2(cx, cy + 2.0f), rad + 1.5f, IM_COL32(0, 0, 0, 45));
			sd->AddCircleFilled(ImVec2(cx, cy), rad, col);
			sd->AddCircleFilled(ImVec2(cx - rad * 0.28f, cy - rad * 0.32f), rad * 0.42f, IM_COL32(255, 255, 255, 55));
			float k = clampf(sr.x, 0.0f, 1.0f);
			if (k > 0.01f)
			{
				sd->AddCircle(ImVec2(cx, cy), rad + 2.0f + 3.0f * k, Glass::InkColor(), 0, 1.6f + 1.0f * k);
				float m = rad * 0.40f * k;
				ImU32 wcol = IM_COL32(255, 255, 255, (int)(235 * k));
				sd->AddLine(ImVec2(cx - m, cy + m * 0.05f), ImVec2(cx - m * 0.15f, cy + m * 0.7f), wcol, 2.2f);
				sd->AddLine(ImVec2(cx - m * 0.15f, cy + m * 0.7f), ImVec2(cx + m, cy - m * 0.6f), wcol, 2.2f);
			}
		}
		ImGui::SetCursorScreenPos(base);
		ImGui::Dummy(ImVec2(8 * (sw + gp), 2 * (sw + gp) + 6.0f));
	}
}

void ng_tabs::draw_customize_tab()
{
	using namespace Cheat;
	auto& gui = g_Settings.gui;

	float left_w = 0.f, right_w = 0.f, h = 0.f;
	begin_columns(&left_w, &right_w, &h);

	// left column: the live glass-material knobs (ported from the example's
	// Customize page - GLASS MATERIAL / LIGHTING / MORE GLASS)
	begin_panel("##cu_perf", left_w, h, true);
	{
		// the example's PERFORMANCE panel: overlay + glass refresh rates
		row_slider_i("overlay refresh (Hz)", &gui.fps_cap, 0, 480);
		row_slider_i("glass refresh (Hz)", &gui.blur_hz, 1, 240);

		row_slider_f("saturation", &gui.glass_sat, 0.3f, 3.f, "%.2f");
		row_slider_f("refraction", &gui.glass_refr, 0.f, 60.f, "%.1f");
		row_slider_f("chroma", &gui.glass_chroma, 0.f, 8.f, "%.2f");
		row_slider_f("edge glow", &gui.glass_edge, 0.f, 1.f, "%.2f");
		row_slider_f("shadow", &gui.glass_shadow, 0.f, 1.f, "%.2f");
		row_slider_f("liquid flow", &gui.glass_flow, 0.f, 10.f, "%.1f");

		row_slider_f("light 1 angle", &gui.light1_angle, 0.f, 360.f, "%.0f");
		row_slider_f("light 1 strength", &gui.light1_amt, 0.f, 3.f, "%.2f");
		row_slider_f("light 2 angle", &gui.light2_angle, 0.f, 360.f, "%.0f");
		row_slider_f("light 2 strength", &gui.light2_amt, 0.f, 3.f, "%.2f");

		row_slider_f("fresnel", &gui.fresnel, 1.f, 6.f, "%.2f");
		row_slider_f("bevel", &gui.bevel, 0.f, 2.f, "%.2f");
		row_slider_f("specular", &gui.specular_amt, 0.f, 3.f, "%.2f");
		row_slider_f("sheen", &gui.sheen, 0.f, 3.f, "%.2f");
		row_slider_f("grain", &gui.grain, 0.f, 4.f, "%.2f");
		row_slider_f("ambient rim", &gui.ambient_rim, 0.f, 2.f, "%.2f");
	}
	end_panel();

	ImGui::SameLine(0.f, panel_gap);

	// right column: appearance + accent (example's APPEARANCE / ACCENT COLOR)
	begin_panel("##cu_theme", right_w, h, true);
	{
		static const std::vector<const char*> k_theme = { "dark", "light" };
		row_combo("theme", &gui.appearance, k_theme);

		row_checkbox("dim background", &gui.dim_backdrop);

		accent_palette(gui.accent);

		// custom accent through the same rgba swatch row everything else uses
		gui.accent[3] = 1.f;
		row_color("custom accent", gui.accent);
	}
	end_panel();
}