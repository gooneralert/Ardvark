#pragma once

#include "Layout.h"
#include "../execute/Clipboard.h"
#include "../execute/Scripts.h"
#include "../execute/Tabs.h"
#include "../protocol/Editor.h"
#include "../protocol/Log.h"
#include "gui/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include "gui/widgets/widgets.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaDetail {

inline void PushScrollStyle()
{
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.f);
	ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(
		ImGuiCol_ScrollbarGrab,
		ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.35f));
	ImGui::PushStyleColor(
		ImGuiCol_ScrollbarGrabHovered,
		ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.55f));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, colors::accent);
}

inline void PopScrollStyle()
{
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar();
}

inline bool DrawHeaderClose(const ImVec2& title_pos, float title_fs, float title_right)
{
	const float sz = Layout::k_close_sz;
	const float pad = Layout::k_title_pad;
	const float oy = ImFloor((title_fs - sz) * 0.5f);
	ImGui::SetCursorScreenPos(ImVec2(title_right - pad - sz, title_pos.y + oy));
	return widgets::icon_close("##lua_win_close", ImVec2(sz, sz));
}

// табы на верхней кромке content-панели — как draw_tab_bar в меню
inline void DrawFileTabsOnPanel(
	ImDrawList* dl,
	const ImVec2& panel_min,
	const ImVec2& panel_max)
{
	ImFont* font = fonts::ui_bold();
	const float fs = fonts::ui_size(font);
	const float tab_h = Layout::k_tab_h;
	const float tab_top = panel_min.y - tab_h;
	const float spacing = Layout::k_tab_spacing;
	const float close_sz = 12.f;
	const float pad_x = 10.f;
	const float close_gap = 6.f;

	struct TabGeom {
		float x = 0.f;
		float w = 0.f;
		float label_w = 0.f;
	};

	std::vector<TabGeom> geoms;
	geoms.reserve(g_tabs.size());
	float x = panel_min.x;
	const float max_x = panel_max.x - 28.f;

	for (int i = 0; i < (int)g_tabs.size(); ++i)
	{
		const char* label = g_tabs[i].name.empty() ? "tab" : g_tabs[i].name.c_str();
		const float label_w = font->CalcTextSizeA(fs, FLT_MAX, 0.f, label).x;
		float w = pad_x + label_w + close_gap + close_sz + pad_x;
		if (w < 72.f)
			w = 72.f;
		if (x + w > max_x && i > 0)
			break;
		geoms.push_back({ x, w, label_w });
		x += w + spacing;
	}

	const float plus_w = 22.f;
	const bool draw_plus = (x + plus_w <= panel_max.x - 4.f);

	const ImU32 panel_fill = ImGui::GetColorU32(colors::content_fill);
	const ImU32 panel_outer = ImGui::GetColorU32(colors::content_outer_border);
	const ImU32 panel_inner = ImGui::GetColorU32(colors::content_inner_border);

	const int panel_top_outer = (int)ImFloor(panel_min.y);
	const int panel_top_inner = panel_top_outer + 1;
	const int panel_outer_l = (int)ImFloor(panel_min.x);
	const int panel_outer_r = (int)ImCeil(panel_max.x) - 1;
	const int panel_outer_b = (int)ImCeil(panel_max.y) - 1;
	const int panel_inner_l = panel_outer_l + 1;
	const int panel_inner_r = panel_outer_r - 1;
	const int panel_inner_b = panel_outer_b - 1;

	int close_idx = -1;
	int clicked = g_tab;
	if (clicked < 0 || clicked >= (int)geoms.size())
		clicked = 0;

	// hit tests табов (крестик — после paint)
	for (int i = 0; i < (int)geoms.size(); ++i)
	{
		const auto& g = geoms[i];
		ImGui::SetCursorScreenPos(ImVec2(g.x, tab_top));
		ImGui::PushID(i + 3000);
		if (ImGui::InvisibleButton("##ltab", ImVec2(g.w - pad_x - close_sz - 2.f, tab_h)))
			clicked = i;
		ImGui::PopID();
	}

	auto paint_hline = [&](int x0, int x1, int y, ImU32 col) {
		if (x1 < x0) return;
		dl->AddRectFilled(ImVec2((float)x0, (float)y), ImVec2((float)(x1 + 1), (float)(y + 1)), col);
	};
	auto paint_vline = [&](int xv, int y0, int y1, ImU32 col) {
		if (y1 < y0) return;
		dl->AddRectFilled(ImVec2((float)xv, (float)y0), ImVec2((float)(xv + 1), (float)(y1 + 1)), col);
	};
	auto paint_top_gap = [&](int seg_l, int seg_r, int y, int gap_l, int gap_r, ImU32 col) {
		if (gap_l > seg_l)
			paint_hline(seg_l, gap_l - 1, y, col);
		if (gap_r < seg_r)
			paint_hline(gap_r + 1, seg_r, y, col);
	};

	// panel fill + borders (как меню)
	dl->AddRectFilled(
		ImVec2((float)(panel_inner_l + 1), (float)panel_top_outer),
		ImVec2((float)panel_inner_r, (float)(panel_inner_b + 1)),
		panel_fill);
	paint_vline(panel_outer_l, panel_top_outer, panel_outer_b, panel_outer);
	paint_vline(panel_outer_r, panel_top_outer, panel_outer_b, panel_outer);
	paint_hline(panel_outer_l, panel_outer_r, panel_outer_b, panel_outer);
	paint_vline(panel_inner_l, panel_top_inner, panel_inner_b, panel_inner);
	paint_vline(panel_inner_r, panel_top_inner, panel_inner_b, panel_inner);
	paint_hline(panel_inner_l, panel_inner_r, panel_inner_b, panel_inner);

	int gap_l = panel_outer_l;
	int gap_r = panel_outer_l;
	if (!geoms.empty() && clicked >= 0 && clicked < (int)geoms.size())
	{
		gap_l = (int)ImFloor(geoms[clicked].x);
		gap_r = (int)ImFloor(geoms[clicked].x + geoms[clicked].w) - 1;
	}
	paint_top_gap(panel_outer_l, panel_outer_r, panel_top_outer, gap_l, gap_r, panel_outer);
	paint_top_gap(panel_inner_l, panel_inner_r, panel_top_inner, gap_l + 1, gap_r - 1, panel_inner);

	// tabs paint: inactive then active
	for (int pass = 0; pass < 2; ++pass)
	{
		for (int i = 0; i < (int)geoms.size(); ++i)
		{
			const bool active = (i == clicked);
			if (pass == 0 && active) continue;
			if (pass == 1 && !active) continue;

			const auto& g = geoms[i];
			const int ol = (int)ImFloor(g.x);
			const int ot = (int)ImFloor(tab_top);
			const int orr = (int)ImFloor(g.x + g.w) - 1;
			const int ob = panel_top_outer;
			const int il = ol + 1;
			const int it = ot + 1;
			const int ir = orr - 1;

			ImRect fill(
				ImVec2((float)(il + 1), (float)(it + 1)),
				ImVec2((float)ir, (float)panel_top_outer));

			// gradient rows ~ menu tabs
			const float height = fill.GetHeight();
			if (height > 0.f)
			{
				const int grad_n = 17;
				for (int row = 0; row < grad_n - 1; ++row)
				{
					float y0 = fill.Min.y + (height * (float)row) / (float)(grad_n - 1);
					float y1 = fill.Min.y + (height * (float)(row + 1)) / (float)(grad_n - 1);
					float t = (float)row / (float)(grad_n - 1);
					ImVec4 c0, c1;
					if (active)
					{
						ImVec4 top = ImLerp(colors::content_fill, colors::child_fill, 0.15f);
						c0 = ImLerp(top, colors::content_fill, t);
						c1 = ImLerp(top, colors::content_fill, (float)(row + 1) / (float)(grad_n - 1));
					}
					else
					{
						ImVec4 top = ImLerp(colors::child_fill, colors::panel_fill, 0.25f);
						ImVec4 bot = ImLerp(colors::child_fill, colors::content_fill, 0.35f);
						c0 = ImLerp(top, bot, t);
						c1 = ImLerp(top, bot, (float)(row + 1) / (float)(grad_n - 1));
					}
					dl->AddRectFilledMultiColor(
						ImVec2(fill.Min.x, y0), ImVec2(fill.Max.x, y1),
						ImGui::ColorConvertFloat4ToU32(c0), ImGui::ColorConvertFloat4ToU32(c0),
						ImGui::ColorConvertFloat4ToU32(c1), ImGui::ColorConvertFloat4ToU32(c1));
				}
			}

			if (active)
			{
				dl->AddRectFilled(
					ImVec2((float)il, (float)panel_top_outer),
					ImVec2((float)(ir + 1), (float)(panel_top_inner + 1)),
					panel_fill);
			}
			else
			{
				const ImU32 seam = ImGui::ColorConvertFloat4ToU32(
					ImLerp(colors::child_fill, colors::content_fill, 0.35f));
				dl->AddRectFilled(
					ImVec2((float)ol, (float)(panel_top_outer - 1)),
					ImVec2((float)(orr + 1), (float)(panel_top_inner + 1)),
					seam);
			}

			paint_hline(ol, orr, ot, panel_outer);
			paint_vline(ol, ot, ob, panel_outer);
			paint_vline(orr, ot, ob, panel_outer);
			paint_hline(il, ir, it, panel_inner);
			paint_vline(il, it, panel_top_inner, panel_inner);
			paint_vline(ir, it, panel_top_inner, panel_inner);

			const char* label = g_tabs[i].name.empty() ? "tab" : g_tabs[i].name.c_str();
			const ImU32 label_col = active ? colors::accent_u32() : colors::label_u32(0.f);
			const float text_area_w = g.w - pad_x * 2.f - close_sz - close_gap;
			const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
			widgets::draw_outlined_text(
				dl, font, fs,
				ImVec2(
					ImFloor(g.x + pad_x + (text_area_w - tsz.x) * 0.5f),
					ImFloor(tab_top + (tab_h - tsz.y) * 0.5f)),
				label_col, label);
		}
	}

	// крестики и + поверх табов
	for (int i = 0; i < (int)geoms.size(); ++i)
	{
		const auto& g = geoms[i];
		ImGui::SetCursorScreenPos(ImVec2(
			g.x + g.w - pad_x - close_sz,
			tab_top + ImFloor((tab_h - close_sz) * 0.5f)));
		ImGui::PushID(i + 5000);
		if (widgets::icon_close("##ltx", ImVec2(close_sz, close_sz)))
			close_idx = i;
		ImGui::PopID();
	}

	if (draw_plus)
	{
		ImGui::SetCursorScreenPos(ImVec2(x, tab_top + ImFloor((tab_h - Layout::k_btn_h) * 0.5f)));
		ImGui::PushID("##ltab_new");
		if (widgets::button("+", ImVec2(plus_w, Layout::k_btn_h)))
			NewBlankTab();
		ImGui::PopID();
	}

	g_tab = clicked;
	if (close_idx >= 0)
		CloseTab(close_idx);
}

