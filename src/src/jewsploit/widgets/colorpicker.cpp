#include "colorpicker.h"
#include "argb_field.h"
#include "../colors/colors.h"
#include "../animation/animation.h"

#include <math.h>
#include <stdio.h>
#include <imgui.h>

namespace
{
	void rgb_to_hsv(float r, float g, float b, float* h, float* s, float* v)
	{
		float mx = r;
		if (g > mx) mx = g;
		if (b > mx) mx = b;
		float mn = r;
		if (g < mn) mn = g;
		if (b < mn) mn = b;
		float d = mx - mn;

		*v = mx;
		*s = (mx <= 0.0001f) ? 0.f : d / mx;

		if (d <= 0.0001f)
		{
			*h = 0.f;
			return;
		}

		if (mx == r)
		{
			*h = (g - b) / d;
			if (g < b) *h += 6.f;
		}

		else if (mx == g)
		{
			*h = (b - r) / d + 2.f;
		}

		else
		{
			*h = (r - g) / d + 4.f;
		}

		*h /= 6.f;
	}

	void hsv_to_rgb(float h, float s, float v, float* r, float* g, float* b)
	{
		if (s <= 0.0001f)
		{
			*r = *g = *b = v;
			return;
		}

		h = h - floorf(h);
		float ht = h * 6.f;
		int i = (int)ht;
		float f = ht - (float)i;
		float p = v * (1.f - s);
		float q = v * (1.f - s * f);
		float t = v * (1.f - s * (1.f - f));

		if (i == 0) { *r = v; *g = t; *b = p; }

		else if (i == 1) { *r = q; *g = v; *b = p; }

		else if (i == 2) { *r = p; *g = v; *b = t; }

		else if (i == 3) { *r = p; *g = q; *b = v; }

		else if (i == 4) { *r = t; *g = p; *b = v; }

		else { *r = v; *g = p; *b = q; }
	}

	ImU32 with_a(ImU32 c, float a01)
	{
		if (a01 < 0.f) a01 = 0.f;
		if (a01 > 1.f) a01 = 1.f;
		ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
		v.w *= a01;
		return ImGui::ColorConvertFloat4ToU32(v);
	}

	ImU32 col4(float r, float g, float b, float a)
	{
		return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
	}

	void draw_round_swatch(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 fill, ImU32 br, float rnd)
	{
		dl->AddRectFilled(a, b, IM_COL32(40, 42, 48, 255), rnd);
		dl->AddRectFilled(a, b, fill, rnd);
		dl->AddRect(a, b, br, rnd, 0, 1.f);
	}

	void mask_round_corners(ImDrawList* dl, ImVec2 a, ImVec2 b, float r, ImU32 bg)
	{
		if (r < 0.5f)
		{
			return;
		}

		dl->PathClear();
		dl->PathLineTo(a);
		dl->PathArcTo(ImVec2(a.x + r, a.y + r), r, 3.14159265f, 3.14159265f * 1.5f);
		dl->PathFillConvex(bg);

		dl->PathClear();
		dl->PathLineTo(ImVec2(b.x, a.y));
		dl->PathArcTo(ImVec2(b.x - r, a.y + r), r, -3.14159265f * 0.5f, 0.f);
		dl->PathFillConvex(bg);

		dl->PathClear();
		dl->PathLineTo(b);
		dl->PathArcTo(ImVec2(b.x - r, b.y - r), r, 0.f, 3.14159265f * 0.5f);
		dl->PathFillConvex(bg);

		dl->PathClear();
		dl->PathLineTo(ImVec2(a.x, b.y));
		dl->PathArcTo(ImVec2(a.x + r, b.y - r), r, 3.14159265f * 0.5f, 3.14159265f);
		dl->PathFillConvex(bg);
	}

	int g_cp_frame = -1;
	bool g_cp_open = false;
	bool g_cp_open_prev = false;

	void cp_frame_sync()
	{
		int f = ImGui::GetFrameCount();
		if (g_cp_frame != f)
		{
			g_cp_open_prev = g_cp_open;
			g_cp_open = false;
			g_cp_frame = f;
		}
	}
}

bool ng::colorpicker_any_open()
{
	cp_frame_sync();
	return g_cp_open_prev;
}

