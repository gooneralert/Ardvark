#include "float_panel.h"
#include "../animation/animation.h"
#include "../colors/colors.h"

#include <cfloat>
#include <cstdio>
#include <cstring>
#include <imgui.h>

namespace
{
	float g_head = 34.f;
	bool g_edge_prev = true;
	int g_style_n = 0;

	struct anim_t
	{
		char id[48];
		float vis;
		ImVec2 hold_sz;
		ImVec2 hold_pos;
		float min_w;
		float min_h;
		bool has_hold;
		bool used;
	};

	anim_t g_anims[8]{};

	float snap(float v)
	{
		return (float)(int)(v + 0.5f);
	}

	anim_t* slot_for(const char* id)
	{
		if (!id) id = "";
		for (int i = 0; i < 8; ++i)
		{
			if (g_anims[i].used && std::strcmp(g_anims[i].id, id) == 0)
				return &g_anims[i];
		}

		for (int i = 0; i < 8; ++i)
		{
			if (!g_anims[i].used)
			{
				g_anims[i].used = true;
				std::snprintf(g_anims[i].id, sizeof(g_anims[i].id), "%s", id);
				g_anims[i].vis = 0.f;
				g_anims[i].has_hold = false;
				return &g_anims[i];
			}
		}

		return &g_anims[0];
	}
}

float ng::float_panel_head_h()
{
	return g_head;
}

