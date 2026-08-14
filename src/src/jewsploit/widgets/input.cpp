#include "input.h"
#include "../colors/colors.h"

#include <imgui.h>

bool ng::input(const char* id, char* buf, int buf_n)
{
	if (!buf || buf_n <= 0)
	{
		return false;
	}

	// справа от бокса как у select, внутри текста ~12
	float pad_r = 12.f;
	float pad_x = 12.f;
	float avail = ImGui::GetContentRegionAvail().x;
	float box_w = avail - pad_r;
	if (box_w < 40.f)
	{
		box_w = 40.f;
	}

	float box_h = 30.f;
	float rnd = 10.f;
	float pad_y = (box_h - ImGui::GetFontSize()) * 0.5f;
	if (pad_y < 0.f)
	{
		pad_y = 0.f;
	}

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	bool hov = false;
	{
		ImVec2 m = ImGui::GetIO().MousePos;
		if (m.x >= pos.x && m.x <= pos.x + box_w && m.y >= pos.y && m.y <= pos.y + box_h)
		{
			hov = true;
		}
	}

	ImU32 bg = col::checkbox_off_u32();
	if (hov)
	{
		bg = IM_COL32(36, 40, 50, 220);
	}

	dl->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + box_w, pos.y + box_h), bg, rnd);
	dl->AddRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + box_w, pos.y + box_h), IM_COL32(255, 255, 255, 28), rnd, 0, 1.f);

	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(220.f / 255.f, 226.f / 255.f, 236.f / 255.f, 1.f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad_x, pad_y));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rnd);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

	ImGui::SetCursorScreenPos(pos);
	ImGui::SetNextItemWidth(box_w);
	bool ch = ImGui::InputText(id, buf, (size_t)buf_n);

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(4);

	return ch;
}