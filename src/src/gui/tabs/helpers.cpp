#include "pch.h"
#include "helpers.h"
#include "../widgets/widgets.h"
#include "../widgets/text.h"
#include "../glass.h"
#include "../liquid_ui.h"
#include "imgui.h"
#include <cstdio>
#include <vector>
#include <unordered_map>
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

	// Rows are normally highlighted with a glass hover plate. Inside a *solid*
	// popup (the profile menu) that plate would be invisible: the whole glass
	// pass runs before any ImGui geometry, so the popup's own background paints
	// over it. Rows then draw their hover with ImGui instead.
	static bool s_imgui_chrome = false;
	void rows_use_imgui_chrome(bool on) { s_imgui_chrome = on; }

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

		RowBox row_box(float pad = content_pad)
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
		// ------------------------------------------------------------------
		// row chrome. Every control is one row: the label sits at the left
		// edge of the box, its control is right aligned in a shared control
		// column, and a hairline closes the row - the reference layout.
		// ------------------------------------------------------------------
		struct Row
		{
			RowBox box;
			ImVec2 pos{};        // un-animated row origin
			float  y        = 0.f;   // animated row top
			float  h        = k_row_h;
			float  a        = 1.f;   // reveal animation
			float  hover    = 0.f;
			float  ctl_x    = 0.f;   // left edge of the control column
			float  ctl_w    = 0.f;
			bool   hovered  = false;
			bool   active   = false;
			bool   clicked  = false;
		};

		ImU32 lerp_col(ImU32 a, ImU32 b, float t)
		{
			t = clamp01(t);
			const float ia = 1.f - t;
			const int r = (int)(((a >> IM_COL32_R_SHIFT) & 0xFF) * ia + ((b >> IM_COL32_R_SHIFT) & 0xFF) * t);
			const int g = (int)(((a >> IM_COL32_G_SHIFT) & 0xFF) * ia + ((b >> IM_COL32_G_SHIFT) & 0xFF) * t);
			const int bl = (int)(((a >> IM_COL32_B_SHIFT) & 0xFF) * ia + ((b >> IM_COL32_B_SHIFT) & 0xFF) * t);
			const int al = (int)(((a >> IM_COL32_A_SHIFT) & 0xFF) * ia + ((b >> IM_COL32_A_SHIFT) & 0xFF) * t);
			return IM_COL32(r, g, bl, al);
		}

		// the accent the LiquidUI kit currently uses (accent palette swatch)
		ImU32 accent_col(float a)
		{
			const float* c = Glass::EditParams(Glass::Material::Accent).tint_rgb;
			return IM_COL32((int)(c[0] * 255.f), (int)(c[1] * 255.f), (int)(c[2] * 255.f),
			                (int)(255.f * clamp01(a)));
		}

		Row row_begin(const char* label, float h, float ctl_w, const char* anim_key)
		{
			Row r{};
			ImGui::PushID(label ? label : "row");
			r.box = row_box();
			r.h = h;
			r.ctl_w = ctl_w;
			r.ctl_x = r.box.right - ctl_w;

			const RowAnim an = row_anim(anim_key);
			r.a = an.a;

			r.pos = row_begin(r.box);
			// AllowOverlap: the row's own hit area spans the full width, so
			// without this the *later* widgets that sit on top of it (the
			// keybind chip, the colour swatch) can never take hover - which is
			// why the keybind chips would not react to a click
			ImGui::SetNextItemAllowOverlap();
			ImGui::InvisibleButton("##row", ImVec2(r.box.width, h));
			r.hovered = ImGui::IsItemHovered();
			r.active = ImGui::IsItemActive();
			r.clicked = ImGui::IsItemDeactivated() && r.hovered;
			r.y = r.pos.y + an.dy;

			const ImGuiID id = ImGui::GetID("##row");
			Glass::Spring& hv = Glass::g->springs().Get((uint32_t)id, 3, Glass::SpringStyle::Critical, 0.f);
			hv.target = (r.hovered || r.active) ? 1.f : 0.f;
			hv.Tick(ImGui::GetIO().DeltaTime);
			r.hover = clamp01(hv.x);

			// soft hover plate behind the row
			if (hv.x > 0.01f)
			{
				if (s_imgui_chrome)
				{
					ImGui::GetWindowDrawList()->AddRectFilled(
						ImVec2(r.box.left, r.y), ImVec2(r.box.right, r.y + h),
						IM_COL32(255, 255, 255, (int)(26.f * hv.x * r.a)), 10.f);
				}
				else
				{
					Glass::Primitive p{};
					p.cx = r.box.left + r.box.width * 0.5f;
					p.cy = r.y + h * 0.5f;
					p.hw = r.box.width * 0.5f;
					p.hh = h * 0.5f;
					p.corner_radius = 10.f;
					p.fade = hv.x * 0.5f * r.a;
					p.material = Glass::Material::Thin;
					Glass::g->Submit(p);
				}
			}

			if (label && *label)
				ImGui::GetWindowDrawList()->AddText(
					ImVec2(r.box.left, r.y + (h - ImGui::GetTextLineHeight()) * 0.5f),
					ink_a(Glass::InkColor(), r.a), label);

			return r;
		}

		// closes the row: hairline along the bottom edge, cursor onto the next
		void row_end(const Row& r)
		{
			if (r.a > 0.01f)
				ImGui::GetWindowDrawList()->AddLine(
					ImVec2(r.box.left - content_pad + 2.f, r.y + r.h - 1.f),
					ImVec2(r.box.right + content_pad - 2.f, r.y + r.h - 1.f),
					IM_COL32(255, 255, 255, (int)(12.f * r.a)), 1.f);

			ImGui::SetCursorScreenPos(ImVec2(r.pos.x, r.pos.y + r.h));
			ImGui::Dummy(ImVec2(0.01f, 0.01f));
			ImGui::PopID();
		}

		// the feature switch: label left, pill switch right. `more` marks a
		// row that owns sub-options (the reference draws "..." beside it);
		// when `open` is given the marker is clickable and toggles it, so a
		// row can carry hidden sub-options even while it is switched on
		bool feature_toggle(const char* label, bool* v, bool more, bool* open = nullptr)
		{
			const bool dots = more || (open != nullptr);
			const float ctl_w = k_switch_w + (dots ? 26.f : 0.f);
			Row r = row_begin(label, k_row_h, ctl_w, "tg");

			const float sw = k_switch_w, sh = k_switch_h, sp = 3.f;
			const float sx = r.box.right - sw;
			const float sy = r.y + (r.h - sh) * 0.5f;

			bool on_dots = false;
			if (open && r.clicked)
			{
				const ImVec2 d0(sx - 26.f, r.y + 4.f), d1(sx - 4.f, r.y + r.h - 4.f);
				on_dots = ImGui::IsMouseHoveringRect(d0, d1);
			}
			if (r.clicked)
			{
				if (on_dots) *open = !*open;
				else         *v = !*v;
			}

			const ImGuiID id = ImGui::GetID("##row");
			Glass::Spring& k = Glass::g->springs().Get((uint32_t)id, 2, Glass::SpringStyle::Critical, *v ? 1.f : 0.f);
			k.target = *v ? 1.f : 0.f;
			k.Tick(ImGui::GetIO().DeltaTime);
			const float on = clamp01(k.x);

			ImDrawList* dl = ImGui::GetWindowDrawList();
			// the track fills with the accent when on; the knob is a crisp
			// white puck drawn by ImGui rather than a glass primitive - a glass
			// knob sitting on an accent track blended into it and the switch
			// read as one solid blue slab with no visible thumb
			const ImU32 track = lerp_col(IM_COL32(64, 64, 72, (int)(215 * r.a)), accent_col(r.a), on);
			dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + sw, sy + sh), track, sh * 0.5f);
			dl->AddRect(ImVec2(sx, sy), ImVec2(sx + sw, sy + sh),
			            IM_COL32(255, 255, 255, (int)(34 * r.a)), sh * 0.5f);

			// the reference's switch keeps its thumb at the RIGHT end in both
			// states - the track colour alone carries on/off
			const ImVec2 kc(sx + sw - sh * 0.5f, sy + sh * 0.5f);
			const float kr = sh * 0.5f - sp;
			dl->AddCircleFilled(ImVec2(kc.x, kc.y + 1.f), kr, IM_COL32(0, 0, 0, (int)(70 * r.a)), 24);
			dl->AddCircleFilled(kc, kr, ink_a(IM_COL32(245, 245, 248, 255), r.a), 24);

			if (dots)
			{
				const float cx = sx - 20.f, cy = sy + sh * 0.5f;
				for (int i = -1; i <= 1; ++i)
					dl->AddCircleFilled(ImVec2(cx, cy + i * 5.f), 1.5f,
					                    IM_COL32(170, 170, 182, (int)(220 * r.a)));
			}

			row_end(r);
			return r.clicked;
		}

		// the reference's expandable rows: label left, chevron right, plus the
		// white rounded square for rows that own a master switch. The square
		// toggles the feature, anywhere else on the row expands it.
		bool expand_row_impl(const char* label, bool* open, bool* check)
		{
			const float ctl_w = check ? (16.f + 22.f + 8.f) : 22.f;
			Row r = row_begin(label, k_row_h, ctl_w, "ex");

			const float bs = 16.f;
			const float bx = r.box.right - 22.f - bs;
			const ImVec2 mn(bx, r.y + (r.h - bs) * 0.5f);
			const ImVec2 mx(bx + bs, mn.y + bs);

			if (r.clicked)
			{
				if (check && ImGui::IsMouseHoveringRect(mn, mx))
					*check = !*check;
				else if (open)
					*open = !*open;
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();

			if (check)
			{
				if (*check)
					dl->AddRectFilled(mn, mx, ink_a(IM_COL32(238, 239, 245, 255), r.a), 4.f);
				else
				{
					dl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, (int)(26 * r.a)), 4.f);
					dl->AddRect(mn, mx, IM_COL32(255, 255, 255, (int)(70 * r.a)), 4.f);
				}
			}

			Glass::DrawIcon(dl, Glass::Icon::ChevronR,
			                ImVec2(r.box.right - 8.f, r.y + r.h * 0.5f), 5.f,
			                ink_a(Glass::InkSoftColor(), r.a), 2.f);

			row_end(r);
			return open ? *open : false;
		}

		// one line of label + live value, thin track underneath - the way a
		// setting gets adjusted
		// small plate holding the current value - the reference shows the
		// number in a box at the end of the row
		void value_box(ImVec2 mn, ImVec2 mx, const char* text, float a, bool hot)
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, (int)((hot ? 28.f : 16.f) * a)), 8.f);
			dl->AddRect(mn, mx, IM_COL32(255, 255, 255, (int)((hot ? 44.f : 26.f) * a)), 8.f);
			const ImVec2 ts = ImGui::CalcTextSize(text);
			dl->AddText(ImVec2(mn.x + ((mx.x - mn.x) - ts.x) * 0.5f,
			                   mn.y + ((mx.y - mn.y) - ts.y) * 0.5f),
			            ink_a(Glass::InkColor(), a), text);
		}

		// the settings slider: label left, thin accent track, value box right
		bool settings_slider(const char* label, float* v, float mn, float mx, const char* fmt)
		{
			const float ctl_w = k_box_w + 14.f + k_track_w;
			Row r = row_begin(label, k_row_h, ctl_w, "sl");
			const float old = *v;

			const float bx = r.box.right - k_box_w;           // value box left
			const float tx = bx - 14.f - k_track_w;           // track left
			const float ty = r.y + r.h * 0.5f;
			const float kr = 6.f;

			if (r.active || r.clicked)
			{
				float t = (ImGui::GetIO().MousePos.x - tx) / (std::max)(1.f, k_track_w);
				*v = mn + clamp01(t) * (mx - mn);
			}

			const float t = clamp01((mx != mn) ? ((*v - mn) / (mx - mn)) : 0.f);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(ImVec2(tx, ty - 2.f), ImVec2(tx + k_track_w, ty + 2.f),
			                  IM_COL32(255, 255, 255, (int)(36 * r.a)), 2.f);
			if (t > 0.002f)
				dl->AddRectFilled(ImVec2(tx, ty - 2.f), ImVec2(tx + k_track_w * t, ty + 2.f),
				                  accent_col(r.a), 2.f);

			const ImVec2 kc(tx + k_track_w * t, ty);
			const float krr = kr * (r.active ? 1.3f : (r.hovered ? 1.12f : 1.f));
			dl->AddCircleFilled(ImVec2(kc.x, kc.y + 1.f), krr, IM_COL32(0, 0, 0, (int)(70 * r.a)), 20);
			dl->AddCircleFilled(kc, krr, ink_a(IM_COL32(245, 245, 248, 255), r.a), 20);

			char buf[64];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, (fmt && *fmt) ? fmt : "%.2f", *v);
			value_box(ImVec2(bx, ty - k_ctl_h * 0.5f), ImVec2(bx + k_box_w, ty + k_ctl_h * 0.5f),
			          buf, r.a, r.hovered || r.active);

			row_end(r);
			return *v != old;
		}

		// value dropdown: label on the left, current value + chevron on the
		// right, a compact popup for picking
		// value dropdown: label left, current value in a box on the right,
		// popup list underneath it
		// `open` marks a row that owns sub-options: it reserves the reference's
		// "..." column to the left of the box, and that column expands the row
		// while clicking the box itself still opens the list
		bool settings_dropdown(const char* label, const char* const* items, int count, int* sel,
		                       bool* open = nullptr)
		{
			bool changed = false;
			const char* cur_val = (sel && *sel >= 0 && *sel < count) ? items[*sel] : "-";
			const float dots_w = open ? 26.f : 0.f;
			Row r = row_begin(label, k_row_h, k_drop_w + dots_w, "dd");

			ImDrawList* dl = ImGui::GetWindowDrawList();

			bool on_dots = false;
			if (open && r.clicked)
			{
				const ImVec2 d0(r.box.right - k_drop_w - 26.f, r.y + 4.f);
				const ImVec2 d1(r.box.right - k_drop_w - 4.f, r.y + r.h - 4.f);
				on_dots = ImGui::IsMouseHoveringRect(d0, d1);
			}
			if (r.clicked)
			{
				if (on_dots) *open = !*open;
				else         ImGui::OpenPopup("##ddpop");
			}
			const ImVec2 mn(r.box.right - k_drop_w, r.y + (r.h - k_drop_h) * 0.5f);
			const ImVec2 mx(mn.x + k_drop_w, mn.y + k_drop_h);
			// Matcha's dropdown: value left aligned with padding, chevron hard
			// right - not the centred value box the other controls share
			{
				const bool hot = r.hovered || ImGui::IsPopupOpen("##ddpop");
				dl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, (int)((hot ? 28.f : 16.f) * r.a)), 8.f);
				dl->AddRect(mn, mx, IM_COL32(255, 255, 255, (int)((hot ? 44.f : 26.f) * r.a)), 8.f);

				const ImVec2 ts = ImGui::CalcTextSize(cur_val);
				dl->AddText(ImVec2(mn.x + 13.f, mn.y + (k_drop_h - ts.y) * 0.5f),
				            ink_a(Glass::InkColor(), r.a), cur_val);
			}
			Glass::DrawIcon(dl, Glass::Icon::ChevronD, ImVec2(mx.x - 13.f, mn.y + k_drop_h * 0.5f),
			                5.f, ink_a(Glass::InkSoftColor(), r.a), 2.f);

			if (open)
			{
				const float cx = r.box.right - k_drop_w - 20.f, cy = r.y + r.h * 0.5f;
				for (int i = -1; i <= 1; ++i)
					dl->AddCircleFilled(ImVec2(cx, cy + i * 5.f), 1.5f,
					                    IM_COL32(170, 170, 182, (int)(220 * r.a)));
			}

			ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f, 0.075f, 0.09f, 0.985f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.08f));
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1.f, 1.f, 1.f, 0.10f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.f, 1.f, 1.f, 0.16f));
			ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.f);
			ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
			ImGui::SetNextWindowPos(ImVec2(mn.x, mx.y + 4.f));
			if (ImGui::BeginPopup("##ddpop"))
			{
				const float pw = (std::max)(k_drop_w, 150.f);
				for (int i = 0; i < count; ++i)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(items[i], sel && *sel == i, 0,
					                      ImVec2(pw, ImGui::GetTextLineHeight() + 8.f)))
					{
						if (sel && *sel != i) { *sel = i; changed = true; }
						ImGui::CloseCurrentPopup();
					}
					ImGui::PopID();
				}
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(4);

			row_end(r);
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

		// key chip right-aligned inside a row
		void glass_key_chip_row(const char* label, int* key, float row_w)
		{
			(void)row_w;                      // the row box owns the geometry
			char buf[32];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "[%s]", widgets::key_name(*key));
			const float chw = ImGui::CalcTextSize(buf).x + 26.f;

			Row r = row_begin(label, k_row_h, chw, "key");
			ImGui::SetCursorScreenPos(ImVec2(r.ctl_x, r.y + (r.h - 30.f) * 0.5f));
			glass_key_chip(key);
			row_end(r);
		}

		// color row: label left, swatch right, opens the rgba picker
		void glass_color_row(const char* label, float col[4])
		{
			Row r = row_begin(label, k_row_h, k_swatch_w, "col");

			const float sw = k_swatch_w, sh = k_swatch_w;
			const ImVec2 smin(r.box.right - sw, r.y + (r.h - sh) * 0.5f);
			const ImVec2 smax(smin.x + sw, smin.y + sh);
			ImGui::SetCursorScreenPos(smin);
			ImGui::InvisibleButton("##swatch", ImVec2(sw, sh));
			if (ImGui::IsItemDeactivated() && ImGui::IsItemHovered())
				ImGui::OpenPopup("##col_pop");

			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(smin, smax,
				IM_COL32((int)(col[0] * 255.f), (int)(col[1] * 255.f),
				         (int)(col[2] * 255.f), (int)(col[3] * 255.f * r.a)), 7.f);
			dl->AddRect(smin, smax, IM_COL32(255, 255, 255, (int)((r.hovered ? 70.f : 52.f) * r.a)), 7.f, 0, 1.f);

			row_end(r);

			if (ImGui::BeginPopup("##col_pop"))
			{
				widgets::color_picker("##picker", col);
				ImGui::EndPopup();
			}
		}
	}

	// exported wrapper - the implementation lives in the anonymous namespace above
	bool expand_row(const char* label, bool* open, bool* check)
	{
		return expand_row_impl(label, open, check);
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
			return;
		}

		ImGui::PopItemWidth();
		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}
	// -------------------------------------------------------------------------
	// page layout (the reference structure): a page is two columns, and each
	// column is a stack of "SECTION header + box" pairs that size themselves
	// to their rows
	// -------------------------------------------------------------------------
	static std::vector<size_t> s_section_stack;

	void begin_column(const char* id, float width, float height)
	{
		// a thin, unobtrusive scrollbar: a page whose sections are taller than
		// the card scrolls instead of spilling past the bottom of the menu
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(1.f, 1.f, 1.f, 0.16f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(1.f, 1.f, 1.f, 0.26f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(1.f, 1.f, 1.f, 0.34f));

		if (glass_mode())
		{
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
			ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.f);
			ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_None,
				ImGuiWindowFlags_NoBackground);
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
			return;
		}

		ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_None, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 4.f));
	}

	void end_column()
	{
		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::PopStyleColor(4);   // scrollbar colours
	}

	// "MAIN", "SELECTION", ... - small, letter spaced, hairline underneath
	void section_header(const char* text)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 o = ImGui::GetCursorScreenPos();
		const float w = ImGui::GetContentRegionAvail().x;

		if (!glass_mode())
		{
			ImGui::TextUnformatted(text);
			ImGui::Separator();
			return;
		}

		const float fs = ImGui::GetFontSize() * 0.82f;
		float x = o.x;
		for (const char* p = text; *p; ++p)
		{
			char ch[2] = { (char)toupper((unsigned char)*p), 0 };
			dl->AddText(ImGui::GetFont(), fs, ImVec2(x, o.y), Glass::InkSoftColor(), ch);
			x += ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.f, ch).x + 1.f;
		}
		// the reference has no rule under its section labels, just the label
		ImGui::Dummy(ImVec2(w, 24.f));
	}

	// a box around its rows; the plate is submitted up front and its geometry
	// filled in when the box closes (so it always matches the real content)
	bool begin_section(const char* id)
	{
		if (!glass_mode())
			return begin_panel(id, ImGui::GetContentRegionAvail().x, 0.f, false);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(content_pad, 6.f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::BeginChild(id, ImVec2(0.f, 0.f),
			ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
			ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleColor();

		size_t idx = (size_t)-1;
		if (Glass::g)
		{
			Glass::Primitive p{};
			// the section plate is deliberately faint: the reference's boxes are
			// only a hair lighter than the card behind them
			// the reference's boxes are clearly a backdrop behind the rows - they
			// read as a distinct plate over the card, not a hairline
			p.fade = Glass::g->SubmitFadeValue() * 0.55f;
			p.material = Glass::Material::Thin;
			p.corner_radius = 10.f;
			// geometry is filled in by end_section(); the scissor is
			// per-primitive now (Renderer::Submit clips every primitive to the
			// rect its own window/child may paint into), so the old global
			// SetClipRect call here - which was last-write-wins for the whole
			// frame - is gone
			idx = Glass::g->Submit(p);
		}
		s_section_stack.push_back(idx);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
		ImGui::PushItemWidth(ImGui::GetWindowSize().x - content_pad * 2.f);
		return true;
	}

	void end_section()
	{
		if (!glass_mode())
		{
			end_panel();
			return;
		}

		const ImVec2 o = ImGui::GetWindowPos();
		const ImVec2 s = ImGui::GetWindowSize();
		if (!s_section_stack.empty())
		{
			const size_t idx = s_section_stack.back();
			s_section_stack.pop_back();
			if (Glass::g && idx != (size_t)-1)
			{
				Glass::Primitive& p = Glass::g->At(idx);
				p.cx = o.x + s.x * 0.5f;
				p.cy = o.y + s.y * 0.5f;
				p.hw = s.x * 0.5f;
				p.hh = s.y * 0.5f;
			}
		}

		ImGui::PopStyleVar();          // item spacing
		ImGui::PopItemWidth();
		ImGui::EndChild();
		ImGui::PopStyleVar();          // window padding

		ImGui::Dummy(ImVec2(0.f, 10.f));   // gap before the next section header
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

	// same dropdown, flagged as owning sub-options (the "..." marker expands `open`)
	bool row_combo_more(const char* label, int* cur, const std::vector<const char*>& items, bool* open)
	{
		if (glass_mode())
		{
			ImGui::PushID(label);
			const int old = *cur;
			settings_dropdown(label, items.data(), (int)items.size(), cur, open);
			ImGui::PopID();
			return old != *cur;
		}

		// no markers in the ImGui fallback path - `open` is left alone so a row
		// that was already expanded stays reachable if the glass renderer fails
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
			return feature_toggle(label, v, false);

		pad();
		return widgets::checkbox(label, v);
	}

	// same row, but flagged as owning sub-options (draws the "..." marker)
	bool row_checkbox_more(const char* label, bool* v)
	{
		if (glass_mode())
			return feature_toggle(label, v, true);

		pad();
		return widgets::checkbox(label, v);
	}

	// the marker itself toggles `open` instead of the switch
	bool row_checkbox_expand(const char* label, bool* v, bool* open)
	{
		if (glass_mode())
			return feature_toggle(label, v, true, open);

		if (open) *open = false;   // no markers in the fallback path
		pad();
		return widgets::checkbox(label, v);
	}

	bool row_checkbox_color(const char* label, bool* v, float col[4])
	{
		if (glass_mode())
		{
			const bool old = *v;
			feature_toggle(label, v, false);
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
			feature_toggle(label, v, false);
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

	// muted caption row - used for empty states and page notes
	void row_note(const char* text)
	{
		if (glass_mode())
		{
			const Row r = row_begin(nullptr, k_row_h, 0.f, "note");
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(r.box.left, r.y + (r.h - ImGui::GetTextLineHeight()) * 0.5f),
				ink_a(Glass::InkSoftColor(), r.a), text ? text : "");
			row_end(r);
			return;
		}

		pad();
		ImGui::TextUnformatted(text ? text : "");
	}

	// a feature that does not exist yet: the row and its switch are real and the
	// state is kept per row, nothing else consumes it
	bool row_placeholder(const char* label)
	{
		static std::unordered_map<unsigned int, bool> s_ph;

		const unsigned int key = (unsigned int)ImGui::GetID(label);
		bool& v = s_ph[key];
		if (glass_mode())
			return feature_toggle(label, &v, false);

		pad();
		return widgets::checkbox(label, &v);
	}
}
