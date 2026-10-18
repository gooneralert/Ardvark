#include "pch.h"
#include "helpers.h"
#include "../widgets/widgets.h"
#include "../widgets/text.h"
#include "helpers.h"
#include "../glass.h"
#include "../menu_icons.h"   // rail/row chevrons drawn as geometry
#include "../liquid_ui.h"
#include "../resources/fonts/fonts.h"   // fonts::menu_regular() - the menu face
#include "app/Settings.h"    // Cheat::g_Settings.gui.accent - the live accent
#include "../widgets/widgets.h"
#include "../widgets/text.h"
#include "imgui.h"
#include <cstdio>
#include <cmath>
#include "imgui.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <windows.h>

namespace ng_tabs
{
	static const ImVec4 k_border = ImVec4(0.18f, 0.18f, 0.18f, 1.f);

	// -------------------------------------------------------------------------
	// Reference-styled rows. The main menu now renders in its own window (see
	// gui/menu_window.cpp + gui/matcha/matcha_menu.cpp) instead of inside the
	// overlay's LiquidUI glass pass, so this kit is fully self-contained: the
	// accent comes from the settings, the hover/reveal motion is a small local
	// spring, and the ink colours are the reference's fixed greys. Nothing here
	// touches the LiquidUI renderer any more.
	// -------------------------------------------------------------------------
	struct Spring { float x = 0.f; float target = 0.f; };

	Spring& spring_for(ImGuiID id, int order, float init)
	{
		static std::unordered_map<uint64_t, Spring> pool;
		const uint64_t key = (uint64_t)id * 131u + (uint64_t)(order + 1);
		auto it = pool.find(key);
		if (it == pool.end())
		{
			Spring s; s.x = init; s.target = init;
			it = pool.emplace(key, s).first;
		}
		return it->second;
	}

	void spring_tick(Spring& s, float speed = 13.f)
	{
		float dt = ImGui::GetIO().DeltaTime;
		if (dt <= 0.f || dt > 0.1f) dt = 1.f / 60.f;
		s.x += (s.target - s.x) * (1.f - std::exp(-speed * dt));
		if (std::fabs(s.target - s.x) < 0.002f) s.x = s.target;
	}

	inline ImU32 ink()      { return IM_COL32(236, 237, 243, 255); }
	inline ImU32 ink_soft() { return IM_COL32(151, 154, 168, 255); }

	inline const float* accent_rgb() { return Cheat::g_Settings.gui.accent; }

	// The pages only ever render in the menu window, so the reference-styled
	// rows are always used (the legacy ImGui fallback path stays compiled in
	// for the non-menu callers, it is just never taken any more).
	bool glass_mode() { return true; }

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
			Spring& sp = spring_for(ImGui::GetID(key), 40, 0.f);
			sp.target = 1.f;
			spring_tick(sp, 9.f);
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
			float  on       = 0.f;   // master-switch state (0..1, springed)
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

		// The reference paints every "on" control with a two-stop gradient, not
		// a flat colour: violet at the left edge through to magenta at the
		// right. The second stop is derived from the live accent so switching
		// the accent swatch keeps the gradient shape (see MATCHA_UI_SPEC §2).
		ImU32 accent_grad(float t, float a)
		{
            const float* c = Glass::g ? Glass::EditParams(Glass::Material::Accent).tint_rgb : accent_rgb();
			const float e[3] = { clamp01(c[0] * 1.35f), clamp01(c[1] * 0.55f), clamp01(c[2] * 0.95f) };
			t = clamp01(t);
			const float ia = 1.f - t;
			return IM_COL32((int)((c[0] * ia + e[0] * t) * 255.f),
			                (int)((c[1] * ia + e[1] * t) * 255.f),
			                (int)((c[2] * ia + e[2] * t) * 255.f),
			                (int)(255.f * clamp01(a)));
		}