bool ng::colorpicker(const char* id, float col[4], bool shown, int slot, bool show_swatch, bool open_now, bool show_alpha)
{
	if (!col)
	{
		return false;
	}

	if (!show_alpha)
	{
		col[3] = 1.f;
	}

	if (slot < 0) slot = 0;

	ImGuiIO& io = ImGui::GetIO();
	ImGuiStorage* st = ImGui::GetStateStorage();

	// якорь строки чекбокса — второй свотч иначе цепляется за dummy
	ImGuiID mid_id = ImGui::GetID("##cp_row_mid");
	ImGuiID r_id = ImGui::GetID("##cp_row_r");
	ImGuiID y0_id = ImGui::GetID("##cp_row_y0");
	ImGuiID y1_id = ImGui::GetID("##cp_row_y1");

	ImVec2 row0 = ImGui::GetItemRectMin();
	ImVec2 row1 = ImGui::GetItemRectMax();
	float ih = row1.y - row0.y;
	float mid_y = (row0.y + row1.y) * 0.5f;
	float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

	if (ih > 4.f)
	{
		st->SetFloat(mid_id, mid_y);
		st->SetFloat(r_id, right);
		st->SetFloat(y0_id, row0.y);
		st->SetFloat(y1_id, row1.y);
	}

	else
	{
		mid_y = st->GetFloat(mid_id, mid_y);
		right = st->GetFloat(r_id, right);
		row0.y = st->GetFloat(y0_id, row0.y);
		row1.y = st->GetFloat(y1_id, row1.y);
	}

	ImGui::PushID(id);

	ImGuiID oid = ImGui::GetID("open");
	ImGuiID pid = ImGui::GetID("pop");

	float open = st->GetFloat(oid, shown ? 1.f : 0.f);
	open = anim::approach(open, shown ? 1.f : 0.f, 7.f, io.DeltaTime);
	st->SetFloat(oid, open);
	float e = anim::ease_out_cubic(open);

	int pop = st->GetInt(pid, 0);
	if (open_now)
	{
		pop = pop ? 0 : 1;
		st->SetInt(pid, pop);
		st->SetInt(ImGui::GetID("foc"), 1);
	}

	if (!shown)
	{
		pop = 0;
		st->SetInt(pid, 0);
	}

	if (e < 0.001f)
	{
		ImGui::PopID();
		return false;
	}

	float pad_r = 12.f;
	float sw = 22.f;
	float sh = 22.f;
	float rnd = 8.f;
	float gap_s = 6.f;

	float sx = right - pad_r - sw - (float)slot * (sw + gap_s) + (1.f - e) * 14.f;
	float sy = mid_y - sh * 0.5f;

	// без свотча якорим попап к кисти / прошлому item
	if (!show_swatch)
	{
		sx = row0.x;
		sy = row0.y;
		sw = row1.x - row0.x;
		sh = row1.y - row0.y;
		if (sw < 8.f) sw = 22.f;
		if (sh < 8.f) sh = 22.f;
	}

	ImDrawList* dl = ImGui::GetWindowDrawList();
	bool ch = false;
	bool hov = false;

	if (show_swatch)
	{
		ImGui::SetCursorScreenPos(ImVec2(sx, sy));
		ImGui::InvisibleButton("sw", ImVec2(sw, sh));
		bool hit = ImGui::IsItemClicked();
		hov = ImGui::IsItemHovered();

		if (hit && e > 0.55f)
		{
			pop = pop ? 0 : 1;
			st->SetInt(pid, pop);
		}

		ImU32 fill = col4(col[0], col[1], col[2], col[3] * e);
		ImU32 br = with_a(IM_COL32(255, 255, 255, hov || pop ? 48 : 28), e);
		draw_round_swatch(dl, ImVec2(sx, sy), ImVec2(sx + sw, sy + sh), fill, br, rnd);
	}

	if (pop)
	{
		cp_frame_sync();
		g_cp_open = true;

		float pad = 12.f;
		float sq = 160.f;
		float hue_w = 16.f;
		float gap = 8.f;
		float sq_r = 12.f;
		float pw = pad + sq + gap + hue_w + pad;
		float ph = pad + sq + 12.f + 22.f + pad;
		if (show_alpha)
		{
			ph = pad + sq + 12.f + 14.f + 12.f + 22.f + pad;
		}

		ImVec2 pp = ImVec2(sx + sw - pw, sy + sh + 8.f);
		if (pp.x < 8.f) pp.x = 8.f;
		if (pp.y + ph > io.DisplaySize.y - 8.f) pp.y = sy - ph - 8.f;

		ImGui::SetNextWindowPos(pp, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(pw, ph), ImGuiCond_Always);

		ImGuiID foc_id = ImGui::GetID("foc");
		if (st->GetInt(foc_id, 1))
		{
			ImGui::SetNextWindowFocus();
			st->SetInt(foc_id, 0);
		}

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoCollapse;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(8.f / 255.f, 10.f / 255.f, 14.f / 255.f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.08f));

		char wid[64]{};
		snprintf(wid, sizeof(wid), "##cpop_%s", id);
		ImGui::Begin(wid, nullptr, flags);

		ImDrawList* pdl = ImGui::GetWindowDrawList();
		ImVec2 wp = ImGui::GetWindowPos();

		float hh = 0.f, ss = 0.f, vv = 0.f;
		rgb_to_hsv(col[0], col[1], col[2], &hh, &ss, &vv);

		ImVec2 sq0 = ImVec2(wp.x + pad, wp.y + pad);
		ImVec2 sq1 = ImVec2(sq0.x + sq, sq0.y + sq);

		float hr, hg, hb;
		hsv_to_rgb(hh, 1.f, 1.f, &hr, &hg, &hb);
		ImU32 hue_col = col4(hr, hg, hb, 1.f);

		pdl->PushClipRect(sq0, sq1, true);
		pdl->AddRectFilledMultiColor(sq0, sq1, IM_COL32(255, 255, 255, 255), hue_col, hue_col, IM_COL32(255, 255, 255, 255));
		pdl->AddRectFilledMultiColor(sq0, sq1, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 255), IM_COL32(0, 0, 0, 255));
		pdl->PopClipRect();
		mask_round_corners(pdl, sq0, sq1, sq_r, IM_COL32(8, 10, 14, 255));
		pdl->AddRect(sq0, sq1, IM_COL32(255, 255, 255, 28), sq_r, 0, 1.f);

		ImGui::SetCursorScreenPos(sq0);
		ImGui::InvisibleButton("sv", ImVec2(sq, sq));
		if (ImGui::IsItemActive())
		{
			ss = (io.MousePos.x - sq0.x) / sq;
			vv = 1.f - (io.MousePos.y - sq0.y) / sq;
			if (ss < 0.f) ss = 0.f;
			if (ss > 1.f) ss = 1.f;
			if (vv < 0.f) vv = 0.f;
			if (vv > 1.f) vv = 1.f;
			hsv_to_rgb(hh, ss, vv, &col[0], &col[1], &col[2]);
			ch = true;
		}

		float cx = sq0.x + ss * sq;
		float cy = sq0.y + (1.f - vv) * sq;
		pdl->AddCircle(ImVec2(cx, cy), 5.f, IM_COL32(255, 255, 255, 230), 0, 1.6f);
		pdl->AddCircle(ImVec2(cx, cy), 3.5f, IM_COL32(0, 0, 0, 160), 0, 1.2f);

		ImVec2 h0 = ImVec2(sq1.x + gap, sq0.y);
		ImVec2 h1 = ImVec2(h0.x + hue_w, sq1.y);
		pdl->PushClipRect(h0, h1, true);
		for (int i = 0; i < 6; i++)
		{
			float t0 = (float)i / 6.f;
			float t1 = (float)(i + 1) / 6.f;
			float r0, g0, b0, r1, g1, b1;
			hsv_to_rgb(t0, 1.f, 1.f, &r0, &g0, &b0);
			hsv_to_rgb(t1, 1.f, 1.f, &r1, &g1, &b1);
			float y0 = h0.y + (h1.y - h0.y) * t0;
			float y1 = h0.y + (h1.y - h0.y) * t1;
			pdl->AddRectFilledMultiColor(
				ImVec2(h0.x, y0), ImVec2(h1.x, y1),
				col4(r0, g0, b0, 1.f), col4(r0, g0, b0, 1.f),
				col4(r1, g1, b1, 1.f), col4(r1, g1, b1, 1.f)
			);
		}
		pdl->PopClipRect();
		mask_round_corners(pdl, h0, h1, 8.f, IM_COL32(8, 10, 14, 255));
		pdl->AddRect(h0, h1, IM_COL32(255, 255, 255, 28), 8.f, 0, 1.f);

		ImGui::SetCursorScreenPos(h0);
		ImGui::InvisibleButton("hue", ImVec2(hue_w, sq));
		if (ImGui::IsItemActive())
		{
			hh = (io.MousePos.y - h0.y) / sq;
			if (hh < 0.f) hh = 0.f;
			if (hh > 1.f) hh = 1.f;
			hsv_to_rgb(hh, ss, vv, &col[0], &col[1], &col[2]);
			ch = true;
		}

		float hy = h0.y + hh * sq;
		pdl->AddRectFilled(ImVec2(h0.x - 2.f, hy - 2.f), ImVec2(h1.x + 2.f, hy + 2.f), IM_COL32(255, 255, 255, 230), 3.f);

		ImVec2 a1 = ImVec2(sq1.x + gap + hue_w, sq1.y);
		float py = sq1.y + 12.f;

		if (show_alpha)
		{
			float ay = sq1.y + 12.f;
			ImVec2 a0 = ImVec2(sq0.x, ay);
			a1 = ImVec2(sq1.x + gap + hue_w, ay + 14.f);
			pdl->AddRectFilled(a0, a1, col::checkbox_off_u32(), 7.f);
			pdl->AddRectFilled(a0, ImVec2(a0.x + (a1.x - a0.x) * col[3], a1.y), col4(col[0], col[1], col[2], 1.f), 7.f);
			pdl->AddRect(a0, a1, IM_COL32(255, 255, 255, 28), 7.f, 0, 1.f);

			ImGui::SetCursorScreenPos(a0);
			ImGui::InvisibleButton("alpha", ImVec2(a1.x - a0.x, a1.y - a0.y));
			if (ImGui::IsItemActive())
			{
				col[3] = (io.MousePos.x - a0.x) / (a1.x - a0.x);
				if (col[3] < 0.f) col[3] = 0.f;
				if (col[3] > 1.f) col[3] = 1.f;
				ch = true;
			}

			float ax = a0.x + col[3] * (a1.x - a0.x);
			pdl->AddCircleFilled(ImVec2(ax, (a0.y + a1.y) * 0.5f), 6.f, IM_COL32(245, 248, 255, 255));
			pdl->AddCircle(ImVec2(ax, (a0.y + a1.y) * 0.5f), 6.f, col4(col[0], col[1], col[2], 1.f), 0, 2.f);
			py = a1.y + 12.f;
		}

		else
		{
			col[3] = 1.f;
		}

		// preview + argb micro child
		float prev_w = 36.f;
		float prev_h = 22.f;
		draw_round_swatch(
			pdl,
			ImVec2(sq0.x, py),
			ImVec2(sq0.x + prev_w, py + prev_h),
			col4(col[0], col[1], col[2], col[3]),
			IM_COL32(255, 255, 255, 28),
			8.f
		);

		float field_x = sq0.x + prev_w + 8.f;
		float field_w = a1.x - field_x;
		ImGui::SetCursorScreenPos(ImVec2(field_x, py));
		if (ng::argb_field("##argb", col, field_w))
		{
			ch = true;
		}

		if (!show_alpha)
		{
			col[3] = 1.f;
		}

		// закрытие по пустому месту — даже если dim/search словили клик
		bool hover_win = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		bool hold_inside = ImGui::IsMouseDown(0) && hover_win;
		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(3);

		// open_now = клик по кисти, не закрывать в том же кадре
		if (io.MouseClicked[0] && !hover_win && !hov && !hold_inside && !open_now)
		{
			pop = 0;
			st->SetInt(pid, 0);
			st->SetInt(ImGui::GetID("foc"), 1);
		}
	}

	else
	{
		st->SetInt(ImGui::GetID("foc"), 1);
	}

	if (show_swatch)
	{
		ImGui::SetCursorScreenPos(ImVec2(row0.x, row1.y));
		ImGui::Dummy(ImVec2(0.f, 0.01f));
	}

	ImGui::PopID();
	return ch;
}