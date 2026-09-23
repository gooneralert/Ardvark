//============ Copyright KiwiHax, All rights reserved ============//
//
//  Purpose: 
//
//================================================================//

#pragma once

#include <vector>
#include "../imgui.h"

#define IMADD_ANIMATIONS_SPEED	0.025f

struct ImGuiWindow;

namespace ImAdd
{
	// Rounded button-shaped swatch beside checkbox labels.
	constexpr float kColorSwatchSize = 20.f;
	constexpr float kColorSwatchWidth = 20.f;
	constexpr float kColorSwatchGap  = 6.f;

	// Helpers
	ImVec4  HexToColorVec4(unsigned int hex_color, float alpha = 1.0f);
	float	GetColorPickerWidth();
	// Width of the small square settings-icon button drawn by KeyBind() — used
	// by call sites that right-align the button against the edge of a panel.
	float	GetKeyBindWidth();
	
	// Separators
	void    SeparatorText(const char* label, float thickness = 1.0f);
	void    VSeparator(float margin = 0.0f, float thickness = 1.0f);

	// Widgets
	bool	SelectableLabel(const char* label, bool selected, bool centered = false, const ImVec2& size_arg = ImVec2(0, 0));
	bool    CheckBox(const char* label, bool* v, float trailing_extra = 0.f);
	// Checkbox + color swatch placed after the label with a fixed gap.
	bool    CheckBoxColor(const char* label, bool* v, float col[4], bool layout_color = true);
	// Three checkbox+color pairs on one row, evenly divided into columns.
	void    CheckBoxColorRow3(
		const char* l0, bool* v0, float* c0,
		const char* l1, bool* v1, float* c1,
		const char* l2, bool* v2, float* c2);
	// Two checkboxes side-by-side, each taking exactly 50% of the available
	// width — used to lay independent toggles out in a compact 2-column grid
	// instead of a tall vertical list.
	bool    CheckBoxRow2(const char* label1, bool* v1, const char* label2, bool* v2);
	// Checkbox + settings gear on one row. The gear sits next to the label
	// with the same gap used between the box and text (not pinned right).
	bool    CheckBoxKeyBind(const char* label, bool* v, ImGuiKey* k, int* activation_mode);
	bool    EnableRow(const char* label, bool* v, ImGuiKey* k, int* activation_mode);
	bool	Button(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags button_flags = 0);
	bool	ButtonAccent(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags button_flags = 0);
	bool	Combo(const char* label, int* selected_index, std::vector<const char*> items);
	bool	ColorEdit4(const char* label, float col[4], const ImVec2& size_arg = ImVec2(0, 0));
	bool	KeyBind(const char* str_id, ImGuiKey* k, const ImVec2& size_arg = ImVec2(0, 0), int* activation_mode = nullptr);

	// Child Windows
	bool	Tab(const char* label, bool selected, const ImVec2& size_arg = ImVec2(0, 0));
	void	ScrollBar(const char* str_id, ImGuiWindow* window, const ImVec2& size_arg = ImVec2(0, 0));
	bool	BeginChild(const char* str_id, std::vector<const char*> tabs, int* selected_tab_index_callback, const ImVec2& size_arg = ImVec2(0, 0), const char* badge = nullptr, ImVec4 badge_col = ImVec4(1,1,1,1));
	bool	BeginChild(const char* str_id, std::vector<const char*> tabs, const ImVec2& size_arg = ImVec2(0, 0));
	bool	BeginChild(const char* str_id, const ImVec2& size_arg = ImVec2(0, 0));
	// Plain tab-header style title + right-aligned "Active"/"Disabled" badge,
	// but with no clickable sub-tab strip below it — for sections that need
	// the same header treatment as Aimbot/Visuals/etc. without real subtabs.
	bool	BeginChild(const char* str_id, const ImVec2& size_arg, const char* badge, ImVec4 badge_col = ImVec4(1,1,1,1));
	void    EndChild();

	// Animated reveal block — wraps a chunk of conditionally-shown content
	// (e.g. "if (feature enabled) { extra settings }") so it smoothly grows
	// in / shrinks out over `duration_sec` (200ms by default) with a fade,
	// instead of popping in/out instantly. Returns false (and draws/measures
	// nothing) once fully collapsed — callers should skip the EndReveal()
	// call in that case, exactly like ImGui::BeginChild/EndChild:
	//
	//   if (ImAdd::BeginReveal("id", condition)) {
	//       ... widgets ...
	//       ImAdd::EndReveal();
	//   }
	bool	BeginReveal(const char* str_id, bool show, float duration_sec = 0.2f);
	void	EndReveal();

	// Sliders
	bool	SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format = NULL);
	bool	SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f");
	bool	SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d");

	// Drawing
	void	RenderText(ImVec2 pos, const char* text, const char* text_end = NULL, bool hide_text_after_hash = true, bool has_outlines = false);
	// Animated glow helpers shared with the built-in widgets so custom-drawn
	// elements (e.g. the config-card grid) highlight exactly like buttons:
	// GlowAlphaEased eases a per-id 0..1 hover progress over duration_sec,
	// DrawGlowRectBg paints the soft gaussian halo behind a rounded rect.
	float	GlowAlphaEased(ImGuiID id, bool active, float duration_sec = 0.2f);
	void	DrawGlowRectBg(ImDrawList* dl, const ImVec2& bb_min, const ImVec2& bb_max, float rounding, float alpha01,
		ImVec4 col = ImVec4(1, 1, 1, 1), float spread = 14.f, int layers = 18);
	// Draw a texture clipped to rounded corners. corner_mask_col fills the cut-off
	// corner pixels (pass the panel background colour behind the image).
	void	ImageRounded(ImDrawList* dl, ImTextureID tex, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, float rounding, ImU32 corner_mask_col);

	// Staggered row reveal — call SetStaggerAnim() once per frame, then wrap each
	// logical feature row in BeginStaggerRow(index) / EndStaggerRow().
	void	SetStaggerAnim(float base_alpha, float (*row_progress)(int row));
	bool	BeginStaggerRow(int row);
	void	EndStaggerRow();
}
