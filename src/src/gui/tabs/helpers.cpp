#include "pch.h"
#include "helpers.h"
#include "../widgets/widgets.h"
#include "../widgets/text.h"
#include "../glass.h"
#include "../liquid_ui.h"
#include "imgui.h"
#include <cstdio>
#include <vector>
#include <windows.h>

namespace ng_tabs
{
	static const ImVec4 k_border = ImVec4(0.18f, 0.18f, 0.18f, 1.f);

	// -------------------------------------------------------------------------
	// LiquidUI mode: when the glass renderer is live every control renders
	// through the vendored LiquidUI widget kit - the same widgets the reference
	// app uses (RowToggle, Slider, Dropdown, Chip, ...). When it is unavailable
	// we fall back to the legacy ImGui widgets so the menu stays usable.
	// -------------------------------------------------------------------------
	bool glass_mode() { return glass::ready() && Glass::g != nullptr; }

	namespace
	{
		float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

		ImU32 ink_a(ImU32 c, float a)
		{
			const ImU32 ca = (ImU32)((((c >> 24) & 0xFF) * a) + 0.5f);
			return (c & 0x00FFFFFFu) | (ca << 24);
		}

		// rows ease in (fade + small rise). This is what animates the settings
		// in when a feature gets switched on: the revealed rows ease from 0.
		struct RowAnim { float a; float dy; };

		RowAnim row_anim(const char* key, float rise = 10.f)
		{
			Glass::Spring& sp = Glass::g->springs().Get((uint32_t)ImGui::GetID(key), 40,
			                                           Glass::SpringStyle::Overdamped, 0.f);
			sp.target = 1.f;
			sp.Tick(ImGui::GetIO().DeltaTime);
			const float a = clamp01(sp.x);
			return RowAnim{ a, (1.f - a) * rise };
		}

		// a row's geometry comes from the panel child's own rect - not from the
		// cursor - so every row spans the plate edge to edge no matter what the
		// tab did to the cursor before calling it
		struct RowBox { float left, right, width; };

		RowBox row_box(float pad = 10.f)
		{
			const ImVec2 wp = ImGui::GetWindowPos();
			const ImVec2 ws = ImGui::GetWindowSize();
			RowBox b;
			b.left = wp.x + pad;
			b.right = wp.x + ws.x - pad;
			b.width = (std::max)(1.f, b.right - b.left);
			return b;
		}

		// aligns the ImGui cursor to the row box so the hit area matches what
		// gets drawn
		ImVec2 row_begin(const RowBox& b)
		{
			ImVec2 c = ImGui::GetCursorScreenPos();
			c.x = b.left;
			ImGui::SetCursorScreenPos(c);
			return c;
		}

