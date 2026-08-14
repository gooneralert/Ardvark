#pragma once

#include <vector>

namespace ng_tabs
{
	constexpr float content_pad = 6.f;
	constexpr float content_spacing = 8.f;
	constexpr float panel_gap = 6.f;

	void pad();
	void begin_columns(float* out_left_w, float* out_right_w, float* out_h);
	bool begin_panel(const char* id, float width, float height);
	void end_panel();

	void row_keybind(const char* id, const char* label, int* key, int* mode);
	void row_keybind_simple(const char* id, const char* label, int* key);
	bool row_combo(const char* label, int* cur, const std::vector<const char*>& items);
	bool row_slider_f(const char* label, float* v, float mn, float mx, const char* fmt = "%.1f");
	bool row_slider_i(const char* label, int* v, int mn, int mx);
	bool row_checkbox(const char* label, bool* v);
	bool row_checkbox_color(const char* label, bool* v, float col[4]);
	bool row_checkbox_keybind(const char* label, bool* v, int* key);
	bool row_multicombo(const char* id, const char* label, bool* selected, const std::vector<const char*>& items);
}
