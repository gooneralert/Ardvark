#include "slider.h"
#include "spacing.h"
#include "../colors/colors.h"
#include "../animation/animation.h"

#include <stdio.h>
#include <math.h>
#include <imgui.h>

namespace
{
	ImU32 with_a(ImU32 c, float a01)
	{
		if (a01 < 0.f) a01 = 0.f;
		if (a01 > 1.f) a01 = 1.f;
		ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
		v.w *= a01;
		return ImGui::ColorConvertFloat4ToU32(v);
	}

	// без грубого snap как в approach — слайдеру он палит дёрганье
	float glide(float cur, float tgt, float spd, float dt)
	{
		float d = tgt - cur;
		if (d > -0.0005f && d < 0.0005f)
		{
			return tgt;
		}

		float k = 1.f - expf(-spd * dt);
		if (k < 0.f) k = 0.f;
		if (k > 1.f) k = 1.f;
		return cur + d * k;
	}

	void draw_tip(ImDrawList* dl, float gx, float gy, float grab_r, float a, float val)
	{
		if (a < 0.01f)
		{
			return;
		}

		char buf[32]{};
		snprintf(buf, sizeof(buf), "%.1f", val);
		ImVec2 ts = ImGui::CalcTextSize(buf);

		float pad_x = 8.f;
		float pad_y = 4.f;
		float bw = ts.x + pad_x * 2.f;
		float bh = ts.y + pad_y * 2.f;
		float gap = 10.f;

		// scale pop
		float s = 0.85f + 0.15f * a;
		bw *= s;
		bh *= s;

		float bx = gx - bw * 0.5f;
		float by = gy - grab_r - gap - bh;
		float br = 7.f;

		ImU32 bg = with_a(IM_COL32(22, 24, 30, 245), a);
		ImU32 tx = with_a(IM_COL32(240, 244, 250, 255), a);

		dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), bg, br);

		float ax = gx;
		float ay = by + bh;
		dl->AddTriangleFilled(
			ImVec2(ax - 5.f * a, ay),
			ImVec2(ax + 5.f * a, ay),
			ImVec2(ax, ay + 5.f * a),
			bg
		);

		float txp = bx + (bw - ts.x * s) * 0.5f;
		float typ = by + (bh - ts.y * s) * 0.5f;
		if (s < 0.999f)
		{
			dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * s, ImVec2(txp, typ), tx, buf);
		}

		else
		{
			dl->AddText(ImVec2(bx + pad_x, by + pad_y), tx, buf);
		}
	}
}

bool ng::slider(const char* id, float* v, float mn, float mx, bool shown)
{
	ImGuiIO& io = ImGui::GetIO();
	ImGuiStorage* st = ImGui::GetStateStorage();

	ImGui::PushID(id);
	ImGuiID oid = ImGui::GetID("open");
	ImGuiID tid = ImGui::GetID("t");
	ImGuiID tip_id = ImGui::GetID("tip");

	float open = st->GetFloat(oid, shown ? 1.f : 0.f);
	open = anim::approach(open, shown ? 1.f : 0.f, 7.f, io.DeltaTime);
	st->SetFloat(oid, open);

	float e = anim::ease_out_cubic(open);

	if (e < 0.001f)
	{
		st->SetFloat(tip_id, 0.f);
		ImGui::PopID();
		return false;
	}

	// толстый pill, сверху общий item_gap
	float track_h = 16.f;
	float grab_r = 9.f;
	float row_h = 24.f;
	float top = item_gap * e;
	float h = row_h * e;
	float pad_y = top + (h - track_h) * 0.5f;

	float pad_r = 12.f;
	float avail = ImGui::GetContentRegionAvail().x;
	float track_w = avail - pad_r;
	if (track_w < 40.f)
	{
		track_w = 40.f;
	}

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImDrawList* fdl = ImGui::GetForegroundDrawList();

	float sx = pos.x;
	float sy = pos.y + pad_y;

	ImGui::InvisibleButton("hit", ImVec2(track_w, top + h));
	bool active = ImGui::IsItemActive();
	bool ch = false;

	float inner = track_w - grab_r * 2.f;
	if (inner < 1.f)
	{
		inner = 1.f;
	}

	if (e > 0.55f && active)
	{
		float nt = (io.MousePos.x - sx - grab_r) / inner;
		if (nt < 0.f) nt = 0.f;
		if (nt > 1.f) nt = 1.f;
		float nv = mn + (mx - mn) * nt;
		if (nv != *v)
		{
			*v = nv;
			ch = true;
		}
	}

	float want_t = 0.f;
	if (mx > mn)
	{
		want_t = (*v - mn) / (mx - mn);
	}
	if (want_t < 0.f) want_t = 0.f;
	if (want_t > 1.f) want_t = 1.f;

	// за мышкой чуть быстрее, отпустил — мягко доезжает
	float spd = active ? 28.f : 14.f;
	float t = st->GetFloat(tid, want_t);
	t = glide(t, want_t, spd, io.DeltaTime);
	st->SetFloat(tid, t);

	float tip = st->GetFloat(tip_id, 0.f);
	tip = glide(tip, active ? 1.f : 0.f, 16.f, io.DeltaTime);
	st->SetFloat(tip_id, tip);
	float tip_e = anim::ease_out_cubic(tip) * e;
	float tip_v = mn + (mx - mn) * t;

	ImU32 track = with_a(col::slider_track_u32(), e);
	ImU32 fill = with_a(col::slider_fill_u32(), e);
	ImU32 br = with_a(IM_COL32(255, 255, 255, 22), e);
	ImU32 grab = with_a(IM_COL32(245, 248, 255, 255), e);
	ImU32 grab_br = with_a(col::slider_fill_u32(), e);

	float rnd = track_h * 0.5f;
	dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + track_w, sy + track_h), track, rnd);
	dl->AddRect(ImVec2(sx, sy), ImVec2(sx + track_w, sy + track_h), br, rnd, 0, 1.f);

	float gx = sx + grab_r + inner * t;
	float gy = sy + track_h * 0.5f;

	float fw = gx - sx;
	if (fw > rnd)
	{
		dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + fw, sy + track_h), fill, rnd);
	}

	else if (fw > 0.5f)
	{
		dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + fw, sy + track_h), fill, fw * 0.5f);
	}

	float gr = grab_r * (0.7f + 0.3f * e);
	dl->AddCircleFilled(ImVec2(gx, gy), gr, grab);
	dl->AddCircle(ImVec2(gx, gy), gr, grab_br, 0, 2.f);

	draw_tip(fdl, gx, gy, gr, tip_e, tip_v);

	ImGui::PopID();
	return ch;
}