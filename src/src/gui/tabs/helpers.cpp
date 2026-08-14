#include "pch.h"
#include "helpers.h"
#include "../widgets/widgets.h"
#include "../widgets/text.h"
#include "imgui.h"
#include <cstdio>
#include <vector>

namespace ng_tabs
{
	static const ImVec4 k_border = ImVec4(0.18f, 0.18f, 0.18f, 1.f);

	void pad()
	{
		ImGui::SetCursorPosX(content_pad);
	}

	void begin_columns(float* out_left_w, float* out_right_w, float* out_h)
	{
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float left = (float)(int)((avail.x - panel_gap) * 0.5f);
		float right = avail.x - panel_gap - left;
		if (out_left_w) *out_left_w = left;
		if (out_right_w) *out_right_w = right;
		if (out_h) *out_h = avail.y;
	}

	bool begin_panel(const char* id, float width, float height)
	{
		ImGui::PushStyleColor(ImGuiCol_Border, k_border);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.f);
		ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleVar(2);

		float content_w = ImGui::GetWindowSize().x - content_pad * 2.f;
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, content_spacing));
		ImGui::SetCursorPos(ImVec2(content_pad, content_pad));
		ImGui::PushItemWidth(content_w);
		return true;
	}

	void end_panel()
	{
		ImGui::PopItemWidth();
		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	void row_keybind(const char* id, const char* label, int* key, int* mode)
	{
		if (!key || !mode) return;

		ImGui::PushID(id);
		pad();

		float avail_w = ImGui::CalcItemWidth();
		if (avail_w < 1.f)
			avail_w = ImGui::GetContentRegionAvail().x;

		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 ts = ImGui::CalcTextSize(label ? label : "");
		float row_h = ts.y > 0.f ? ts.y : ImGui::GetFontSize();

		const char* kn = widgets::key_name(*key);
		char kb_buf[32];
		snprintf(kb_buf, sizeof(kb_buf), "[%s]", kn);
		ImVec2 kb_sz = ImGui::CalcTextSize(kb_buf);

		ImGui::Dummy(ImVec2(avail_w, row_h));
		ImDrawList* dl = ImGui::GetWindowDrawList();
		widgets::text_outlined(dl, pos, ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.85f, 1.f)), label ? label : "");

		ImGui::SetCursorScreenPos(ImVec2(pos.x + avail_w - kb_sz.x, pos.y));
		widgets::keybind("##kb", key);

		ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + row_h));
		ImGui::Dummy(ImVec2(0.01f, 0.01f));

		static const std::vector<const char*> modes = { "hold", "toggle", "always" };
		if (*mode < 0) *mode = 0;
		if (*mode > 2) *mode = 2;
		pad();
		widgets::combo("##mode", mode, modes);

		ImGui::PopID();
	}

	void row_keybind_simple(const char* id, const char* label, int* key)
	{
		if (!key) return;

		ImGui::PushID(id);
		pad();

		float avail_w = ImGui::CalcItemWidth();
		if (avail_w < 1.f)
			avail_w = ImGui::GetContentRegionAvail().x;

		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 ts = ImGui::CalcTextSize(label ? label : "");
		float row_h = ts.y > 0.f ? ts.y : ImGui::GetFontSize();

		const char* kn = widgets::key_name(*key);
		char kb_buf[32];
		snprintf(kb_buf, sizeof(kb_buf), "[%s]", kn);
		ImVec2 kb_sz = ImGui::CalcTextSize(kb_buf);

		ImGui::Dummy(ImVec2(avail_w, row_h));
		ImDrawList* dl = ImGui::GetWindowDrawList();
		widgets::text_outlined(dl, pos, ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.85f, 1.f)), label ? label : "");

		ImGui::SetCursorScreenPos(ImVec2(pos.x + avail_w - kb_sz.x, pos.y));
		widgets::keybind("##kb", key);

		ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + row_h));
		ImGui::Dummy(ImVec2(0.01f, 0.01f));

		ImGui::PopID();
	}

	bool row_combo(const char* label, int* cur, const std::vector<const char*>& items)
	{
		pad();
		return widgets::combo(label, cur, items);
	}

	bool row_slider_f(const char* label, float* v, float mn, float mx, const char* fmt)
	{
		pad();
		return widgets::slider_float(label, v, mn, mx, fmt);
	}

	bool row_slider_i(const char* label, int* v, int mn, int mx)
	{
		pad();
		return widgets::slider_int(label, v, mn, mx);
	}

	bool row_checkbox(const char* label, bool* v)
	{
		pad();
		return widgets::checkbox(label, v);
	}

	bool row_checkbox_color(const char* label, bool* v, float col[4])
	{
		pad();
		return widgets::checkbox_color(label, v, col);
	}

	bool row_checkbox_keybind(const char* label, bool* v, int* key)
	{
		pad();
		return widgets::checkbox_keybind(label, v, key);
	}

	bool row_multicombo(const char* id, const char* label, bool* selected, const std::vector<const char*>& items)
	{
		pad();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		widgets::text_outlined(dl, pos, ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.85f, 1.f)), label ? label : "");
		ImGui::Dummy(ImVec2(0.f, ImGui::GetFontSize() + 2.f));
		pad();
		return widgets::multicombo(id, selected, items);
	}
}
