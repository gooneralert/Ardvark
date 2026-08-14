#pragma once

#include "imgui.h"
#include "../colors/colors.h"
#include "../resources/fonts/fonts.h"
#include "../widgets/widgets.h"
#include "app/Settings.h"

namespace Cheat {
namespace GUI {
namespace menu {

inline void sync_gui_theme()
{
	auto& g = Cheat::g_Settings.gui;
	if (g.theme < 0 || g.theme >= colors::Theme_Count)
		g.theme = colors::Theme_Default;

	if (g.theme != colors::Theme_Custom)
	{
		colors::ApplyPreset(
			g.theme, g.accent, g.text_active, g.text_inactive,
			g.outer_border, g.inner_border, g.panel_fill,
			g.content_outer, g.content_inner, g.content_fill, g.child_fill);
	}

	colors::SyncFromSettings(
		g.accent, g.text_active, g.text_inactive,
		g.outer_border, g.inner_border, g.panel_fill,
		g.content_outer, g.content_inner, g.content_fill, g.child_fill);
}

inline void row_checkbox_color(const char* label, bool* value, float color[4], const char* color_id)
{
	widgets::checkbox(label, value);
	const float row_y = widgets::color_picker_row_y();
	widgets::same_line_color_picker(row_y, 0, 1);
	widgets::color_edit4(color_id, color);
}

inline void label_color_row(const char* label, float color[4], const char* id)
{
	ImGui::SetCursorPosX(6.0f);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);

	const float row_y = ImGui::GetCursorPosY();
	ImFont* font = fonts::ui();
	const float fs = fonts::ui_size(font);
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, label);
	widgets::draw_outlined_text(
		ImGui::GetWindowDrawList(), font, fs,
		ImVec2(ImFloor(pos.x), ImFloor(pos.y)),
		colors::text_active_u32(), label);
	ImGui::Dummy(ImVec2(tsz.x, tsz.y));

	widgets::same_line_color_picker(row_y, 0, 1);
	widgets::color_edit4(id, color);
}

inline void theme_color_row(const char* label, float color[4], const char* id)
{
	ImGui::SetCursorPosX(6.0f);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);

	ImFont* font = fonts::ui();
	const float fs = fonts::ui_size(font);
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, label);
	widgets::draw_outlined_text(
		ImGui::GetWindowDrawList(), font, fs,
		ImVec2(ImFloor(pos.x), ImFloor(pos.y)),
		colors::text_active_u32(), label);
	ImGui::Dummy(ImVec2(tsz.x, tsz.y));

	const float row_y = widgets::color_picker_row_y();
	widgets::same_line_color_picker(row_y, 0, 1);
	if (widgets::color_edit4(id, color))
		Cheat::g_Settings.gui.theme = colors::Theme_Custom;
}

inline ImVec2 make_side_child_size(
	float inner_w,
	float side_child_h,
	float side_child_gap,
	float side_child_w_min,
	int columns)
{
	int cols = columns;
	if (cols < 1) cols = 1;
	if (cols > 2) cols = 2;
	float gaps = side_child_gap * (float)(cols - 1);
	float column_w = (inner_w - gaps) / (float)cols;
	if (column_w < side_child_w_min) column_w = side_child_w_min;
	return ImVec2(column_w, side_child_h);
}

inline bool draw_side_child(
	const char* id,
	const char* title,
	const ImVec2& cursor_pos,
	const ImVec2& size)
{
	ImGui::SetCursorPos(cursor_pos);
	const float side_title_size = fonts::ui_size(fonts::ui_bold());
	return widgets::begin_child_panel(
		id,
		size,
		title,
		fonts::ui_bold(),
		side_title_size,
		nullptr,
		nullptr,
		nullptr);
}

}
}
}