		// rounded rect filled with the accent gradient, sampled between two
		// stops of a normalised span (0 = track start, 1 = track end)
		void accent_rect(ImDrawList* dl, const ImVec2& mn, const ImVec2& mx,
		                 float t0, float t1, float rounding, float a)
		{
			dl->AddRectFilledMultiColorRounded(mn, mx, accent_grad(t0, a), accent_grad(t1, a),
			                                   accent_grad(t1, a), accent_grad(t0, a), rounding);
		}

		// a row's master switch track: flat "off" wash, then either the accent
        // gradient or a flat on-colour cross-faded in as the switch springs on.
        // The reference's off track is a clearly visible mid grey, not a hint of
        // one - it reads as a filled pill even when the feature is off.
        void switch_track(ImDrawList* dl, const ImVec2& t0, const ImVec2& t1,
                          float rounding, float on, float a, ImU32 flat_on)
        {
            const ImU32 off = IM_COL32(255, 255, 255, (int)(60 * a));
			dl->AddRectFilled(t0, t1, off, rounding);
			if (on <= 0.002f)
				return;
			if (flat_on == 0)
				accent_rect(dl, t0, t1, 0.f, 1.f, rounding, a * on);
			else
				dl->AddRectFilled(t0, t1, lerp_col(off, flat_on, on), rounding);
		}

        // the reference's green: only the Field Of View master switch uses it
        const ImU32 k_green = IM_COL32(53, 200, 95, 255);

        // the switch puck: grey while the track is off, white once it is on -
        // and it slides across with the same spring that fills the track
        void switch_thumb(ImDrawList* dl, const ImVec2& t0, const ImVec2& t1,
                          float on, float a)
        {
            const float h = t1.y - t0.y;
            const float r = h * 0.5f - 2.f;             // 18 - 4 -> 14 px puck
            const float cx0 = t0.x + h * 0.5f;          // left seat  (off)
            const float cx1 = t1.x - h * 0.5f;          // right seat (on)
            const ImVec2 kc(cx0 + (cx1 - cx0) * on, t0.y + h * 0.5f);
            dl->AddCircleFilled(ImVec2(kc.x, kc.y + 1.f), r + 0.5f,
                                IM_COL32(0, 0, 0, (int)(55 * a)), 24);
            dl->AddCircleFilled(kc, r,
                                ink_a(lerp_col(IM_COL32(158, 159, 170, 255),
                                               IM_COL32(248, 248, 252, 255), on), a), 24);
        }

        // the "..." marker: three dots laid out horizontally, left of the
        // control column - the reference draws them in a row, not a column
        void dots_marker(ImDrawList* dl, float cx, float cy, float a)
        {
            for (int i = -1; i <= 1; ++i)
                dl->AddCircleFilled(ImVec2(cx + i * 5.f, cy), 1.4f,
                                    IM_COL32(138, 140, 152, (int)(255 * a)), 8);
        }

        // `on_state` marks a row that owns a master switch: the reference keeps
        // a soft plate behind every row whose feature is ON, so an enabled
        // setting reads at a glance without looking at the switch itself.
        Row row_begin(const char* label, float h, float ctl_w, const char* anim_key,
                      const bool* on_state = nullptr)
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
			Spring& hv = spring_for(id, 3, 0.f);
			hv.target = (r.hovered || r.active) ? 1.f : 0.f;
			spring_tick(hv, 16.f);
			r.hover = clamp01(hv.x);

            if (on_state)
            {
                Spring& k = spring_for(id, 2, *on_state ? 1.f : 0.f);
                k.target = *on_state ? 1.f : 0.f;
                spring_tick(k, 16.f);
                r.on = clamp01(k.x);
            }

