#include "search_popup.h"
#include "scroll_fade.h"
#include "colorpicker.h"
#include "../colors/colors.h"
#include "../animation/animation.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <imgui.h>

namespace
{
	bool match_name(const char* name, const char* q)
	{
		if (!name) return false;
		if (!q || !q[0]) return true;

		char a[128]{};
		char b[128]{};
		int na = 0;
		int nb = 0;

		for (int i = 0; name[i] && na < 127; i++)
		{
			a[na++] = (char)tolower((unsigned char)name[i]);
		}
		a[na] = 0;

		for (int i = 0; q[i] && nb < 127; i++)
		{
			b[nb++] = (char)tolower((unsigned char)q[i]);
		}
		b[nb] = 0;

		return strstr(a, b) != nullptr;
	}

	float glide(float cur, float tgt, float spd, float dt)
	{
		float d = tgt - cur;
		if (d > -0.15f && d < 0.15f) return tgt;
		float k = 1.f - expf(-spd * dt);
		if (k < 0.f) k = 0.f;
		if (k > 1.f) k = 1.f;
		return cur + d * k;
	}

	void draw_blur_dim(float e)
	{
		ImDrawList* bg = ImGui::GetBackgroundDrawList();
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 a = ImVec2(0.f, 0.f);
		ImVec2 b = io.DisplaySize;

		// затемнение + несколько слоёв под "frost"
		int dim = (int)(160.f * e);
		bg->AddRectFilled(a, b, IM_COL32(4, 6, 10, dim));

		int frost = (int)(28.f * e);
		bg->AddRectFilled(a, b, IM_COL32(180, 190, 210, frost));

		int mist = (int)(18.f * e);
		bg->AddRectFilled(
			ImVec2(0.f, 0.f),
			ImVec2(b.x, b.y * 0.45f),
			IM_COL32(12, 14, 20, mist)
		);
	}
}

