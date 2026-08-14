#pragma once

namespace ng_tabs
{
	void pad();
	void gap();
	void lab(const char* text);

	bool row_cb_color(const char* label, bool* v, float col[4], const char* id);
	// colors_always — свотчи/пресеты даже если чекбокс выкл (chams)
	bool row_cb_color2(
		const char* label,
		bool* v,
		float col_a[4],
		float col_b[4],
		const char* id_a,
		const char* id_b,
		const char* name_a = "outline",
		const char* name_b = "fill",
		bool colors_always = false
	);

	void row_keybind(const char* id, const char* label, int* vk, int* mode);

	bool row_select(const char* id, const char* label, int* cur, const char* const items[], int count, bool shown = true, bool close_on_pick = true);
	bool row_slider(const char* id, const char* label, float* v, float mn, float mx, bool shown = true);
	bool row_slider_i(const char* id, const char* label, int* v, int mn, int mx, bool shown = true);
	bool row_dropdown(const char* id, const char* label, bool* sel, const char* const items[], int count, bool shown = true);
}
