#include "child.h"
#include "scroll_fade.h"
#include "../colors/colors.h"

#include <stdio.h>
#include <string.h>

namespace
{
	float head_h = 34.f;

	struct stk_t
	{
		char title[48];
		float round;
		float w;
		float h;
	};

	stk_t g_stk[6]{};
	int g_n = 0;

	float snap(float v)
	{
		return (float)(int)(v + 0.5f);
	}

	ImU32 body_bg()
	{
		// opacity из темы как есть — не форсим 230
		return col::child_bg_u32();
	}

	ImU32 body_bg_solidish()
	{
		// хедер чуть плотнее боди, но без +40 в бетон
		col::theme_t& th = col::live();
		int a = (int)(th.child[3] * 255.f + 0.5f);
		if (a < 1) a = 1;
		if (a > 255) a = 255;
		int bump = a + (a / 6);
		if (bump > 255) bump = 255;
		return IM_COL32(
			(int)(th.child[0] * 255.f + 0.5f),
			(int)(th.child[1] * 255.f + 0.5f),
			(int)(th.child[2] * 255.f + 0.5f),
			bump
		);
	}

	void draw_text_center(ImDrawList* dl, ImVec2 box0, ImVec2 box1, const char* text, ImU32 col)
	{
		ImVec2 ts = ImGui::CalcTextSize(text);
		float x = snap(box0.x + (box1.x - box0.x - ts.x) * 0.5f);
		float y = snap(box0.y + (box1.y - box0.y - ts.y) * 0.5f);
		dl->AddText(ImVec2(x, y), col, text);
	}

	void paint_head(ImDrawList* dl, ImVec2 wp, float w, float round, const char* title)
	{
		ImU32 bg = body_bg_solidish();
		dl->AddRectFilled(
			wp,
			ImVec2(wp.x + w, wp.y + head_h),
			bg,
			round,
			ImDrawFlags_RoundCornersTop
		);

		draw_text_center(
			dl,
			wp,
			ImVec2(wp.x + w, wp.y + head_h),
			title,
			IM_COL32(230, 235, 245, 200)
		);

		float ly = snap(wp.y + head_h);
		dl->AddLine(
			ImVec2(snap(wp.x + 12.f), ly),
			ImVec2(snap(wp.x + w - 12.f), ly),
			IM_COL32(255, 255, 255, 18),
			1.f
		);
	}
}

float ng::child_head_h()
{
	return head_h;
}

bool ng::child_begin(const char* id, const char* title, float w, float h, float round, bool scroll)
{
	if (g_n >= 6)
	{
		return false;
	}

	if (w < 40.f) w = 40.f;
	if (h < 40.f) h = 40.f;
	if (round < 6.f) round = 6.f;

	stk_t& s = g_stk[g_n++];
	s.round = round;
	s.w = w;
	s.h = h;
	s.title[0] = 0;
	if (title)
	{
		snprintf(s.title, sizeof(s.title), "%s", title);
	}

	ImVec2 p0 = ImGui::GetCursorScreenPos();
	ImVec2 p1 = ImVec2(p0.x + w, p0.y + h);
	ImDrawList* dl = ImGui::GetWindowDrawList();

	dl->AddRectFilled(p0, p1, body_bg(), round);
	dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 12), round, 0, 1.f);
	paint_head(dl, p0, w, round, s.title);

	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, round);
	// паддинг на все строки а то после первой всё липнет к левому краю
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f, 12.f));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoBackground;
	if (!scroll)
	{
		flags |= ImGuiWindowFlags_NoScrollbar;
	}

	bool ok = ImGui::BeginChild(
		id,
		ImVec2(w, h),
		ImGuiChildFlags_AlwaysUseWindowPadding,
		flags
	);

	// контент ниже шапки / выше нижнего скругления
	ImVec2 wp = ImGui::GetWindowPos();
	float bot = round;
	if (bot < 8.f) bot = 8.f;
	ImGui::PushClipRect(
		ImVec2(wp.x + 1.f, wp.y + head_h + 1.f),
		ImVec2(wp.x + w - 1.f, wp.y + h - bot),
		true
	);

	ImGui::SetCursorPos(ImVec2(14.f, head_h + 12.f));
	return ok;
}

void ng::child_end()
{
	if (g_n <= 0)
	{
		ImGui::EndChild();
		return;
	}

	stk_t& s = g_stk[g_n - 1];
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 wp = ImGui::GetWindowPos();

	{
		ImU32 fade_bg = body_bg_solidish();
		ng::scroll_fade_child(fade_bg, 22.f);
	}

	ImGui::PopClipRect();

	// шапка сверху поверх строк
	paint_head(dl, wp, s.w, s.round, s.title);

	// низ скруглённый — imgui child иначе рисует квадратные уголки
	float r = s.round;
	ImU32 bg = body_bg();
	dl->AddRectFilled(
		ImVec2(wp.x, wp.y + s.h - r - 1.f),
		ImVec2(wp.x + s.w, wp.y + s.h),
		bg,
		r,
		ImDrawFlags_RoundCornersBottom
	);

	dl->AddRect(
		wp,
		ImVec2(wp.x + s.w, wp.y + s.h),
		IM_COL32(255, 255, 255, 12),
		s.round,
		0,
		1.f
	);

	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	g_n--;
}