void ng::search_popup(bool* open, const search_entry_t* items, int count, search_draw_fn draw_feature)
{
	if (!open)
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	static float vis = 0.f;
	static float anim_h = 120.f;
	static char query[64]{};
	static bool focus = false;
	static bool was_open = false;
	static int active = -1;

	if (*open && !was_open)
	{
		focus = true;
		query[0] = 0;
		active = -1;
	}

	if (!*open && was_open)
	{
		active = -1;
	}
	was_open = *open;

	vis = anim::approach(vis, *open ? 1.f : 0.f, *open ? 14.f : 22.f, io.DeltaTime);
	float e = *open ? anim::ease_out_cubic(vis) : vis;

	if (vis < 0.001f)
	{
		return;
	}

	draw_blur_dim(e);

	// клик-блокер под попапом
	{
		ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
		ImGuiWindowFlags df =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoBackground;

		ImGui::Begin("##search_dim", nullptr, df);
		ImGui::InvisibleButton("##dimhit", io.DisplaySize);
		bool dim_hit = ImGui::IsItemClicked();
		ImGui::End();

		// пустое место при открытом пикере — закрывает пикер, не весь search
		if (dim_hit && *open && !ng::colorpicker_any_open())
		{
			*open = false;
			active = -1;
		}
	}

	int matches[64]{};
	int match_n = 0;
	if (items && count > 0)
	{
		for (int i = 0; i < count && match_n < 64; i++)
		{
			if (match_name(items[i].name, query))
			{
				matches[match_n++] = i;
			}
		}
	}

	float pad = 14.f;
	float title_h = 40.f; // шапка edit/search, контент ниже
	float box_h = 30.f;
	float row_h = 28.f;
	float gap = 10.f;
	float pw = 340.f;

	float want_h = pad + title_h + box_h + gap + pad;
	if (active >= 0)
	{
		// фикс высота в edit — не дёргать пока крутишь picker
		want_h = 300.f;
	}

	else
	{
		int rows = match_n;
		if (rows < 1) rows = 1;
		if (rows > 8) rows = 8;
		want_h += (float)rows * row_h + 6.f;
	}

	if (want_h < 120.f) want_h = 120.f;
	if (want_h > 420.f) want_h = 420.f;

	if (active >= 0)
	{
		anim_h = glide(anim_h, want_h, 18.f, io.DeltaTime);
	}

	else
	{
		anim_h = glide(anim_h, want_h, 14.f, io.DeltaTime);
	}

	float s = 0.97f + 0.03f * e;
	ImVec2 sz = ImVec2(pw * s, anim_h * s);
	ImVec2 c = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.42f);
	ImVec2 pp = ImVec2(c.x - sz.x * 0.5f, c.y - sz.y * 0.5f);

	ImGui::SetNextWindowPos(pp, ImGuiCond_Always);
	ImGui::SetNextWindowSize(sz, ImGuiCond_Always);
	// фокус тока при открытии — иначе колорпикер каждый кадр теряет фокус
	if (focus)
	{
		ImGui::SetNextWindowFocus();
	}

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse;

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));

	ImVec4 wbg = ImVec4(
		col::live().window[0],
		col::live().window[1],
		col::live().window[2],
		col::live().window[3] > 0.92f ? col::live().window[3] : 0.94f
	);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, wbg);
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.08f));

	ImGui::Begin("##search_pop", nullptr, flags);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 wp = ImGui::GetWindowPos();
	ImVec2 wsz = ImGui::GetWindowSize();
	float box_w = wsz.x - pad * 2.f;

	// title / back
	{
		const char* ttl = active >= 0 ? "edit" : "search";
		ImVec2 ts = ImGui::CalcTextSize(ttl);
		dl->AddText(
			ImVec2(wp.x + (wsz.x - ts.x) * 0.5f, wp.y + 12.f),
			IM_COL32(230, 235, 245, 200),
			ttl
		);

		if (active >= 0)
		{
			ImGui::SetCursorScreenPos(ImVec2(wp.x + 12.f, wp.y + 10.f));
			if (ImGui::InvisibleButton("##back", ImVec2(28.f, 22.f)))
			{
				active = -1;
			}

			bool hov = ImGui::IsItemHovered();
			ImU32 bc = IM_COL32(245, 248, 255, hov ? 230 : 150);
			float bx = wp.x + 18.f;
			float by = wp.y + 20.f;
			dl->AddLine(ImVec2(bx + 6.f, by - 5.f), ImVec2(bx, by), bc, 1.6f);
			dl->AddLine(ImVec2(bx, by), ImVec2(bx + 6.f, by + 5.f), bc, 1.6f);
		}
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		if (active >= 0)
		{
			active = -1;
		}

		else
		{
			*open = false;
		}
	}

	float cur_y = title_h + 6.f;

	if (active < 0)
	{
		ImGui::SetCursorPos(ImVec2(pad, cur_y));
		ImVec2 ip = ImGui::GetCursorScreenPos();
		float rnd = 10.f;

		dl->AddRectFilled(ip, ImVec2(ip.x + box_w, ip.y + box_h), col::checkbox_off_u32(), rnd);
		dl->AddRect(ip, ImVec2(ip.x + box_w, ip.y + box_h), IM_COL32(255, 255, 255, 28), rnd, 0, 1.f);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(220.f / 255.f, 226.f / 255.f, 236.f / 255.f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, (box_h - ImGui::GetFontSize()) * 0.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
		ImGui::SetNextItemWidth(box_w);
		if (focus)
		{
			ImGui::SetKeyboardFocusHere();
			focus = false;
		}
		ImGui::InputText("##sq", query, sizeof(query));
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(4);

		cur_y += box_h + gap;
		float list_h = wsz.y - cur_y - pad;
		if (list_h < row_h) list_h = row_h;

		ImGui::SetCursorPos(ImVec2(pad, cur_y));
		ImGui::BeginChild(
			"##sres",
			ImVec2(box_w, list_h),
			false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
		);

		ImDrawList* bdl = ImGui::GetWindowDrawList();

		if (match_n == 0)
		{
			const char* empty = "nothing found";
			ImVec2 ts = ImGui::CalcTextSize(empty);
			ImVec2 wp2 = ImGui::GetWindowPos();
			ImVec2 wz = ImGui::GetWindowSize();
			bdl->AddText(
				ImVec2(wp2.x + (wz.x - ts.x) * 0.5f, wp2.y + 10.f),
				IM_COL32(160, 165, 180, 160),
				empty
			);
		}

		else
		{
			for (int m = 0; m < match_n; m++)
			{
				int i = matches[m];
				ImVec2 rp = ImGui::GetCursorScreenPos();
				char id[24]{};
				snprintf(id, sizeof(id), "r%d", i);
				ImGui::InvisibleButton(id, ImVec2(box_w - 2.f, row_h));
				bool hit = ImGui::IsItemClicked();
				bool hov = ImGui::IsItemHovered();

				if (hov || hit)
				{
					bdl->AddRectFilled(
						ImVec2(rp.x, rp.y + 1.f),
						ImVec2(rp.x + box_w - 2.f, rp.y + row_h - 1.f),
						hit ? IM_COL32(210, 215, 230, 28) : IM_COL32(255, 255, 255, 10),
						6.f
					);
				}

				ImVec2 ts = ImGui::CalcTextSize(items[i].name);
				bdl->AddText(
					ImVec2(rp.x + 10.f, rp.y + (row_h - ts.y) * 0.5f),
					IM_COL32(220, 226, 236, 230),
					items[i].name
				);

				if (hit)
				{
					active = items[i].id;
				}
			}
		}

		{
			int ra = (int)(col::live().window[0] * 255.f + 0.5f);
			int ga = (int)(col::live().window[1] * 255.f + 0.5f);
			int ba = (int)(col::live().window[2] * 255.f + 0.5f);
			ng::scroll_fade_child(IM_COL32(ra, ga, ba, 230), 16.f);
		}

		ImGui::EndChild();
	}

	else
	{
		// имя фичи
		const char* nm = "feature";
		for (int i = 0; i < count; i++)
		{
			if (items[i].id == active)
			{
				nm = items[i].name;
				break;
			}
		}

		ImGui::SetCursorPos(ImVec2(pad, cur_y));
		ImGui::TextUnformatted(nm);
		cur_y = ImGui::GetCursorPosY() + 10.f;

		float body_h = wsz.y - cur_y - pad;
		if (body_h < 40.f) body_h = 40.f;

		ImGui::SetCursorPos(ImVec2(pad, cur_y));
		ImGui::BeginChild(
			"##sedit",
			ImVec2(box_w, body_h),
			false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
		);

		ImGui::Dummy(ImVec2(0.f, 4.f));
		ImGui::SetCursorPosX(0.f);
		if (draw_feature)
		{
			draw_feature(active);
		}

		ImGui::EndChild();
	}

	ImGui::End();
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(4);
}
