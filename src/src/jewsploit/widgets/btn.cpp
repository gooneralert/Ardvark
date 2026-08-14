#include "btn.h"
#include "../colors/colors.h"

#include <imgui.h>

bool ng::btn(const char* id, const char* label, float w, float h)
{
	if (!label)
	{
		return false;
	}

	ImGui::PushID(id);

	float pad_x = 12.f;
	ImVec2 ts = ImGui::CalcTextSize(label);
	if (w <= 0.f)
	{
		w = ts.x + pad_x * 2.f;
	}

	if (h <= 0.f)
	{
		h = 30.f;
	}

	float rnd = 10.f;
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImGui::InvisibleButton("##b", ImVec2(w, h));
	bool hit = ImGui::IsItemClicked();
	bool hov = ImGui::IsItemHovered();

	ImU32 bg = col::checkbox_off_u32();
	if (hov)
	{
		bg = IM_COL32(36, 40, 50, 220);
	}

	dl->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + w, pos.y + h), bg, rnd);
	dl->AddRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + w, pos.y + h), IM_COL32(255, 255, 255, 28), rnd, 0, 1.f);

	float tx = pos.x + (w - ts.x) * 0.5f;
	float ty = pos.y + (h - ts.y) * 0.5f;
	// узкие кнопки (+ и тп) — не клипаем в ноль
	float clip_l = pad_x;
	if (w < ts.x + pad_x * 2.f)
	{
		clip_l = 4.f;
	}

	dl->PushClipRect(ImVec2(pos.x + clip_l, pos.y), ImVec2(pos.x + w - clip_l, pos.y + h), true);
	dl->AddText(ImVec2(tx, ty), IM_COL32(220, 226, 236, 230), label);
	dl->PopClipRect();

	ImGui::PopID();
	return hit;
}