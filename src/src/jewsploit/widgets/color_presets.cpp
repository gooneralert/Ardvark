#include "color_presets.h"
#include "colorpicker.h"
#include "spacing.h"
#include "../colors/colors.h"
#include "../animation/animation.h"

#include <stdio.h>
#include <math.h>
#include <imgui.h>

namespace
{
	// дефолт кружки как на рефе
	static const ImU32 g_presets[] = {
		IM_COL32(121, 134, 203, 255),
		IM_COL32(245, 220, 130, 255),
		IM_COL32(156, 110, 210, 255),
		IM_COL32(240, 242, 248, 255),
		IM_COL32(230, 70, 70, 255),
		IM_COL32(130, 210, 140, 255),
		IM_COL32(120, 80, 180, 255),
	};

	// engine charm Param 1..7 — 1:1 с nearest
	static const ImU32 g_engine_presets[] = {
		IM_COL32(255, 0, 0, 255),
		IM_COL32(0, 255, 0, 255),
		IM_COL32(255, 128, 0, 255),
		IM_COL32(0, 128, 255, 255),
		IM_COL32(255, 0, 255, 255),
		IM_COL32(0, 255, 255, 255),
		IM_COL32(255, 255, 255, 255),
	};

	static const int g_preset_n = 7;

	void u32_to_rgba(ImU32 c, float out[4])
	{
		out[0] = ((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.f;
		out[1] = ((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.f;
		out[2] = ((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.f;
		out[3] = ((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.f;
	}

	float col_dist(const float a[4], ImU32 b)
	{
		float br = ((b >> IM_COL32_R_SHIFT) & 0xFF) / 255.f;
		float bg = ((b >> IM_COL32_G_SHIFT) & 0xFF) / 255.f;
		float bb = ((b >> IM_COL32_B_SHIFT) & 0xFF) / 255.f;
		float dr = a[0] - br;
		float dg = a[1] - bg;
		float db = a[2] - bb;
		return dr * dr + dg * dg + db * db;
	}

	int nearest_preset(const float col[4], const ImU32* tab)
	{
		if (!tab)
		{
			tab = g_presets;
		}

		int best = -1;
		float bd = 0.02f;
		for (int i = 0; i < g_preset_n; i++)
		{
			float d = col_dist(col, tab[i]);
			if (d < bd)
			{
				bd = d;
				best = i;
			}
		}
		return best;
	}

	void draw_brush(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		float x = c.x;
		float y = c.y;
		ImVec2 tip = ImVec2(x - s * 0.35f, y + s * 0.38f);
		ImVec2 mid = ImVec2(x + s * 0.05f, y - s * 0.05f);
		ImVec2 end = ImVec2(x + s * 0.42f, y - s * 0.42f);
		dl->AddLine(tip, mid, col, 2.2f);
		dl->AddLine(mid, end, col, 2.0f);
		dl->AddCircleFilled(ImVec2(end.x + 1.f, end.y - 1.f), s * 0.22f, col, 8);
	}

	void draw_check(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
	{
		ImVec2 a = ImVec2(c.x - r * 0.45f, c.y + r * 0.05f);
		ImVec2 b = ImVec2(c.x - r * 0.12f, c.y + r * 0.38f);
		ImVec2 d = ImVec2(c.x + r * 0.48f, c.y - r * 0.35f);
		dl->AddLine(a, b, col, 1.6f);
		dl->AddLine(b, d, col, 1.6f);
	}
}

bool ng::color_presets(const char* id, const char* title, const char* desc, float col[4], bool shown, bool show_brush)
{
	if (!id || !title || !col)
	{
		return false;
	}

	ImGuiIO& io = ImGui::GetIO();
	ImGuiStorage* st = ImGui::GetStateStorage();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImGui::PushID(id);

	ImGuiID oid = ImGui::GetID("open");
	float open = st->GetFloat(oid, shown ? 1.f : 0.f);
	open = anim::approach(open, shown ? 1.f : 0.f, 7.f, io.DeltaTime);
	st->SetFloat(oid, open);
	float e = anim::ease_out_cubic(open);

	if (e < 0.001f)
	{
		ImGui::PopID();
		return false;
	}

	float pad_x = 12.f;
	float title_h = 18.f;
	float desc_h = desc && desc[0] ? 16.f : 0.f;
	float gap_td = desc_h > 0.f ? 4.f : 0.f;
	float gap_bar = 10.f;
	float bar_h = 36.f;
	float bar_r = 10.f;
	float brush_sz = 22.f;
	float dot_r = 7.f;
	float hex_w = 72.f;

	// top gap как у слайдера — иначе дыра/слипание с чекбоксом
	float top = item_gap * e;
	float body_h = title_h + gap_td + desc_h + gap_bar + bar_h;
	float total_h = top + body_h * e;
	if (total_h < 1.f)
	{
		ImGui::PopID();
		return false;
	}

	ImVec2 pos = ImGui::GetCursorScreenPos();
	float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
	float bar_x0 = pos.x;
	float bar_x1 = right - pad_x;
	float bar_w = bar_x1 - bar_x0;
	if (bar_w < 120.f) bar_w = 120.f;

	ImGui::PushClipRect(pos, ImVec2(bar_x0 + bar_w + pad_x, pos.y + total_h), true);

	float y = pos.y + top;
	int a = (int)(230.f * e + 0.5f);
	if (a < 0) a = 0;
	if (a > 255) a = 255;

	dl->AddText(ImVec2(bar_x0, y), IM_COL32(235, 238, 245, a), title);
	y += title_h + gap_td;

	if (desc_h > 0.f)
	{
		int da = (int)(140.f * e + 0.5f);
		dl->AddText(ImVec2(bar_x0, y), IM_COL32(150, 156, 168, da), desc);
		y += desc_h;
	}

	y += gap_bar;
	ImVec2 b0 = ImVec2(bar_x0, y);
	ImVec2 b1 = ImVec2(bar_x0 + bar_w, y + bar_h);

	ImU32 bar_bg = IM_COL32(22, 24, 30, (int)(220.f * e));
	ImU32 bar_br = IM_COL32(255, 255, 255, (int)(18.f * e));
	dl->AddRectFilled(b0, b1, bar_bg, bar_r);
	dl->AddRect(b0, b1, bar_br, bar_r, 0, 1.f);

	float mid_y = (b0.y + b1.y) * 0.5f;
	float brush_x = b0.x + 10.f;
	bool brush_hit = false;
	bool ch = false;
	const ImU32* tab = show_brush ? g_presets : g_engine_presets;

	if (show_brush)
	{
		ImVec2 brush_c = ImVec2(brush_x + brush_sz * 0.5f, mid_y);

		ImGui::SetCursorScreenPos(ImVec2(brush_x, mid_y - brush_sz * 0.5f));
		ImGui::InvisibleButton("brush", ImVec2(brush_sz, brush_sz));
		brush_hit = ImGui::IsItemClicked() && e > 0.55f;
		bool brush_hov = ImGui::IsItemHovered();

		// кисть = текущий цвет, не accent
		int ba = (int)((brush_hov ? 255.f : 220.f) * e * col[3] + 0.5f);
		if (ba < 0) ba = 0;
		if (ba > 255) ba = 255;
		ImU32 brush_col = IM_COL32(
			(int)(col[0] * 255.f + 0.5f),
			(int)(col[1] * 255.f + 0.5f),
			(int)(col[2] * 255.f + 0.5f),
			ba
		);
		draw_brush(dl, brush_c, brush_sz * 0.55f, brush_col);
	}

	float dots_x0 = show_brush ? (brush_x + brush_sz + 10.f) : (b0.x + 10.f);
	float dots_x1 = b1.x - hex_w - 8.f;
	float dots_w = dots_x1 - dots_x0;
	float step = dots_w / (float)g_preset_n;
	if (step < dot_r * 2.f + 2.f) step = dot_r * 2.f + 2.f;

	int sel = nearest_preset(col, tab);

	for (int i = 0; i < g_preset_n; i++)
	{
		float cx = dots_x0 + step * ((float)i + 0.5f);
		ImVec2 dc = ImVec2(cx, mid_y);

		ImGui::SetCursorScreenPos(ImVec2(cx - dot_r - 2.f, mid_y - dot_r - 2.f));
		char did[16]{};
		snprintf(did, sizeof(did), "d%d", i);
		ImGui::InvisibleButton(did, ImVec2(dot_r * 2.f + 4.f, dot_r * 2.f + 4.f));
		bool dhit = ImGui::IsItemClicked() && e > 0.55f;
		bool dhov = ImGui::IsItemHovered();

		ImU32 pc = tab[i];
		int pa = (int)((((pc >> 24) & 0xFF) * e) + 0.5f);
		pc = (pc & 0x00FFFFFFu) | ((ImU32)pa << 24);

		float rr = dot_r;
		if (dhov) rr += 1.f;
		dl->AddCircleFilled(dc, rr, pc, 16);
		dl->AddCircle(dc, rr, IM_COL32(255, 255, 255, (int)(28.f * e)), 16, 1.f);

		if (sel == i)
		{
			draw_check(dl, dc, rr, IM_COL32(20, 22, 28, (int)(230.f * e)));
		}

		if (dhit)
		{
			float keep_a = col[3];
			u32_to_rgba(tab[i], col);
			col[3] = keep_a;
			if (col[3] < 0.01f) col[3] = 1.f;
			ch = true;
		}
	}

	int ri = (int)(col[0] * 255.f + 0.5f);
	int gi = (int)(col[1] * 255.f + 0.5f);
	int bi = (int)(col[2] * 255.f + 0.5f);
	if (ri < 0) ri = 0; if (ri > 255) ri = 255;
	if (gi < 0) gi = 0; if (gi > 255) gi = 255;
	if (bi < 0) bi = 0; if (bi > 255) bi = 255;

	char hex[16]{};
	snprintf(hex, sizeof(hex), "#%02X%02X%02X", ri, gi, bi);
	ImVec2 hts = ImGui::CalcTextSize(hex);
	float hx = b1.x - 10.f - hts.x;
	dl->AddText(ImVec2(hx, mid_y - hts.y * 0.5f), IM_COL32(170, 176, 188, (int)(200.f * e)), hex);

	if (show_brush)
	{
		// якорь для попапа = кисть
		ImGui::SetCursorScreenPos(ImVec2(brush_x, mid_y - brush_sz * 0.5f));
		ImGui::InvisibleButton("##cp_anchor", ImVec2(brush_sz, brush_sz));
		if (ng::colorpicker("##cp", col, true, 0, false, brush_hit))
		{
			ch = true;
		}
	}

	ImGui::PopClipRect();

	ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + total_h));
	ImGui::Dummy(ImVec2(bar_w, 0.01f));

	ImGui::PopID();
	return ch;
}