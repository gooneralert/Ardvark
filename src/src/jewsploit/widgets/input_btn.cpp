#include "input_btn.h"
#include "../colors/colors.h"

#include <imgui.h>

bool ng::input_btn(const char* id, char* buf, int buf_n, const char* btn, bool* btn_hit)
{
	if (btn_hit)
	{
		*btn_hit = false;
	}

	if (!buf || buf_n <= 0 || !btn || !btn[0])
	{
		return false;
	}

	ImGui::PushID(id);

	float pad_r = 12.f;
	float gap = 6.f;
	float avail = ImGui::GetContentRegionAvail().x;
	float total_w = avail - pad_r;
	if (total_w < 80.f)
	{
		total_w = 80.f;
	}

	ImVec2 bts = ImGui::CalcTextSize(btn);
	float btn_w = bts.x + 24.f;
	if (btn_w < 52.f)
	{
		btn_w = 52.f;
	}

	float box_w = total_w - gap - btn_w;
	if (box_w < 40.f)
	{
		box_w = 40.f;
	}

	float box_h = 30.f;
	float rnd = 10.f;

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImVec2 m = ImGui::GetIO().MousePos;
	bool in_hov = m.x >= pos.x && m.x <= pos.x + box_w && m.y >= pos.y && m.y <= pos.y + box_h;

	ImU32 ibg = col::checkbox_off_u32();
	if (in_hov)
	{
		ibg = IM_COL32(36, 40, 50, 220);
	}

	dl->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + box_w, pos.y + box_h), ibg, rnd);
	dl->AddRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + box_w, pos.y + box_h), IM_COL32(255, 255, 255, 28), rnd, 0, 1.f);

	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(220.f / 255.f, 226.f / 255.f, 236.f / 255.f, 1.f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, (box_h - ImGui::GetFontSize()) * 0.5f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rnd);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

	ImGui::SetNextItemWidth(box_w);
	bool ch = ImGui::InputText("##in", buf, (size_t)buf_n);

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(4);

	float bx = pos.x + box_w + gap;
	float by = pos.y;

	ImGui::SetCursorScreenPos(ImVec2(bx, by));
	ImGui::InvisibleButton("btn", ImVec2(btn_w, box_h));
	bool hit = ImGui::IsItemClicked();
	bool hov = ImGui::IsItemHovered();

	if (hit && btn_hit)
	{
		*btn_hit = true;
	}

	ImU32 bbg = col::checkbox_off_u32();
	if (hov)
	{
		bbg = IM_COL32(36, 40, 50, 220);
	}

	dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + btn_w, by + box_h), bbg, rnd);
	dl->AddRect(ImVec2(bx, by), ImVec2(bx + btn_w, by + box_h), IM_COL32(255, 255, 255, 28), rnd, 0, 1.f);

	dl->AddText(
		ImVec2(bx + (btn_w - bts.x) * 0.5f, by + (box_h - bts.y) * 0.5f),
		IM_COL32(220, 226, 236, 230),
		btn
	);

	ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + box_h));
	ImGui::Dummy(ImVec2(0.f, 0.01f));

	ImGui::PopID();
	return ch || hit;
}