		// the feature switch: one full-width row, label on the left, the
		// LiquidUI pill on the right - this is how features turn on/off
		bool feature_toggle(const char* label, bool* v)
		{
			ImGui::PushID(label);
			const RowAnim an = row_anim("tg");
			const RowBox b = row_box();
			const float rowh = 36.f;
			ImVec2 cur = row_begin(b);
			ImGui::InvisibleButton("##tg", ImVec2(b.width, rowh));
			const bool hov = ImGui::IsItemHovered();
			const bool clicked = ImGui::IsItemDeactivated() && hov;
			if (clicked) *v = !*v;

			const ImGuiID id = ImGui::GetID("##tg");
			const float dt = ImGui::GetIO().DeltaTime;
			Glass::Spring& k = Glass::g->springs().Get((uint32_t)id, 2, Glass::SpringStyle::Critical, *v ? 1.f : 0.f);
			k.target = *v ? 1.f : 0.f;
			k.Tick(dt);
			Glass::Spring& hv = Glass::g->springs().Get((uint32_t)id, 3, Glass::SpringStyle::Critical, 0.f);
			hv.target = hov ? 1.f : 0.f;
			hv.Tick(dt);

			const float ry = cur.y + an.dy;

			// subtle hover plate behind the row
			if (hv.x > 0.01f)
			{
				Glass::Primitive p{};
				p.cx = b.left + b.width * 0.5f;
				p.cy = ry + rowh * 0.5f;
				p.hw = b.width * 0.5f;
				p.hh = rowh * 0.5f;
				p.corner_radius = 10.f;
				p.fade = hv.x * 0.55f * an.a;
				p.material = Glass::Material::Thin;
				Glass::g->Submit(p);
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 ts = ImGui::CalcTextSize(label ? label : "");
			dl->AddText(ImVec2(b.left, ry + (rowh - ts.y) * 0.5f),
			            ink_a(Glass::InkColor(), an.a), label ? label : "");

			const float sw = 46.f, sh = 26.f, sp = 3.f;
			const ImVec2 smin(b.right - sw, ry + (rowh - sh) * 0.5f);
			Glass::Primitive track{};
			track.cx = smin.x + sw * 0.5f;
			track.cy = smin.y + sh * 0.5f;
			track.hw = sw * 0.5f;
			track.hh = sh * 0.5f;
			track.corner_radius = sh * 0.5f;
			track.fade = an.a;
			track.material = Glass::Material::Thin;
			Glass::g->Submit(track);

			const float on = clamp01(k.x);
			if (on > 0.005f)
			{
				Glass::Primitive fill = track;
				fill.material = Glass::Material::Accent;
				fill.fade = on * an.a;
				Glass::g->Submit(fill);
			}

			const float kr = sh * 0.5f - sp;
			Glass::Primitive knob{};
			knob.cx = smin.x + sh * 0.5f + (sw - sh) * on;
			knob.cy = smin.y + sh * 0.5f;
			knob.hw = kr;
			knob.hh = kr;
			knob.corner_radius = kr;
			knob.fade = an.a;
			knob.material = Glass::Material::Knob;
			Glass::g->Submit(knob);

			ImGui::PopID();
			return clicked;
		}

		// one line of label + live value, thin track underneath - the way a
		// setting gets adjusted
		bool settings_slider(const char* label, float* v, float mn, float mx, const char* fmt)
		{
			ImGui::PushID(label);
			const float old = *v;
			const RowAnim an = row_anim("sl");
			const RowBox b = row_box();
			const float rowh = 46.f;
			const float kr = 7.f;
			ImVec2 cur = row_begin(b);
			ImGui::InvisibleButton("##sl", ImVec2(b.width, rowh));
			const bool act = ImGui::IsItemActive();
			const bool hov = ImGui::IsItemHovered();
			if (act)
			{
				float t = (ImGui::GetIO().MousePos.x - b.left - kr) / (std::max)(1.f, b.width - 2.f * kr);
				t = clamp01(t);
				*v = mn + t * (mx - mn);
			}

			const float ry = cur.y + an.dy;
			const ImU32 ink = ink_a(Glass::InkColor(), an.a);
			const ImU32 inksoft = ink_a(Glass::InkSoftColor(), an.a);

			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddText(ImVec2(b.left, ry + 4.f), ink, label ? label : "");

			char buf[64];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, (fmt && *fmt) ? fmt : "%.2f", *v);
			ImVec2 vs = ImGui::CalcTextSize(buf);
			dl->AddText(ImVec2(b.right - vs.x, ry + 4.f), act ? ink : inksoft, buf);

			const float ty = ry + rowh - 10.f;
			const float t = clamp01((mx != mn) ? ((*v - mn) / (mx - mn)) : 0.f);

			Glass::Primitive bg{};
			bg.cx = b.left + b.width * 0.5f;
			bg.cy = ty;
			bg.hw = b.width * 0.5f;
			bg.hh = 3.f;
			bg.corner_radius = 3.f;
			bg.fade = an.a;
			bg.material = Glass::Material::Thin;
			Glass::g->Submit(bg);

			if (t > 0.002f)
			{
				Glass::Primitive fl{};
				fl.hw = (b.width * 0.5f) * t;
				fl.cx = b.left + fl.hw;
				fl.cy = ty;
				fl.hh = 3.f;
				fl.corner_radius = 3.f;
				fl.fade = an.a;
				fl.material = Glass::Material::Accent;
				Glass::g->Submit(fl);
			}

			const float r = kr * (act ? 1.3f : (hov ? 1.12f : 1.f));
			Glass::Primitive kn{};
			kn.cx = b.left + kr + t * (b.width - 2.f * kr);
			kn.cy = ty;
			kn.hw = r;
			kn.hh = r;
			kn.corner_radius = r;
			kn.fade = an.a;
			kn.material = Glass::Material::Knob;
			Glass::g->Submit(kn);

			ImGui::PopID();
			return *v != old;
		}

