#include "strlist.h"
#include "child.h"
#include "scroll_fade.h"
#include "../colors/colors.h"

#include <stdio.h>
#include <imgui.h>

namespace
{
	float row_h = 26.f;
	float top_pad = 8.f;
	float bot_pad = 8.f;
	float side_pad = 12.f;
	float rnd = 10.f;

	float snap(float v)
	{
		return (float)(int)(v + 0.5f);
	}

	void draw_head(ImDrawList* dl, ImVec2 p0, float w, const char* title)
	{
		float head = ng::child_head_h();

		dl->AddRectFilled(
			p0,
			ImVec2(p0.x + w, p0.y + head),
			col::nested_bg_u32(),
			rnd,
			ImDrawFlags_RoundCornersTop
		);

		ImVec2 ts = ImGui::CalcTextSize(title);
		float tx = snap(p0.x + (w - ts.x) * 0.5f);
		float ty = snap(p0.y + (head - ts.y) * 0.5f);
		dl->AddText(ImVec2(tx, ty), IM_COL32(230, 235, 245, 200), title);

		float ly = snap(p0.y + head);
		dl->AddLine(
			ImVec2(snap(p0.x + side_pad), ly),
			ImVec2(snap(p0.x + w - side_pad), ly),
			IM_COL32(255, 255, 255, 10),
			1.f
		);
	}
}

float ng::strlist_h(int max_vis)
{
	if (max_vis < 1)
	{
		max_vis = 1;
	}

	return child_head_h() + top_pad + (float)max_vis * row_h + bot_pad;
}

bool ng::strlist(
	const char* id,
	const char* title,
	char items[][128],
	int count,
	int* sel,
	float w,
	int max_vis
)
{
	if (!id || !sel || w < 40.f)
	{
		return false;
	}

	if (max_vis < 1)
	{
		max_vis = 1;
	}

	if (*sel < -1) *sel = -1;
	if (count <= 0)
	{
		*sel = -1;
	}

	else if (*sel >= count)
	{
		*sel = count - 1;
	}

	float head = child_head_h();
	float h = strlist_h(max_vis);
	float body_h = h - head;

	ImVec2 p0 = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImGui::Dummy(ImVec2(w, h));

	dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + h), col::nested_bg_u32(), rnd);
	dl->AddRect(p0, ImVec2(p0.x + w, p0.y + h), col::nested_br_u32(), rnd, 0, 1.f);

	const char* ttl = title ? title : "list";

	char cid[64]{};
	snprintf(cid, sizeof(cid), "##slbody_%s", id);

	// тело строго под шапкой — скролл клипается окном
	ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + head));
	ImGui::BeginChild(
		cid,
		ImVec2(w, body_h),
		false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
	);

	ImGui::SetCursorPos(ImVec2(side_pad, top_pad));

	bool ch = false;
	float avail = ImGui::GetContentRegionAvail().x;
	float row_w = avail - side_pad;
	if (row_w < 20.f)
	{
		row_w = 20.f;
	}

	ImDrawList* bdl = ImGui::GetWindowDrawList();

	for (int i = 0; i < count; i++)
	{
		ImGui::SetCursorPosX(side_pad);
		ImVec2 p = ImGui::GetCursorScreenPos();

		char rid[24]{};
		snprintf(rid, sizeof(rid), "r%d", i);
		ImGui::InvisibleButton(rid, ImVec2(row_w, row_h));
		bool hit = ImGui::IsItemClicked();
		bool hov = ImGui::IsItemHovered();

		if (hit)
		{
			*sel = i;
			ch = true;
		}

		if (*sel == i || hov)
		{
			ImU32 bg = (*sel == i) ? IM_COL32(210, 215, 230, 22) : IM_COL32(255, 255, 255, 8);
			bdl->AddRectFilled(
				ImVec2(p.x, p.y + 1.f),
				ImVec2(p.x + row_w, p.y + row_h - 1.f),
				bg,
				6.f
			);
		}

		const char* txt = (items && items[i][0]) ? items[i] : "(empty)";
		ImVec2 ts = ImGui::CalcTextSize(txt);
		bdl->PushClipRect(ImVec2(p.x, p.y), ImVec2(p.x + row_w, p.y + row_h), true);
		bdl->AddText(
			ImVec2(p.x + (row_w - ts.x) * 0.5f, p.y + (row_h - ts.y) * 0.5f),
			IM_COL32(220, 226, 236, 230),
			txt
		);
		bdl->PopClipRect();
	}

	ImGui::Dummy(ImVec2(0.f, bot_pad));

	// края списка тухнут когда есть куда скроллить
	{
		int ra = (int)(col::live().child[0] * 255.f + 0.5f);
		int ga = (int)(col::live().child[1] * 255.f + 0.5f);
		int ba = (int)(col::live().child[2] * 255.f + 0.5f);
		ng::scroll_fade_child(IM_COL32(ra, ga, ba, 230), 22.f);
	}

	ImGui::EndChild();

	// шапка сверху после контента — всегда поверх
	draw_head(dl, p0, w, ttl);

	ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + h));
	ImGui::Dummy(ImVec2(0.f, 0.01f));

	return ch;
}