            // soft plate behind the row. It is an ImGui-drawn wash, not a glass
            // primitive: the glass pass runs before all ImGui geometry, so a
            // glass plate would be painted *under* the section's plate wash and
            // read as a foggy patch inside a flat box. An enabled row keeps a
            // faint plate; hover lifts it a little further.
            const float plate = 11.f * r.on + 10.f * hv.x;
            if (plate > 0.4f)
            {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(r.box.left, r.y), ImVec2(r.box.right, r.y + h),
                    IM_COL32(255, 255, 255, (int)(plate * r.a)), 10.f);
			}

			if (label && *label)
				ImGui::GetWindowDrawList()->AddText(
					ImVec2(r.box.left, r.y + (h - ImGui::GetTextLineHeight()) * 0.5f),
					ink_a(ink(), r.a), label);

			return r;
		}

		// closes the row: hairline along the bottom edge, cursor onto the next
		void row_end(const Row& r)
		{
			// the rule stops at the row box (14 px inside the section plate) and
			// is deliberately faint - it reads as a hairline, not a divider
			if (r.a > 0.01f)
				ImGui::GetWindowDrawList()->AddLine(
					ImVec2(r.box.left, r.y + r.h - 1.f),
					ImVec2(r.box.right, r.y + r.h - 1.f),
					IM_COL32(255, 255, 255, (int)(15.f * r.a)), 1.f);

			ImGui::SetCursorScreenPos(ImVec2(r.pos.x, r.pos.y + r.h));
			ImGui::Dummy(ImVec2(0.01f, 0.01f));
			ImGui::PopID();
		}

		// the feature switch: label left, pill switch right. `more` marks a
		// row that owns sub-options (the reference draws "..." beside it);
		// when `open` is given the marker is clickable and toggles it, so a
		// row can carry hidden sub-options even while it is switched on.
		// `flat_on` = 0 keeps the reference's accent gradient track; any other
        // when `open` is given the marker is clickable and toggles it, so a
        // row can carry hidden sub-options even while it is switched on.
        // `flat_on` = 0 keeps the reference's accent gradient track; any other
        // colour paints a flat "on" track.
		bool feature_toggle(const char* label, bool* v, bool more, bool* open = nullptr,
		                    ImU32 flat_on = 0)
		{
			const bool dots = more || (open != nullptr);
			const float ctl_w = k_switch_w + (dots ? 23.f : 0.f);
			Row r = row_begin(label, k_row_h, ctl_w, "tg", v);

			const float sw = k_switch_w, sh = k_switch_h;
			const float sx = r.box.right - sw;
			const float sy = r.y + (r.h - sh) * 0.5f;

			bool on_dots = false;
			if (open && r.clicked)
			{
				const ImVec2 d0(sx - 24.f, r.y + 4.f), d1(sx - 5.f, r.y + r.h - 4.f);
				on_dots = ImGui::IsMouseHoveringRect(d0, d1);
			}
			if (r.clicked)
			{
				if (on_dots) *open = !*open;
				else         *v = !*v;
			}

			const float on = r.on;

			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImVec2 t0(sx, sy), t1(sx + sw, sy + sh);
			switch_track(dl, t0, t1, sh * 0.5f, on, r.a, flat_on);
            switch_thumb(dl, t0, t1, on, r.a);

            if (dots)
                dots_marker(dl, sx - 17.f, sy + sh * 0.5f, r.a);

			row_end(r);
			return r.clicked;
		}

		// the reference's expandable rows: label left, chevron right, plus a
		// master control to the left of the chevron for rows that own a feature
		// switch. The master toggles the feature, anywhere else expands the row.
        // `master` picks the shape: none, the square checkbox (the reference's
        // `Field Of View` control) or a pill switch.
        enum class ExpandMaster { None, Square, Switch, GreenSwitch };

		bool expand_row_impl(const char* label, bool* open, bool* check, ExpandMaster master)
		{
			const bool has_master = (check != nullptr) && master != ExpandMaster::None;
            const bool sw_master = (master == ExpandMaster::Switch || master == ExpandMaster::GreenSwitch);
            const float master_w = !has_master ? 0.f : (sw_master ? k_switch_w + 8.f : 16.f + 8.f);
            const float ctl_w = 22.f + master_w;
            Row r = row_begin(label, k_row_h, ctl_w, "ex");

            const float bs = sw_master ? k_switch_h : 16.f;
            const float bx = r.box.right - 22.f - bs;
			const ImVec2 mn(bx, r.y + (r.h - bs) * 0.5f);
			const ImVec2 mx(bx + bs, mn.y + bs);

			if (r.clicked)
			{
				if (has_master && ImGui::IsMouseHoveringRect(mn, mx))
					*check = !*check;
				else if (open)
					*open = !*open;
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();

			if (has_master)
			{
				if (sw_master)
				{
                    // a real switch: same spring + gradient as a toggle row, or
                    // the reference's green for Field Of View
                    const ImGuiID id = ImGui::GetID("##row");
                    Glass::Spring& k = Glass::g->springs().Get((uint32_t)id, 2,
                                                               Glass::SpringStyle::Critical,
                                                               *check ? 1.f : 0.f);
                    k.target = *check ? 1.f : 0.f;
                    k.Tick(ImGui::GetIO().DeltaTime);
                    const float on = clamp01(k.x);
                    const float rr = k_switch_h * 0.5f;
                    switch_track(dl, mn, mx, rr, on, r.a,
                                 master == ExpandMaster::GreenSwitch ? k_green : 0);
                    switch_thumb(dl, mn, mx, on, r.a);
                }
                else if (*check)
                {
                    // the reference's `Field Of View` master: a filled white
                    // rounded square, no tick glyph inside it
                    dl->AddRectFilled(mn, mx, ink_a(IM_COL32(243, 243, 248, 255), r.a), 5.f);
				}
				else
				{
					dl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, (int)(26 * r.a)), 5.f);
					dl->AddRect(mn, mx, IM_COL32(255, 255, 255, (int)(70 * r.a)), 5.f);
				}
			}

			menu_icons::Chevron(dl, true,
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
			// the reference's value plate is a faint wash with a hairline edge
            dl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, (int)((hot ? 20.f : 14.f) * a)), 6.f);
            dl->AddRect(mn, mx, IM_COL32(255, 255, 255, (int)((hot ? 34.f : 26.f) * a)), 6.f);
            const ImVec2 ts = ImGui::CalcTextSize(text);
            dl->AddText(ImVec2(mn.x + ((mx.x - mn.x) - ts.x) * 0.5f,
                               mn.y + ((mx.y - mn.y) - ts.y) * 0.5f),
                        IM_COL32(217, 219, 229, (int)(255.f * a)), text);
		}

		// the settings slider: label left, thin accent track, value box right
		bool settings_slider(const char* label, float* v, float mn, float mx, const char* fmt)
		{
			const float ctl_w = k_box_w + 15.f + k_track_w;
			Row r = row_begin(label, k_row_h, ctl_w, "sl");
			const float old = *v;

			const float bx = r.box.right - k_box_w;           // value box left
			const float tx = bx - 15.f - k_track_w;           // track left
			const float ty = r.y + r.h * 0.5f;
			const float kr = 6.5f;

			if (r.active || r.clicked)
			{
				float t = (ImGui::GetIO().MousePos.x - tx) / (std::max)(1.f, k_track_w);
				*v = mn + clamp01(t) * (mx - mn);
			}

			const float t = clamp01((mx != mn) ? ((*v - mn) / (mx - mn)) : 0.f);
			ImDrawList* dl = ImGui::GetWindowDrawList();
            // empty rail is a flat mid-grey wash - the same weight as a switch
            // track that is off; the filled part is the accent gradient sampled
            // from the track start to the thumb
            const ImVec2 tr0(tx, ty - 2.f), tr1(tx + k_track_w, ty + 2.f);
            dl->AddRectFilled(tr0, tr1, IM_COL32(255, 255, 255, (int)(52 * r.a)), 2.f);
			if (t > 0.002f)
				accent_rect(dl, tr0, ImVec2(tx + k_track_w * t, ty + 2.f), 0.f, t, 2.f, r.a);

			const ImVec2 kc(tx + k_track_w * t, ty);
            const float krr = kr * (r.active ? 1.25f : (r.hovered ? 1.1f : 1.f));
            dl->AddCircleFilled(ImVec2(kc.x, kc.y + 1.f), krr + 0.5f, IM_COL32(0, 0, 0, (int)(55 * r.a)), 20);
            dl->AddCircleFilled(kc, krr, ink_a(IM_COL32(248, 248, 252, 255), r.a), 20);

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
			const float dots_w = open ? 23.f : 0.f;
			Row r = row_begin(label, k_row_h, k_drop_w + dots_w, "dd");

			ImDrawList* dl = ImGui::GetWindowDrawList();

			bool on_dots = false;
			if (open && r.clicked)
			{
				const ImVec2 d0(r.box.right - k_drop_w - 24.f, r.y + 4.f);
				const ImVec2 d1(r.box.right - k_drop_w - 5.f, r.y + r.h - 4.f);
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
				// the reference's dropdown is a slightly lighter plate than the
                // slider value box, with a hairline edge
                dl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, (int)((hot ? 24.f : 18.f) * r.a)), 6.f);
                dl->AddRect(mn, mx, IM_COL32(255, 255, 255, (int)((hot ? 36.f : 28.f) * r.a)), 6.f);

                const ImVec2 ts = ImGui::CalcTextSize(cur_val);
                dl->AddText(ImVec2(mn.x + 12.f, mn.y + (k_drop_h - ts.y) * 0.5f),
                            IM_COL32(217, 219, 229, (int)(255.f * r.a)), cur_val);
            }
            menu_icons::Chevron(dl, false, ImVec2(mx.x - 12.f, mn.y + k_drop_h * 0.5f),
                            5.f, ink_a(ink_soft(), r.a), 1.6f);

            if (open)
                dots_marker(dl, r.box.right - k_drop_w - 17.f, r.y + r.h * 0.5f, r.a);

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
			const ImVec2 kmin = cur;
			const ImVec2 kmax = ImVec2(cur.x + w, cur.y + h);
			if (listening)
				accent_rect(dl, kmin, kmax, 0.f, 1.f, h * 0.5f, 1.f);
			else
			{
				dl->AddRectFilled(kmin, kmax, IM_COL32(255, 255, 255,
					(int)((ImGui::IsItemHovered() ? 44.f : 30.f))), h * 0.5f);
				dl->AddRect(kmin, kmax, IM_COL32(255, 255, 255, 46), h * 0.5f);
			}
			dl->AddText(ImVec2(cur.x + (w - ts.x) * 0.5f, cur.y + (h - ts.y) * 0.5f),
			            listening ? IM_COL32(255, 255, 255, 255) : ink(), buf);

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

		// a pill toggle used by the multi-select rows: flat wash when off, the
		// accent gradient when on - the reference kit's Chip, drawn with the
		// plain draw list (no LiquidUI submission)
		bool chip(const char* label, bool selected)
		{
			ImGui::PushID(label);
			const ImVec2 ts = ImGui::CalcTextSize(label);
			const float w = ts.x + 26.f, h = 30.f;
			const ImVec2 cur = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##chip", ImVec2(w, h));
			const bool hov = ImGui::IsItemHovered();
			const bool clicked = ImGui::IsItemDeactivated() && hov;

			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImVec2 mn = cur, mx = ImVec2(cur.x + w, cur.y + h);
			if (selected)
				accent_rect(dl, mn, mx, 0.f, 1.f, h * 0.5f, 1.f);
			else
			{
				dl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, hov ? 34 : 22), h * 0.5f);
				dl->AddRect(mn, mx, IM_COL32(255, 255, 255, hov ? 60 : 42), h * 0.5f);
			}
			dl->AddText(ImVec2(cur.x + (w - ts.x) * 0.5f, cur.y + (h - ts.y) * 0.5f),
			            selected ? IM_COL32(255, 255, 255, 255) : ink(), label);
			ImGui::PopID();
			return clicked;
		}
	}

	// exported wrappers - the implementation lives in the anonymous namespace above
	bool expand_row(const char* label, bool* open, bool* check)
	{
		return expand_row_impl(label, open, check,
			check ? ExpandMaster::Square : ExpandMaster::None);
	}

    // the reference's `Field Of View` shape: same expandable row, but the
    // master control is a switch instead of the square - green, as in the
    // reference (MATCHA_UI_SPEC §5.1)
    bool expand_row_switch(const char* label, bool* open, bool* check, bool green)
    {
        return expand_row_impl(label, open, check,
            check ? (green ? ExpandMaster::GreenSwitch : ExpandMaster::Switch)
                  : ExpandMaster::None);
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
			// same geometry as the legacy panel, but the chrome is the reference's
			// flat plate wash (MATCHA_UI_SPEC §4.5: rgba(255,255,255,0.022) fill
			// + a 1 px rgba(255,255,255,0.04) edge). It is deliberately NOT a
			// glass primitive: a second glass layer on top of the card stacks
			// blur + tint and makes the panel a different shade than the rail.
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
			ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.f);
			ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_None,
				ImGuiWindowFlags_NoScrollbar);
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();

			// the plate takes the child's REAL rect, so it can never sit offset
			// from its own rows - a clamped/short child used to let the plate
			// stick out past them and look cut off
			{
				const ImVec2 o = ImGui::GetWindowPos();
				const ImVec2 s = ImGui::GetWindowSize();
				const ImVec2 p1(o.x + s.x, o.y + s.y);
                const float a = Glass::g ? Glass::g->SubmitFadeValue() : 1.f;
				ImDrawList* dl = ImGui::GetWindowDrawList();
				dl->AddRectFilled(o, p1, IM_COL32(255, 255, 255, (int)(6.f * a)), 14.f);
				dl->AddRect(o, p1, IM_COL32(255, 255, 255, (int)(10.f * a)), 14.f, 0, 1.f);
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

		// 10.5 px caps with 1.2 px of tracking, soft grey - the reference's
		// section label (MATCHA_UI_SPEC §3 / §4.4), drawn in the menu face
        ImFont* sf = fonts::menu_regular();
		const float fs = 10.5f;
		float x = o.x;
		for (const char* p = text; *p; ++p)
		{
			char ch[2] = { (char)toupper((unsigned char)*p), 0 };
            dl->AddText(sf, fs, ImVec2(x, o.y), Glass::InkSoftColor(), ch);
			x += sf->CalcTextSizeA(fs, FLT_MAX, 0.f, ch).x + 1.2f;
		}
		// the reference has no rule under its section labels, just the label
		ImGui::Dummy(ImVec2(w, 24.f));
	}

	// a box around its rows; the plate is drawn in a *reserved channel* of this
	// child's draw list and filled in by end_section(), so it always matches the
	// real content while staying behind the rows.
	//
	// NOTE: the plate is a flat wash, NOT a glass primitive. Every extra glass
	// layer inside the card stacks blur + tint, which made the page read as a
	// different colour than the rail beside it. MATCHA_UI_SPEC §4.5 describes the
	// reference's plate as exactly this: fill rgba(255,255,255,0.022) + a 1 px
	// rgba(255,255,255,0.04) edge.
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

		// channel 0 = the plate, channel 1 = the rows drawn after it
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->ChannelsSplit(2);
		dl->ChannelsSetCurrent(1);

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

		// fill in the plate (channel 0), then merge so it lands behind the rows
		const ImVec2 o = ImGui::GetWindowPos();
		const ImVec2 s = ImGui::GetWindowSize();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->ChannelsSetCurrent(0);
		{
            const float a = Glass::g ? Glass::g->SubmitFadeValue() : 1.f;
			const ImVec2 p1(o.x + s.x, o.y + s.y);
			dl->AddRectFilled(o, p1, IM_COL32(255, 255, 255, (int)(6.f * a)), 14.f);
			dl->AddRect(o, p1, IM_COL32(255, 255, 255, (int)(10.f * a)), 14.f, 0, 1.f);
		}
		dl->ChannelsMerge();

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
				ImGui::PushStyleColor(ImGuiCol_Text, ink());
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
				if (chip(items[i], selected[i]))
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
			return widgets::button(label ? label : "##btn");

		return widgets::button(label);
	}

	bool row_input_text(const char* label, char* buf, int len)
	{
		if (glass_mode())
			return widgets::input_text(label ? label : "##field", buf, len);

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
				ink_a(ink_soft(), r.a), text ? text : "");
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