		// value dropdown: label on the left, current value + chevron on the
		// right, a compact popup for picking
		bool settings_dropdown(const char* label, const char* const* items, int count, int* sel)
		{
			bool changed = false;
			ImGui::PushID(label);
			const RowAnim an = row_anim("dd");
			const RowBox b = row_box();
			const float rowh = 36.f;
			ImVec2 cur = row_begin(b);
			ImGui::InvisibleButton("##dd", ImVec2(b.width, rowh));
			const bool hov = ImGui::IsItemHovered();
			if (ImGui::IsItemDeactivated() && hov)
				ImGui::OpenPopup("##ddpop");

			const ImGuiID id = ImGui::GetID("##dd");
			Glass::Spring& hv = Glass::g->springs().Get((uint32_t)id, 21, Glass::SpringStyle::Critical, 0.f);
			hv.target = hov ? 1.f : 0.f;
			hv.Tick(ImGui::GetIO().DeltaTime);

			const float ry = cur.y + an.dy;

			if (hv.x > 0.01f)
			{
				Glass::Primitive p{};
				p.cx = b.left + b.width * 0.5f;
				p.cy = ry + rowh * 0.5f;
				p.hw = b.width * 0.5f;
				p.hh = rowh * 0.5f;
				p.corner_radius = 10.f;
				p.fade = hv.x * 0.55f * an.a;
				p.material = Glass::Material::Thin;
				Glass::g->Submit(p);
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 ts = ImGui::CalcTextSize(label ? label : "");
			dl->AddText(ImVec2(b.left, ry + (rowh - ts.y) * 0.5f),
			            ink_a(Glass::InkColor(), an.a), label ? label : "");

			const char* val = (sel && *sel >= 0 && *sel < count) ? items[*sel] : "-";
			ImVec2 vs = ImGui::CalcTextSize(val);
			const float cw = 10.f;
			dl->AddText(ImVec2(b.right - cw - 7.f - vs.x, ry + (rowh - vs.y) * 0.5f),
			            ink_a(Glass::InkSoftColor(), an.a), val);
			Glass::DrawIcon(dl, Glass::Icon::ChevronD,
			                ImVec2(b.right - cw * 0.5f, ry + rowh * 0.5f),
			                cw * 0.5f, ink_a(hov ? Glass::InkColor() : Glass::InkSoftColor(), an.a), 2.f);

			const float* ar = Glass::EditParams(Glass::Material::Accent).tint_rgb;
			const ImVec4 acc(ar[0], ar[1], ar[2], 0.96f);
			ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f, 0.075f, 0.09f, 0.985f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.08f));
			ImGui::PushStyleColor(ImGuiCol_Header, acc);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, acc);
			ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.f);
			ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
			if (ImGui::BeginPopup("##ddpop"))
			{
				const float pw = (std::max)(b.width, 150.f);
				for (int i = 0; i < count; ++i)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(items[i], sel && *sel == i, 0,
					                      ImVec2(pw, ImGui::GetTextLineHeight() + 8.f)))
					{
						if (sel && *sel != i) { *sel = i; changed = true; }
					}
					ImGui::PopID();
				}
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(4);
			ImGui::PopID();
			return changed;
		}

		// ------------------------------------------------------------------
		// VK keybind capture (mirrors widgets::keybind) wearing a LiquidUI
		// chip. The kit's own Keybind stores ImGuiKey values while our
		// settings keep VK codes, so the capture stays on the VK path and
		// only the visuals are glass.
		// ------------------------------------------------------------------
		bool    s_kb_ignore[256] = {};
		ImGuiID s_kb_waiting = 0;

		void kb_arm_ignore()
		{
			for (int k = 1; k < 256; ++k)
				s_kb_ignore[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
			s_kb_ignore[VK_LBUTTON] = true;
		}

		void kb_clear_ignore()
		{
			for (int k = 1; k < 256; ++k)
				s_kb_ignore[k] = false;
		}

		int kb_poll_bind_key()
		{
			for (int k = 1; k < 256; ++k)
			{
				if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU)
					continue;

				const bool down = (GetAsyncKeyState(k) & 0x8000) != 0;
				if (s_kb_ignore[k])
				{
					if (!down)
						s_kb_ignore[k] = false;
					continue;
				}
				if (down)
					return k;
			}
			return 0;
		}

		// draws the LiquidUI key chip at the current cursor; returns true
		// when *key changed
		bool glass_key_chip(int* key)
		{
			ImGui::PushID("##gchip");
			ImGuiID self = ImGui::GetID("##gchip");
			const bool listening = (s_kb_waiting == self);

			char buf[32];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "[%s]",
			            listening ? "..." : widgets::key_name(*key));
			ImVec2 ts = ImGui::CalcTextSize(buf);
			const float h = 30.f;
			const float w = ts.x + 26.f;

			ImVec2 cur = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##chip", ImVec2(w, h));

			const bool clicked = ImGui::IsItemDeactivated() && ImGui::IsItemHovered();
			if (clicked)
			{
				if (listening) { s_kb_waiting = 0; kb_clear_ignore(); }
				else           { s_kb_waiting = self; kb_arm_ignore(); }
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !listening)
				*key = 0;

			bool changed = false;
			if (listening)
			{
				if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
				{
					*key = 0;
					s_kb_waiting = 0;
					kb_clear_ignore();
					changed = true;
				}
				else
				{
					const int pressed = kb_poll_bind_key();
					if (pressed && pressed != VK_ESCAPE)
					{
						*key = pressed;
						s_kb_waiting = 0;
						kb_clear_ignore();
						changed = true;
					}
				}
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();
			if (Glass::g)
			{
				Glass::Primitive p{};
				p.cx = cur.x + w * 0.5f;
				p.cy = cur.y + h * 0.5f;
				p.hw = w * 0.5f;
				p.hh = h * 0.5f;
				p.corner_radius = h * 0.5f;
				p.fade = 1.f;
				p.material = listening ? Glass::Material::Accent : Glass::Material::Thin;
				Glass::g->Submit(p);
			}
			dl->AddText(ImVec2(cur.x + (w - ts.x) * 0.5f, cur.y + (h - ts.y) * 0.5f),
			            listening ? IM_COL32(255, 255, 255, 255) : Glass::InkColor(), buf);

			ImGui::PopID();
			return changed;
		}

		// key chip right-aligned inside a fixed-height row
		void glass_key_chip_row(const char* label, int* key, float row_w)
		{
			(void)row_w;                      // the row box owns the geometry
			const RowAnim an = row_anim("key");
			const RowBox b = row_box();
			const float rowh = 40.f;
			ImVec2 pos = row_begin(b);
			ImGui::Dummy(ImVec2(b.width, rowh));

			const float ry = pos.y + an.dy;

			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddText(ImVec2(b.left, ry + (rowh - ImGui::GetTextLineHeight()) * 0.5f),
			            ink_a(Glass::InkColor(), an.a), label ? label : "");

			char buf[32];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "[%s]", widgets::key_name(*key));
			const float chw = ImGui::CalcTextSize(buf).x + 26.f;
			ImGui::SetCursorScreenPos(ImVec2(b.right - chw, ry + (rowh - 30.f) * 0.5f));
			glass_key_chip(key);
			ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + rowh));
		}

		// the kit's ColorButton opens an rgb-only picker; our colors carry
		// alpha, so the glass color row keeps the row pattern (label left,
		// swatch right) and opens the existing rgba picker popup
		void glass_color_row(const char* label, float col[4])
		{
			ImGui::PushID(label);
			const RowAnim an = row_anim("col");
			const RowBox b = row_box();
			const float rowh = 36.f;
			ImVec2 pos = row_begin(b);
			ImGui::Dummy(ImVec2(b.width, rowh));

			const float ry = pos.y + an.dy;

			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 ts = ImGui::CalcTextSize(label ? label : "");
			dl->AddText(ImVec2(b.left, ry + (rowh - ts.y) * 0.5f),
			            ink_a(Glass::InkColor(), an.a), label ? label : "");

			const float sw = 46.f, sh = 26.f;
			ImVec2 smin(b.right - sw, ry + (rowh - sh) * 0.5f);
			ImVec2 smax(smin.x + sw, smin.y + sh);
			ImGui::SetCursorScreenPos(smin);
			ImGui::InvisibleButton("##swatch", ImVec2(sw, sh));
			const bool clicked = ImGui::IsItemDeactivated() && ImGui::IsItemHovered();
			if (clicked)
				ImGui::OpenPopup("##col_pop");

			dl->AddRectFilled(smin, smax,
				IM_COL32((int)(col[0] * 255.f), (int)(col[1] * 255.f),
				         (int)(col[2] * 255.f), (int)(col[3] * 255.f * an.a)), 8.f);
			dl->AddRect(smin, smax, IM_COL32(255, 255, 255, (int)(46 * an.a)), 8.f);

			ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + rowh));

			if (ImGui::BeginPopup("##col_pop"))
			{
				widgets::color_picker("##picker", col);
				ImGui::EndPopup();
			}
			ImGui::PopID();
		}
	}

	void pad()
	{
		if (glass_mode()) return;   // glass panels handle their own padding
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

	bool begin_panel(const char* id, float width, float height, bool scrollable)
	{
		if (glass_mode())
		{
			// same geometry as the legacy panel, but the chrome is a LiquidUI
			// Thin plate and the rows are kit widgets. The renderer clip keeps
			// glass primitives inside the panel (dropdown lists, scrolling...)
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
			ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.f);
			ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_None,
				ImGuiWindowFlags_NoScrollbar);
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();

			if (Glass::g)
			{
				// the plate takes the child's REAL rect, so it can never sit
				// offset from its own rows - a clamped/short child used to let
				// the plate stick out past them and look cut off
				const ImVec2 o = ImGui::GetWindowPos();
				const ImVec2 s = ImGui::GetWindowSize();
				Glass::Primitive p{};
				p.cx = o.x + s.x * 0.5f;
				p.cy = o.y + s.y * 0.5f;
				p.hw = s.x * 0.5f;
				p.hh = s.y * 0.5f;
				p.corner_radius = 15.f;        // the kit's panel radius
				p.fade = Glass::g->SubmitFadeValue();
				p.material = Glass::Material::Thin;
				Glass::g->Submit(p);

				Glass::g->SetClipRect(o.x, o.y, o.x + s.x, o.y + s.y);
			}

			float content_w = width - content_pad * 2.f;
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.f, 9.f));
			ImGui::SetCursorPos(ImVec2(content_pad, content_pad));
			ImGui::PushItemWidth(content_w);
			return true;
		}

		ImGui::PushStyleColor(ImGuiCol_Border, k_border);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, scrollable ? 3.f : 0.f);
		ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_Borders,
			scrollable ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleVar(2);

		float content_w = ImGui::GetWindowSize().x - content_pad * 2.f;
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, content_spacing));
		ImGui::SetCursorPos(ImVec2(content_pad, content_pad));
		ImGui::PushItemWidth(content_w);
		return true;
	}

	void end_panel()
	{
		if (glass_mode())
		{
			ImGui::PopItemWidth();
			ImGui::PopStyleVar();
			ImGui::EndChild();
			if (Glass::g)
				Glass::g->ClearClipRect();
			return;
		}

		ImGui::PopItemWidth();
		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	void row_keybind(const char* id, const char* label, int* key, int* mode)
	{
		if (!key || !mode) return;

		if (glass_mode())
		{
			ImGui::PushID(id);
			const float row_w = (std::min)(ImGui::GetContentRegionAvail().x, 320.f);
			glass_key_chip_row(label, key, row_w);

			static const std::vector<const char*> k_modes = { "hold", "toggle", "always" };
			if (*mode < 0) *mode = 0;
			if (*mode > 2) *mode = 2;
			settings_dropdown("mode", k_modes.data(), 3, mode);
			ImGui::PopID();
			return;
		}

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

		if (glass_mode())
		{
			ImGui::PushID(id);
			const float row_w = (std::min)(ImGui::GetContentRegionAvail().x, 320.f);
			glass_key_chip_row(label, key, row_w);
			ImGui::PopID();
			return;
		}

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
		if (glass_mode())
		{
			ImGui::PushID(label);
			const int old = *cur;
			settings_dropdown(label, items.data(), (int)items.size(), cur);
			ImGui::PopID();
			return old != *cur;
		}

		pad();
		return widgets::combo(label, cur, items);
	}

	bool row_slider_f(const char* label, float* v, float mn, float mx, const char* fmt)
	{
		if (glass_mode())
		{
			ImGui::PushID(label);
			const bool changed = settings_slider(label, v, mn, mx, fmt);
			ImGui::PopID();
			return changed;
		}

		pad();
		return widgets::slider_float(label, v, mn, mx, fmt);
	}

	bool row_slider_i(const char* label, int* v, int mn, int mx)
	{
		if (glass_mode())
		{
			ImGui::PushID(label);
			float t = (float)*v;
			settings_slider(label, &t, (float)mn, (float)mx, "%.0f");
			const int nv = (int)t;
			if (nv != *v) { *v = nv; return true; }
			return false;
		}

		pad();
		return widgets::slider_int(label, v, mn, mx);
	}

	bool row_checkbox(const char* label, bool* v)
	{
		if (glass_mode())
			return feature_toggle(label, v);

		pad();
		return widgets::checkbox(label, v);
	}

	bool row_checkbox_color(const char* label, bool* v, float col[4])
	{
		if (glass_mode())
		{
			const bool old = *v;
			feature_toggle(label, v);
			if (*v)
				glass_color_row("color", col);
			return old != *v;
		}

		pad();
		return widgets::checkbox_color(label, v, col);
	}

	bool row_checkbox_keybind(const char* label, bool* v, int* key)
	{
		if (glass_mode())
		{
			const bool old = *v;
			ImGui::PushID(label);
			feature_toggle(label, v);
			if (*v)
			{
				const float row_w = (std::min)(ImGui::GetContentRegionAvail().x, 320.f);
				glass_key_chip_row("key", key, row_w);
			}
			ImGui::PopID();
			return old != *v;
		}

		pad();
		return widgets::checkbox_keybind(label, v, key);
	}

	bool row_multicombo(const char* id, const char* label, bool* selected, const std::vector<const char*>& items)
	{
		if (glass_mode())
		{
			bool changed = false;
			if (label && *label)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, Glass::InkColor());
				ImGui::TextUnformatted(label);
				ImGui::PopStyleColor();
			}

			ImGui::PushID(id);
			const float x0 = ImGui::GetCursorScreenPos().x;
			const float avail_w = ImGui::GetContentRegionAvail().x;
			bool first_on_line = true;
			for (int i = 0; i < (int)items.size(); ++i)
			{
				const float cw = ImGui::CalcTextSize(items[i]).x + 26.f;
				const bool need_wrap = !first_on_line &&
					(ImGui::GetItemRectMax().x + 6.f + cw > x0 + avail_w);
				if (!first_on_line && !need_wrap)
					ImGui::SameLine(0.f, 6.f);
				// on wrap the cursor is already on the next line
				if (Glass::Chip(items[i], selected[i]))
				{
					selected[i] = !selected[i];
					changed = true;
				}
				first_on_line = false;
			}
			ImGui::Dummy(ImVec2(0.01f, 0.01f));
			ImGui::PopID();
			return changed;
		}

		pad();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		widgets::text_outlined(dl, pos, ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.85f, 1.f)), label ? label : "");
		ImGui::Dummy(ImVec2(0.f, ImGui::GetFontSize() + 2.f));
		pad();
		return widgets::multicombo(id, selected, items);
	}

	// label with a color swatch on the right — clicking the swatch opens the
	// color picker (sv square + hue bar + alpha bar)
	bool row_color(const char* label, float col[4])
	{
		if (!col) return false;

		if (glass_mode())
		{
			glass_color_row(label, col);
			return false;
		}

		pad();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		float avail_w = ImGui::CalcItemWidth();
		if (avail_w < 1.f)
			avail_w = ImGui::GetContentRegionAvail().x;

		ImVec2 ts = ImGui::CalcTextSize(label ? label : "");
		ImGui::Dummy(ImVec2(avail_w, ts.y));
		ImDrawList* dl = ImGui::GetWindowDrawList();
		widgets::text_outlined(dl, pos, ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.85f, 1.f)), label ? label : "");

		constexpr float swatch_w = 30.f;
		ImVec2 smin(pos.x + avail_w - swatch_w, pos.y);
		ImVec2 smax(smin.x + swatch_w, pos.y + ts.y);

		ImGui::SetCursorScreenPos(smin);
		ImGui::PushID(label ? label : "##color");
		bool changed = false;
		if (ImGui::InvisibleButton("##swatch", ImVec2(swatch_w, ts.y)))
			ImGui::OpenPopup("##color_pop");

		dl->AddRectFilled(smin, smax, ImGui::GetColorU32(ImVec4(col[0], col[1], col[2], 1.f)), 4.f);
		dl->AddRect(smin, smax, IM_COL32(255, 255, 255, 46), 4.f);

		if (ImGui::BeginPopup("##color_pop"))
		{
			changed = widgets::color_picker("##picker", col);
			ImGui::EndPopup();
		}
		ImGui::PopID();
		return changed;
	}

	// -------------------------------------------------------------------------
	// plain widgets the settings/local pages use directly
	// -------------------------------------------------------------------------
	bool row_button(const char* label)
	{
		if (glass_mode())
			return Glass::Button(label ? label : "##btn", false);

		return widgets::button(label);
	}

	bool row_input_text(const char* label, char* buf, int len)
	{
		if (glass_mode())
			return Glass::TextField(label ? label : "##field", buf, len);

		return widgets::input_text(label, buf, len);
	}
}
