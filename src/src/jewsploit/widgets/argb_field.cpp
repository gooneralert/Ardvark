#include "argb_field.h"
#include "../colors/colors.h"

#include <stdio.h>
#include <imgui.h>

namespace
{
	void to_argb(float col[4], int* a, int* r, int* g, int* b)
	{
		*a = (int)(col[3] * 255.f + 0.5f);
		*r = (int)(col[0] * 255.f + 0.5f);
		*g = (int)(col[1] * 255.f + 0.5f);
		*b = (int)(col[2] * 255.f + 0.5f);
		if (*a < 0) *a = 0; if (*a > 255) *a = 255;
		if (*r < 0) *r = 0; if (*r > 255) *r = 255;
		if (*g < 0) *g = 0; if (*g > 255) *g = 255;
		if (*b < 0) *b = 0; if (*b > 255) *b = 255;
	}

	void fmt_argb(float col[4], char* out, int n)
	{
		int a, r, g, b;
		to_argb(col, &a, &r, &g, &b);
		snprintf(out, n, "%d, %d, %d, %d", a, r, g, b);
	}

	bool parse_argb(const char* s, float col[4])
	{
		int a = 255, r = 255, g = 255, b = 255;
		if (sscanf_s(s, "%d , %d , %d , %d", &a, &r, &g, &b) < 4)
		{
			if (sscanf_s(s, "%d,%d,%d,%d", &a, &r, &g, &b) < 4)
			{
				return false;
			}
		}

		if (a < 0) a = 0; if (a > 255) a = 255;
		if (r < 0) r = 0; if (r > 255) r = 255;
		if (g < 0) g = 0; if (g > 255) g = 255;
		if (b < 0) b = 0; if (b > 255) b = 255;

		col[0] = (float)r / 255.f;
		col[1] = (float)g / 255.f;
		col[2] = (float)b / 255.f;
		col[3] = (float)a / 255.f;
		return true;
	}
}

bool ng::argb_field(const char* id, float col[4], float w)
{
	if (!col)
	{
		return false;
	}

	ImGuiIO& io = ImGui::GetIO();
	ImGui::PushID(id);
	ImGuiStorage* st = ImGui::GetStateStorage();
	ImGuiID eid = ImGui::GetID("edit");
	ImGuiID cid = ImGui::GetID("copy_t");
	ImGuiID sid = ImGui::GetID("seed");

	int editing = st->GetInt(eid, 0);
	float copy_t = st->GetFloat(cid, -1.f);

	char shown[64]{};
	fmt_argb(col, shown, sizeof(shown));

	float pad_x = 8.f;
	float h = 22.f;
	float rnd = 8.f;
	ImVec2 pos = ImGui::GetCursorScreenPos();

	if (w <= 0.f)
	{
		float right = ImGui::GetWindowPos().x + ImGui::GetWindowSize().x - 12.f;
		w = right - pos.x;
	}

	if (w < 80.f) w = 80.f;

	ImDrawList* dl = ImGui::GetWindowDrawList();

	dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), col::nested_bg_u32(), rnd);
	dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), col::nested_br_u32(), rnd, 0, 1.f);

	bool ch = false;
	static char edit_buf[64]{};

	if (editing)
	{
		bool first = (st->GetInt(sid, 0) == 0);
		if (first)
		{
			snprintf(edit_buf, sizeof(edit_buf), "%s", shown);
			st->SetInt(sid, 1);
		}

		ImVec2 ts = ImGui::CalcTextSize(edit_buf);
		float pad_y = (h - ts.y) * 0.5f;
		if (pad_y < 0.f) pad_y = 0.f;

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(220.f / 255.f, 226.f / 255.f, 236.f / 255.f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad_x, pad_y));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

		ImGui::SetCursorScreenPos(pos);
		ImGui::PushItemWidth(w);
		if (first)
		{
			ImGui::SetKeyboardFocusHere();
		}

		bool enter = ImGui::InputText(
			"##e",
			edit_buf,
			sizeof(edit_buf),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_NoHorizontalScroll
		);

		ImGui::PopItemWidth();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(4);

		bool outside = io.MouseClicked[0] && !ImGui::IsItemHovered();
		if (enter || outside)
		{
			if (parse_argb(edit_buf, col))
			{
				ch = true;
			}

			st->SetInt(eid, 0);
			st->SetInt(sid, 0);
		}
	}

	else
	{
		ImGui::SetCursorScreenPos(pos);
		ImGui::InvisibleButton("##hit", ImVec2(w, h));

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
		{
			st->SetInt(eid, 1);
			st->SetInt(sid, 0);
			st->SetFloat(cid, -1.f);
		}

		else if (ImGui::IsItemClicked(0))
		{
			st->SetFloat(cid, (float)ImGui::GetTime() + 0.22f);
		}

		if (copy_t > 0.f && (float)ImGui::GetTime() >= copy_t && st->GetInt(eid, 0) == 0)
		{
			ImGui::SetClipboardText(shown);
			st->SetFloat(cid, -1.f);
		}

		ImVec2 ts = ImGui::CalcTextSize(shown);
		dl->AddText(
			ImVec2(pos.x + pad_x, pos.y + (h - ts.y) * 0.5f),
			IM_COL32(220, 226, 236, 230),
			shown
		);
	}

	ImGui::SetCursorScreenPos(ImVec2(pos.x + w, pos.y));
	ImGui::Dummy(ImVec2(0.f, h));
	ImGui::PopID();
	return ch;
}