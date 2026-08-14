#include "island.h"
#include "../colors/colors.h"
#include "../animation/animation.h"
#include "../jewsploit_shell.h"
#include "app/Settings.h"

#include <stdio.h>
#include <imgui.h>

void ng::island()
{
	ImGuiIO& io = ImGui::GetIO();

	static float tab_t[3]{};

	const char* names[] = { "lua", "explorer", "players" };
	const int n = 3;
	bool* tab_on[3] = {
		&Cheat::g_Settings.lua.executor,
		&Cheat::g_Settings.misc.explorer,
		&Cheat::g_Settings.misc.players
	};

	float pad_x = 16.f;
	float pad_y = 8.f;
	float tab_gap = 22.f;
	float hit_pad = 8.f;
	float hit_h = 26.f;

	float widths[3]{};
	float row_w = 0.f;
	for (int i = 0; i < n; i++)
	{
		widths[i] = ImGui::CalcTextSize(names[i]).x;
		row_w += widths[i] + hit_pad * 2.f;
		if (i + 1 < n) row_w += tab_gap;
	}

	float box_w = row_w + pad_x * 2.f;
	float box_h = hit_h + pad_y * 2.f;
	float bx = (io.DisplaySize.x - box_w) * 0.5f;
	float by = 12.f;

	ImGui::SetNextWindowPos(ImVec2(bx, by), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(box_w, box_h), ImGuiCond_Always);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, box_h * 0.5f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(
		col::live().window[0],
		col::live().window[1],
		col::live().window[2],
		0.96f
	));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.10f));

	ImGui::Begin("##jewsploit_island", nullptr, flags);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 wp = ImGui::GetWindowPos();
	ImVec2 wsz = ImGui::GetWindowSize();
	float pill_r = wsz.y * 0.5f;

	dl->AddRect(
		wp,
		ImVec2(wp.x + wsz.x, wp.y + wsz.y),
		IM_COL32(255, 255, 255, 22),
		pill_r,
		0,
		1.f
	);

	float mid_y = wp.y + wsz.y * 0.5f;
	float x = wp.x + pad_x;

	for (int i = 0; i < n; i++)
	{
		float bw = widths[i] + hit_pad * 2.f;
		ImGui::SetCursorScreenPos(ImVec2(x, mid_y - hit_h * 0.5f));

		char id[24]{};
		snprintf(id, sizeof(id), "##isl%d", i);

		if (ImGui::InvisibleButton(id, ImVec2(bw, hit_h)))
		{
			*tab_on[i] = !*tab_on[i];
		}

		bool hov = ImGui::IsItemHovered();
		bool sel = *tab_on[i];

		float want = sel ? 1.f : 0.f;
		tab_t[i] = anim::approach(tab_t[i], want, 14.f, io.DeltaTime);
		float t = tab_t[i];

		int a = 120;
		if (hov) a = 190;
		a = (int)(a + (235 - a) * t);

		float lift = t * 1.5f;
		ImVec2 ts = ImGui::CalcTextSize(names[i]);
		float tx = x + (bw - ts.x) * 0.5f;
		float ty = mid_y - ts.y * 0.5f - lift;

		if (t > 0.01f)
		{
			ImU32 glow = col::accent_u32((int)(50.f * t));
			dl->AddText(ImVec2(tx + 0.6f, ty + 0.6f), glow, names[i]);
		}

		dl->AddText(ImVec2(tx, ty), IM_COL32(245, 248, 255, a), names[i]);

		x += bw + tab_gap;
	}

	ImGui::End();
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(3);

	// lua/explorer/players — свои float окна, island ток тумблеры
}