inline bool PassesOutputFilter(const OutputLine& line)
{
	switch (g_out_filter_mode)
	{
	case 1: if (line.level != LogLevel::Print) return false; break;
	case 2: if (line.level != LogLevel::Warn) return false; break;
	case 3: if (line.level != LogLevel::Error) return false; break;
	case 4:
		if (line.level != LogLevel::Success && line.level != LogLevel::Info)
			return false;
		break;
	default: break;
	}
	if (g_out_filter[0] == '\0')
		return true;
	return std::strstr(line.text.c_str(), g_out_filter) != nullptr;
}

inline void DrawScriptList(float width, float height)
{
	ImFont* font = fonts::ui();
	const float fs = fonts::ui_size(font);
	const float tf = PanelFs();
	const float row_h = Layout::k_btn_h;

	if (!widgets::begin_child_panel(
			"lua_scripts", ImVec2(width, height),
			"scripts", fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
	{
		widgets::end_child_panel();
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
	PushScrollStyle();
	if (ImGui::BeginChild("##lua_script_scroll", ImVec2(0, 0), ImGuiChildFlags_None))
	{
		if (g_scripts.empty())
		{
			ImGui::SetCursorPos(ImVec2(Layout::k_inner_pad, Layout::k_child_margin));
			widgets::draw_outlined_text(
				ImGui::GetWindowDrawList(), font, fs,
				ImGui::GetCursorScreenPos(),
				colors::text_inactive_u32(), "no scripts");
			ImGui::Dummy(ImVec2(1, row_h));
		}

		for (int i = 0; i < (int)g_scripts.size(); ++i)
		{
			ImGui::PushID(i);
			const ImVec2 row = ImGui::GetCursorScreenPos();
			bool selected = false;
			if (!g_tabs.empty() &&
				g_tabs[g_tab].path == (ScriptsDir() / g_scripts[i]).string())
			{
				selected = true;
			}

			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(
				ImGuiCol_HeaderHovered,
				ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.20f));
			ImGui::PushStyleColor(
				ImGuiCol_HeaderActive,
				ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.32f));
			const bool clicked = ImGui::Selectable("##s", selected, 0, ImVec2(0, row_h));
			ImGui::PopStyleColor(3);

			if (selected)
			{
				ImVec4 a = colors::accent;
				a.w = 0.14f;
				ImGui::GetWindowDrawList()->AddRectFilled(
					row,
					ImVec2(row.x + ImGui::GetContentRegionAvail().x + Layout::k_inner_pad, row.y + row_h),
					ImGui::ColorConvertFloat4ToU32(a));
			}

			widgets::draw_outlined_text(
				ImGui::GetWindowDrawList(), font, fs,
				ImVec2(ImFloor(row.x + Layout::k_inner_pad), ImFloor(row.y + (row_h - fs) * 0.5f)),
				selected ? colors::accent_u32() : colors::text_active_u32(),
				g_scripts[i].c_str());

			if (clicked)
				LoadFileIntoTab(ScriptsDir() / g_scripts[i]);
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
	PopScrollStyle();
	ImGui::PopStyleColor();
	widgets::end_child_panel();
}

inline void DrawOutputToolbar()
{
	const float y0 = ImGui::GetCursorPosY() + Layout::k_child_margin;
	static const char* k_modes[] = { "all", "print", "warn", "error", "ok" };
	const float row_h = Layout::k_btn_h;
	const float gap = Layout::k_btn_gap;
	const float btn_w = 52.f;
	const float avail = ImGui::GetContentRegionAvail().x;

	ImGui::SetCursorPos(ImVec2(Layout::k_inner_pad, y0));
	widgets::input_text("##lua_out_filter", "filter...", g_out_filter, IM_ARRAYSIZE(g_out_filter), 120.f);

	ImGui::SameLine(0.f, gap);
	widgets::combo_inline("##lua_out_mode", &g_out_filter_mode, k_modes, IM_ARRAYSIZE(k_modes), 78.f);

	ImGui::SameLine(0.f, gap);
	widgets::checkbox_inline("follow", &g_out_auto_scroll);
	ImGui::SameLine(0.f, gap);
	widgets::checkbox_inline("wrap", &g_out_wrap);

	float bx = avail - btn_w * 2.f - gap - Layout::k_inner_pad;
	if (bx < Layout::k_inner_pad)
		bx = Layout::k_inner_pad;
	ImGui::SetCursorPos(ImVec2(bx, y0));
	ImGui::PushID("lua_out_copy");
	if (widgets::button("copy", ImVec2(btn_w, row_h)))
		SetClipboardText(BuildOutputText(g_out_sel >= 0));
	ImGui::PopID();
	ImGui::SameLine(0.f, gap);
	ImGui::PushID("lua_out_clear");
	if (widgets::button("clear", ImVec2(btn_w, row_h)))
	{
		g_output.clear();
		g_out_sel = -1;
	}
	ImGui::PopID();

	ImGui::SetCursorPosY(y0 + row_h + Layout::k_child_margin);
}

inline void DrawOutput(float width, float height)
{
	const float tf = PanelFs();
	if (!widgets::begin_child_panel(
			"lua_output", ImVec2(width, height),
			"output", fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
	{
		widgets::end_child_panel();
		return;
	}

	ImFont* font = fonts::proggy_clean ? fonts::proggy_clean : fonts::ui();
	const float fs = fonts::ui_size(font);

	DrawOutputToolbar();

	ImGui::SetCursorPos(ImVec2(Layout::k_child_margin, ImGui::GetCursorPosY()));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
	PushScrollStyle();

	const ImGuiWindowFlags out_flags = g_out_wrap
		? ImGuiWindowFlags_None
		: ImGuiWindowFlags_HorizontalScrollbar;
	if (ImGui::BeginChild("##lua_output_scroll", ImVec2(0, 0), ImGuiChildFlags_None, out_flags))
	{
		ImGui::PushFont(font, fs);
		const float wrap_w = g_out_wrap ? ImGui::GetContentRegionAvail().x : 0.f;

		if (g_output.empty())
		{
			ImGui::SetCursorPos(ImVec2(Layout::k_inner_pad, Layout::k_child_margin));
			widgets::draw_outlined_text(
				ImGui::GetWindowDrawList(), fonts::ui(), fonts::ui_size(),
				ImGui::GetCursorScreenPos(),
				colors::text_inactive_u32(),
				"empty - press run");
			ImGui::Dummy(ImVec2(1.f, Layout::k_btn_h));
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 2.f));
			for (int i = 0; i < (int)g_output.size(); ++i)
			{
				const auto& line = g_output[i];
				if (!PassesOutputFilter(line))
					continue;

				ImGui::PushID(i);
				char prefix[16];
				std::snprintf(prefix, sizeof(prefix), "[%s]", LevelTag(line.level));

				const bool selected = (g_out_sel == i);
				ImGui::PushStyleColor(ImGuiCol_Header,
					ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.18f));
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
					ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.28f));
				ImGui::PushStyleColor(ImGuiCol_HeaderActive,
					ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.38f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(LevelColor(line.level)));

				char label[2048];
				std::snprintf(label, sizeof(label), "%s  %s", prefix, line.text.c_str());

				ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;
				if (g_out_wrap)
				{
					ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_w);
					if (ImGui::Selectable(label, selected, flags))
					{
						g_out_sel = i;
						if (ImGui::IsMouseDoubleClicked(0))
							SetClipboardText(line.text);
					}
					ImGui::PopTextWrapPos();
				}
				else if (ImGui::Selectable(label, selected, flags))
				{
					g_out_sel = i;
					if (ImGui::IsMouseDoubleClicked(0))
						SetClipboardText(line.text);
				}

				ImGui::PopStyleColor(4);

				if (ImGui::BeginPopupContextItem("##out_ctx"))
				{
					g_out_sel = i;
					if (ImGui::MenuItem("copy line"))
						SetClipboardText(line.text);
					if (ImGui::MenuItem("copy all"))
						SetClipboardText(BuildOutputText(false));
					if (ImGui::MenuItem("clear output"))
					{
						g_output.clear();
						g_out_sel = -1;
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
			ImGui::PopStyleVar();

			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
				ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
			{
				SetClipboardText(BuildOutputText(g_out_sel >= 0));
			}
		}

		if (g_scroll_out && g_out_auto_scroll)
		{
			ImGui::SetScrollHereY(1.f);
			g_scroll_out = false;
		}
		ImGui::PopFont();
	}
	ImGui::EndChild();
	PopScrollStyle();
	ImGui::PopStyleColor();
	widgets::end_child_panel();
}

inline void DrawStatusBar(TextEditor& ed, float width)
{
	ImFont* font = fonts::ui();
	const float fs = fonts::ui_size(font) - 1.f;
	const float status_h = 18.f;
	ImDrawList* dl = ImGui::GetWindowDrawList();

	const ImVec2 p = ImGui::GetCursorScreenPos();
	ImVec4 bar = colors::child_fill;
	bar.x = ImMin(bar.x + 0.03f, 1.f);
	bar.y = ImMin(bar.y + 0.03f, 1.f);
	bar.z = ImMin(bar.z + 0.03f, 1.f);
	dl->AddRectFilled(p, ImVec2(p.x + width, p.y + status_h), ToU32(bar));

	ImVec4 line = colors::accent;
	line.w = 0.30f;
	dl->AddRectFilled(p, ImVec2(p.x + width, p.y + 1.f), ToU32(line));

	const auto cur = ed.GetCursorPosition();
	char left[128];
	char right[96];
	std::snprintf(left, sizeof(left), "Ln %d, Col %d", cur.mLine + 1, cur.mColumn + 1);
	std::snprintf(right, sizeof(right), "Lua  |  %d lines", ed.GetTotalLines());

	widgets::draw_outlined_text(
		dl, font, fs,
		ImVec2(ImFloor(p.x + Layout::k_inner_pad), ImFloor(p.y + (status_h - fs) * 0.5f)),
		colors::text_inactive_u32(), left);

	const ImVec2 rs = font->CalcTextSizeA(fs, FLT_MAX, 0.f, right);
	widgets::draw_outlined_text(
		dl, font, fs,
		ImVec2(ImFloor(p.x + width - rs.x - Layout::k_inner_pad), ImFloor(p.y + (status_h - fs) * 0.5f)),
		colors::text_inactive_u32(), right);

	ImGui::Dummy(ImVec2(width, status_h));
}

inline void DrawEditorFindBar(TextEditor& ed)
{
	const float y0 = ImGui::GetCursorPosY() + Layout::k_child_margin;
	const float avail = ImGui::GetContentRegionAvail().x;
	const float next_w = 52.f;
	const float gap = Layout::k_btn_gap;
	float find_w = avail - Layout::k_inner_pad * 2.f - next_w - gap;
	if (find_w < 80.f)
		find_w = 80.f;

	ImGui::SetCursorPos(ImVec2(Layout::k_inner_pad, y0));
	widgets::input_text("##lua_find", "find in editor...", g_find_buf, IM_ARRAYSIZE(g_find_buf), find_w);
	ImGui::SameLine(0.f, gap);
	if (widgets::button("next", ImVec2(next_w, Layout::k_btn_h)) && g_find_buf[0])
	{
		const std::string text = ed.GetText();
		const auto cur = ed.GetCursorPosition();
		size_t flat = 0;
		{
			const auto lines = ed.GetTextLines();
			for (int i = 0; i < cur.mLine && i < (int)lines.size(); ++i)
				flat += lines[i].size() + 1;
			flat += (size_t)cur.mColumn;
			if (flat < text.size())
				++flat;
		}
		const size_t found = text.find(g_find_buf, flat);
		const size_t use = (found == std::string::npos) ? text.find(g_find_buf) : found;
		if (use != std::string::npos)
		{
			int line = 0, col = 0;
			size_t i = 0;
			while (i < use && i < text.size())
			{
				if (text[i] == '\n') { ++line; col = 0; }
				else ++col;
				++i;
			}
			const int len = (int)std::strlen(g_find_buf);
			TextEditor::Coordinates a(line, col);
			TextEditor::Coordinates b(line, col + len);
			ed.SetCursorPosition(a);
			ed.SetSelection(a, b);
		}
	}
	ImGui::SetCursorPosY(y0 + Layout::k_btn_h + Layout::k_child_margin);
}

inline void DrawEditor(float width, float height)
{
	const float tf = PanelFs();
	if (!widgets::begin_child_panel(
			"lua_editor", ImVec2(width, height),
			"editor", fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
	{
		widgets::end_child_panel();
		return;
	}

	TextEditor& ed = CurEditor();
	ed.SetPalette(MakePalette());
	DrawEditorFindBar(ed);

	ImGui::SetCursorPos(ImVec2(Layout::k_child_margin, ImGui::GetCursorPosY()));
	ImVec2 avail = ImGui::GetContentRegionAvail();
	const float status_h = 18.f;
	float editor_h = avail.y - status_h;
	if (editor_h < 40.f)
		editor_h = 40.f;
	float editor_w = avail.x;
	if (editor_w < 40.f)
		editor_w = 40.f;

	ImFont* code_font = fonts::proggy_clean ? fonts::proggy_clean : fonts::ui();
	ImGui::PushFont(code_font, fonts::ui_size(code_font));
	PushScrollStyle();

	char id[32];
	std::snprintf(id, sizeof(id), "##lua_te_%d", g_tab);
	ed.Render(id, ImVec2(editor_w, editor_h), false);

	PopScrollStyle();
	ImGui::PopFont();
	DrawStatusBar(ed, editor_w);
	widgets::end_child_panel();
}

inline float DrawVSplitter(float width, float* frac, float total_h)
{
	const float grip = Layout::k_split_h;
	ImGui::InvisibleButton("##lua_vsplit", ImVec2(width, grip));
	const bool hov = ImGui::IsItemHovered();
	const bool act = ImGui::IsItemActive();
	if (hov || act)
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 mn = ImGui::GetItemRectMin();
	const ImVec2 mx = ImGui::GetItemRectMax();
	const float mid_y = ImFloor((mn.y + mx.y) * 0.5f);
	ImVec4 c = colors::accent;
	c.w = act ? 0.85f : (hov ? 0.45f : 0.22f);
	dl->AddRectFilled(
		ImVec2(mn.x + Layout::k_inner_pad, mid_y),
		ImVec2(mx.x - Layout::k_inner_pad, mid_y + 1.f),
		ToU32(c));

	if (act && total_h > 1.f)
	{
		*frac += ImGui::GetIO().MouseDelta.y / total_h;
		if (*frac < 0.30f) *frac = 0.30f;
		if (*frac > 0.85f) *frac = 0.85f;
	}
	return grip;
}

}
}
}
