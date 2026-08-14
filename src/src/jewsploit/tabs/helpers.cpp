#include "helpers.h"
#include "../widgets/checkbox.h"
#include "../widgets/colorpicker.h"
#include "../widgets/color_presets.h"
#include "../widgets/color_style.h"
#include "../widgets/keybind.h"
#include "../widgets/select.h"
#include "../widgets/slider.h"
#include "../widgets/dropdown.h"
#include "../widgets/spacing.h"
#include "../animation/animation.h"

#include <imgui.h>

void ng_tabs::pad()
{
	ImGui::SetCursorPosX(14.f);
}

void ng_tabs::gap()
{
	// сначала X, потом dummy — иначе после colorpicker лестница вправо
	ImGui::SetCursorPosX(14.f);
	ImGui::Dummy(ImVec2(0.01f, ng::item_gap));
	ImGui::SetCursorPosX(14.f);
}

void ng_tabs::lab(const char* text)
{
	if (!text) text = "";
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	dl->AddText(p, IM_COL32(220, 226, 236, 230), text);
	ImGui::Dummy(ImVec2(0.f, ImGui::GetFontSize() + 4.f));
	pad();
}

bool ng_tabs::row_cb_color(const char* label, bool* v, float col[4], const char* id)
{
	if (!v || !col) return false;

	pad();
	bool ch = ng::checkbox(label, v);
	ImVec2 r0 = ImGui::GetItemRectMin();
	ImVec2 r1 = ImGui::GetItemRectMax();

	if (ng::cp_style() == 1)
	{
		ImGui::SetCursorPosX(14.f);
		if (ng::color_presets(id, "color", label, col, *v))
			ch = true;
	}

	else
	{
		if (ng::colorpicker(id, col, *v))
			ch = true;
	}

	// свотч уводит курсор вправо — вернуть под чекбокс
	ImGui::SetCursorScreenPos(ImVec2(r0.x, r1.y));
	ImGui::Dummy(ImVec2(0.01f, 0.01f));
	pad();
	return ch;
}

bool ng_tabs::row_cb_color2(
	const char* label,
	bool* v,
	float col_a[4],
	float col_b[4],
	const char* id_a,
	const char* id_b,
	const char* name_a,
	const char* name_b,
	bool colors_always
)
{
	if (!v || !col_a || !col_b) return false;
	if (!name_a) name_a = "outline";
	if (!name_b) name_b = "fill";

	pad();
	bool ch = ng::checkbox(label, v);
	ImVec2 r0 = ImGui::GetItemRectMin();
	ImVec2 r1 = ImGui::GetItemRectMax();
	bool show = colors_always ? true : *v;

	if (ng::cp_style() == 1)
	{
		ImGui::SetCursorPosX(14.f);
		if (ng::color_presets(id_a, "color", name_a, col_a, show))
			ch = true;
		ImGui::SetCursorPosX(14.f);
		if (ng::color_presets(id_b, "color", name_b, col_b, show))
			ch = true;
	}

	else
	{
		if (ng::colorpicker(id_a, col_a, show, 1))
			ch = true;
		if (ng::colorpicker(id_b, col_b, show, 0))
			ch = true;
	}

	ImGui::SetCursorScreenPos(ImVec2(r0.x, r1.y));
	ImGui::Dummy(ImVec2(0.01f, 0.01f));
	pad();
	return ch;
}

void ng_tabs::row_keybind(const char* id, const char* label, int* vk, int* mode)
{
	if (!vk || !mode) return;

	pad();
	ImGui::PushID(id);

	// как чекбокс+keybind: короткий item слева, справа дырка под kb/mode
	ImVec2 p = ImGui::GetCursorScreenPos();
	float h = 28.f;
	ImVec2 ts = ImGui::CalcTextSize(label ? label : "");
	float lw = ts.x + 10.f;
	if (lw < 40.f) lw = 40.f;

	ImGui::Dummy(ImVec2(lw, h));
	ImGui::GetWindowDrawList()->AddText(
		ImVec2(p.x, p.y + (h - ts.y) * 0.5f),
		IM_COL32(220, 226, 236, 230),
		label ? label : ""
	);

	ng::keybind("##kb", vk, mode, true);
	ImGui::PopID();
}

bool ng_tabs::row_select(const char* id, const char* label, int* cur, const char* const items[], int count, bool shown, bool close_on_pick)
{
	if (!cur || !items || count <= 0) return false;

	ImGui::PushID(id);
	ImGuiStorage* st = ImGui::GetStateStorage();
	ImGuiID oid = ImGui::GetID("lab_open");
	float open = st->GetFloat(oid, shown ? 1.f : 0.f);
	open = anim::approach(open, shown ? 1.f : 0.f, 7.f, ImGui::GetIO().DeltaTime);
	st->SetFloat(oid, open);
	float e = anim::ease_out_cubic(open);

	if (e > 0.001f)
	{
		pad();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 p = ImGui::GetCursorScreenPos();
		int a = (int)(230.f * e);
		if (a < 0) a = 0;
		if (a > 255) a = 255;
		dl->AddText(p, IM_COL32(220, 226, 236, a), label ? label : "");
		ImGui::Dummy(ImVec2(0.f, (ImGui::GetFontSize() + 4.f) * e));
		pad();
	}

	bool ch = ng::select("##sel", cur, items, count, shown, close_on_pick);
	ImGui::PopID();
	return ch;
}

bool ng_tabs::row_slider(const char* id, const char* label, float* v, float mn, float mx, bool shown)
{
	if (!v) return false;

	// лейбл вместе со слайдером, а то fog start торчит при выкл
	ImGui::PushID(id);
	ImGuiStorage* st = ImGui::GetStateStorage();
	ImGuiID oid = ImGui::GetID("lab_open");
	float open = st->GetFloat(oid, shown ? 1.f : 0.f);
	open = anim::approach(open, shown ? 1.f : 0.f, 7.f, ImGui::GetIO().DeltaTime);
	st->SetFloat(oid, open);
	float e = anim::ease_out_cubic(open);

	if (e > 0.001f)
	{
		pad();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 p = ImGui::GetCursorScreenPos();
		int a = (int)(230.f * e);
		if (a < 0) a = 0;
		if (a > 255) a = 255;
		dl->AddText(p, IM_COL32(220, 226, 236, a), label ? label : "");
		ImGui::Dummy(ImVec2(0.f, (ImGui::GetFontSize() + 4.f) * e));
		pad();
	}

	bool ch = ng::slider("##sl", v, mn, mx, shown);
	ImGui::PopID();
	return ch;
}

bool ng_tabs::row_slider_i(const char* id, const char* label, int* v, int mn, int mx, bool shown)
{
	if (!v) return false;
	float f = (float)*v;
	bool ch = row_slider(id, label, &f, (float)mn, (float)mx, shown);
	int n = (int)(f + 0.5f);
	if (n < mn) n = mn;
	if (n > mx) n = mx;
	if (n != *v)
	{
		*v = n;
		ch = true;
	}
	return ch;
}

bool ng_tabs::row_dropdown(const char* id, const char* label, bool* sel, const char* const items[], int count, bool shown)
{
	if (!sel || !items || count <= 0) return false;
	pad();
	lab(label);
	return ng::dropdown(id, sel, items, count, shown);
}
