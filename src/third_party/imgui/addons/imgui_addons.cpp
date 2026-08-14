#include "../imgui_internal.h"
#include "imgui_addons.h"

#include <map>
#include <string>

using namespace ImGui;

void ImAdd::SeparatorText(const char* label, float thickness)
{
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const ImVec2 label_size = CalcTextSize(label, NULL, true);

	ImVec2 pos = window->DC.CursorPos;
	ImVec2 size = CalcItemSize(ImVec2(-0.1f, g.FontSize), label_size.x, g.FontSize);

	const ImRect bb(pos, pos + size);
	ItemSize(bb);
	if (!ItemAdd(bb, id))
		return;

	window->DrawList->AddText(pos, GetColorU32(ImGuiCol_TextDisabled), label);

	// линия справа от текста
	if (thickness > 0)
		window->DrawList->AddLine(
			pos + ImVec2(label_size.x + style.ItemInnerSpacing.x, size.y / 2),
			pos + ImVec2(size.x, size.y / 2),
			GetColorU32(ImGuiCol_Border),
			thickness);
}

void ImAdd::VSeparator(float margin, float thickness)
{
	if (thickness <= 0)
		return;

	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return;

	ImVec2 pos = window->DC.CursorPos;
	ImVec2 size = CalcItemSize(ImVec2(thickness, -0.1f), thickness, thickness);

	const ImRect bb(pos, pos + size);
	const ImRect fill(pos + ImVec2(0, margin), pos + size - ImVec2(0, margin));

	ItemSize(ImVec2(thickness, 0.0f));
	if (!ItemAdd(bb, 0))
		return;

	window->DrawList->AddRectFilled(fill.Min, fill.Max, GetColorU32(ImGuiCol_Border));
}
