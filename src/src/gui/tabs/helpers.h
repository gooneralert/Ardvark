#pragma once

#include <vector>

namespace ng_tabs
{
	constexpr float content_pad = 14.f;   // padding inside a section box
	constexpr float content_spacing = 0.f;
	constexpr float panel_gap = 22.f;     // gap between the two page columns

	// control-column metrics, taken from the reference menu: every row is
	// label-left / control-right, and the controls line up in one column
	constexpr float k_row_h    = 43.f;   // row pitch
	constexpr float k_ctl_h    = 22.f;   // slider value box height
	constexpr float k_switch_h = 18.f;   // switch height (the reference's are lower)
	constexpr float k_switch_w = 36.f;
	constexpr float k_box_w    = 44.f;   // slider value box
	constexpr float k_track_w  = 89.f;   // slider track
	constexpr float k_drop_w   = 128.f;  // dropdown value box
	constexpr float k_drop_h   = 30.f;   // dropdown height (taller than a switch)
	constexpr float k_swatch_w = 24.f;   // color swatch (square)

	bool glass_mode();   // true when the LiquidUI glass kit drives the controls
	// switch rows to ImGui-drawn hover chrome (used inside solid popups, where
	// a glass hover plate would be painted over by the popup background)
	void rows_use_imgui_chrome(bool on);
	void pad();
	void begin_columns(float* out_left_w, float* out_right_w, float* out_h);

	// a page column: full-height, scrolls when the sections overflow
	void begin_column(const char* id, float width, float height);
	void end_column();

	void section_header(const char* text);      // "MAIN" / "SELECTION" / ...
	bool begin_section(const char* id);         // auto-height box for its rows
	void end_section();

	bool begin_panel(const char* id, float width, float height, bool scrollable = false);
	void end_panel();

	void row_keybind(const char* id, const char* label, int* key, int* mode);
	void row_keybind_simple(const char* id, const char* label, int* key);
	bool row_combo(const char* label, int* cur, const std::vector<const char*>& items);
	// the same dropdown, flagged as owning sub-options: draws the reference's
	// "..." marker left of its box and toggles `open` - the caller draws the
	// sub-rows while it is open, so extra settings stay off the page itself
	bool row_combo_more(const char* label, int* cur, const std::vector<const char*>& items, bool* open);
	bool row_slider_f(const char* label, float* v, float mn, float mx, const char* fmt = "%.1f");
	bool row_slider_i(const char* label, int* v, int mn, int mx);
	bool row_checkbox(const char* label, bool* v);
	bool row_checkbox_more(const char* label, bool* v);   // row that owns sub-options
	// same row, but the "..." marker is clickable and toggles `open` - use it
	// for a row that must stay on (the master) while hiding its sub-options
	bool row_checkbox_expand(const char* label, bool* v, bool* open);
	bool row_checkbox_color(const char* label, bool* v, float col[4]);
	bool row_checkbox_keybind(const char* label, bool* v, int* key);
	bool row_multicombo(const char* id, const char* label, bool* selected, const std::vector<const char*>& items);
	bool row_color(const char* label, float col[4]);
	bool row_button(const char* label);
	bool row_input_text(const char* label, char* buf, int len);

	// muted full-width note (empty states / captions)
	void row_note(const char* text);
	// expandable row: label + chevron (optionally a master square checkbox).
	// Returns true while `open`; the square toggles `check`, the rest expands.
	bool expand_row(const char* label, bool* open, bool* check);
	// a feature we do not implement yet: the switch works and its state is
	// remembered per row, it just doesn't drive anything
	bool row_placeholder(const char* label);
}