bool ng::float_panel_begin(
	const char* id,
	const char* title,
	bool* open,
	float def_w,
	float def_h,
	float min_w,
	float min_h
)
{
	ImGuiIO& io = ImGui::GetIO();
	col::theme_t& th = col::live();
	anim_t* pa = slot_for(id);
	pa->min_w = min_w;
	pa->min_h = min_h;

	bool want = true;
	if (open)
		want = *open;

	// как shell: открытие 13, закрытие быстрее 26
	float spd = want ? 13.f : 26.f;
	pa->vis = anim::approach(pa->vis, want ? 1.f : 0.f, spd, io.DeltaTime);
	float e = want ? anim::ease_out_cubic(pa->vis) : pa->vis;

	if (pa->vis < 0.001f)
	{
		g_style_n = 0;
		return false;
	}

	float s = 0.94f + 0.06f * e;
	bool live = want && pa->vis > 0.995f;

	ImGui::SetNextWindowSize(ImVec2(def_w, def_h), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(min_w, min_h), ImVec2(FLT_MAX, FLT_MAX));
	ImGui::SetNextWindowPos(
		ImVec2((io.DisplaySize.x - def_w) * 0.5f, (io.DisplaySize.y - def_h) * 0.35f),
		ImGuiCond_FirstUseEver
	);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoMove;

	if (!live)
		flags |= ImGuiWindowFlags_NoInputs;

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(th.window[0], th.window[1], th.window[2], th.window[3]));
	g_style_n = 4;

	g_edge_prev = io.ConfigWindowsResizeFromEdges;
	io.ConfigWindowsResizeFromEdges = live;

	bool ok = ImGui::Begin(id, nullptr, flags);
	if (!ok)
	{
		ImGui::End();
		ImGui::PopStyleColor(1);
		ImGui::PopStyleVar(g_style_n);
		g_style_n = 0;
		io.ConfigWindowsResizeFromEdges = g_edge_prev;
		return false;
	}

	bool appearing = ImGui::IsWindowAppearing();
	if (appearing && live)
		ImGui::SetWindowFocus();

	ImVec2 wp = ImGui::GetWindowPos();
	ImVec2 wsz = ImGui::GetWindowSize();
	float pr = ImGui::GetStyle().WindowRounding;
	ImDrawList* dl = ImGui::GetWindowDrawList();

	if (appearing || !pa->has_hold)
	{
		pa->hold_sz = wsz;
		pa->hold_pos = wp;
		pa->has_hold = true;
	}

	float cz = 16.f;
	float close_w = open ? (cz + 20.f) : 0.f;

	ImGuiStorage* st = ImGui::GetStateStorage();
	float* pcx = st->GetFloatRef(ImGui::GetID("##fp_cx"), wp.x);
	float* pcy = st->GetFloatRef(ImGui::GetID("##fp_cy"), wp.y);
	float* ptx = st->GetFloatRef(ImGui::GetID("##fp_tx"), wp.x);
	float* pty = st->GetFloatRef(ImGui::GetID("##fp_ty"), wp.y);
	int* hold = st->GetIntRef(ImGui::GetID("##fp_hold"), 0);

	ImVec2 cur(*pcx, *pcy);
	ImVec2 tgt(*ptx, *pty);

	if (appearing)
	{
		cur = tgt = wp;
		*hold = 0;
	}

	if (live)
	{
		ImGui::SetCursorScreenPos(wp);
		ImGui::InvisibleButton("##fp_drag", ImVec2(wsz.x - close_w, g_head));

		if (!io.MouseDown[0])
		{
			*hold = 0;
		}

		else if (ImGui::IsItemActive() && ImGui::IsItemActivated())
		{
			*hold = 1;
			tgt = cur = wp;
		}

		// idle — всегда к imgui pos (edge resize двигает окно)
		if (!*hold)
			cur = tgt = wp;

		if (*hold && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			tgt.x += io.MouseDelta.x;
			tgt.y += io.MouseDelta.y;
		}

		if (*hold)
		{
			cur = anim::approach(cur, tgt, 20.f, io.DeltaTime);
			ImGui::SetWindowPos(cur);
			wp = ImGui::GetWindowPos();
		}

		*pcx = cur.x;
		*pcy = cur.y;
		*ptx = tgt.x;
		*pty = tgt.y;

		pa->hold_sz = ImGui::GetWindowSize();
		pa->hold_pos = wp;
		wsz = pa->hold_sz;

		// угол ресайза — edge resize иногда не цепляется
		float grip = 18.f;
		ImGui::SetCursorScreenPos(ImVec2(wp.x + wsz.x - grip, wp.y + wsz.y - grip));
		ImGui::InvisibleButton("##fp_rs", ImVec2(grip, grip));
		bool rs_hov = ImGui::IsItemHovered();
		bool rs_act = ImGui::IsItemActive();
		if (rs_hov || rs_act)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

		if (rs_act)
		{
			ImVec2 ns(
				wsz.x + io.MouseDelta.x,
				wsz.y + io.MouseDelta.y
			);
			if (ns.x < pa->min_w) ns.x = pa->min_w;
			if (ns.y < pa->min_h) ns.y = pa->min_h;
			ImGui::SetWindowSize(ns);
			wsz = ns;
			pa->hold_sz = ns;
		}

		{
			ImU32 gc = rs_hov || rs_act
				? IM_COL32(255, 255, 255, 90)
				: IM_COL32(255, 255, 255, 40);
			float gx = wp.x + wsz.x - 5.f;
			float gy = wp.y + wsz.y - 5.f;
			dl->AddLine(ImVec2(gx - 10.f, gy), ImVec2(gx, gy - 10.f), gc, 1.2f);
			dl->AddLine(ImVec2(gx - 6.f, gy), ImVec2(gx, gy - 6.f), gc, 1.2f);
		}
	}

	else if (pa->has_hold)
	{
		// сжимание к центру + fade (как shell)
		ImVec2 sz(pa->hold_sz.x * s, pa->hold_sz.y * s);
		ImVec2 pos(
			pa->hold_pos.x + (pa->hold_sz.x - sz.x) * 0.5f,
			pa->hold_pos.y + (pa->hold_sz.y - sz.y) * 0.5f
		);
		ImGui::SetWindowSize(sz);
		ImGui::SetWindowPos(pos);
		wsz = sz;
		wp = pos;
		cur = tgt = pa->hold_pos;
		*pcx = cur.x;
		*pcy = cur.y;
		*ptx = tgt.x;
		*pty = tgt.y;
		*hold = 0;
	}

	ImVec2 ts = ImGui::CalcTextSize(title ? title : "");
	float tx = snap(wp.x + (wsz.x - ts.x) * 0.5f);
	float ty = snap(wp.y + (g_head - ts.y) * 0.5f);
	dl->AddText(ImVec2(tx, ty), IM_COL32(230, 235, 245, 200), title);

	if (open && live)
	{
		float cx = wp.x + wsz.x - 14.f - cz;
		float cy = wp.y + (g_head - cz) * 0.5f;
		ImGui::SetCursorScreenPos(ImVec2(cx, cy));
		bool close_hit = ImGui::InvisibleButton("##fp_close", ImVec2(cz, cz));
		bool hov = ImGui::IsItemHovered();
		if (!appearing && close_hit)
			*open = false;

		ImU32 xc = hov ? IM_COL32(255, 255, 255, 220) : IM_COL32(200, 205, 215, 160);
		float p = 4.f;
		dl->AddLine(ImVec2(cx + p, cy + p), ImVec2(cx + cz - p, cy + cz - p), xc, 1.4f);
		dl->AddLine(ImVec2(cx + cz - p, cy + p), ImVec2(cx + p, cy + cz - p), xc, 1.4f);
	}

	float ly = snap(wp.y + g_head);
	dl->AddLine(
		ImVec2(snap(wp.x + 12.f), ly),
		ImVec2(snap(wp.x + wsz.x - 12.f), ly),
		IM_COL32(255, 255, 255, 18),
		1.f
	);

	dl->AddRect(
		wp,
		ImVec2(wp.x + wsz.x, wp.y + wsz.y),
		IM_COL32(255, 255, 255, 12),
		pr,
		0,
		1.f
	);

	ImGui::SetCursorPos(ImVec2(14.f, g_head + 12.f));
	return true;
}

void ng::float_panel_end()
{
	ImGui::GetIO().ConfigWindowsResizeFromEdges = g_edge_prev;
	ImGui::End();
	ImGui::PopStyleColor(1);
	ImGui::PopStyleVar(g_style_n > 0 ? g_style_n : 3);
	g_style_n = 0;
}
