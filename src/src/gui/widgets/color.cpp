#include "pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../colors/colors.h"
#include "../resources/fonts/fonts.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cmath>

namespace {

    // размеры свотча/попапа, не трогать хуйню
    const float color_size = 14.f;
    const float color_gap = 5.f;
    const float right_margin = 14.f;

    const float pad = 6.f;
    const float gap = 6.f;
    const float sv_size = 168.f;
    const float bar_thick = 16.f;
    const float row_h = 16.f;
    const float border = 2.f;

    const int hdr_rows = 22;

    const float inner_w = sv_size + gap + bar_thick;
    const float popup_w = border * 2.f + pad * 2.f + inner_w;
    const float content_h = sv_size + gap + bar_thick + gap + row_h;
    const float popup_h = border + (float)hdr_rows + 1.f
        + pad + content_h + pad + border;

    void frame3(ImDrawList* dl, const ImRect& outer)
    {
        const ImRect inner(outer.Min + ImVec2(1, 1), outer.Max - ImVec2(1, 1));
        dl->AddRect(inner.Min, inner.Max, colors::widget_inline_u32(),  0.0f, 0, 1.0f);
        dl->AddRect(outer.Min, outer.Max, colors::widget_outline_u32(), 0.0f, 0, 1.0f);
    }

    ImRect fill_of(const ImRect& outer)
    {
        return ImRect(outer.Min + ImVec2(2, 2), outer.Max - ImVec2(2, 2));
    }

    // шашечка под альфу
    void draw_checkerboard(ImDrawList* dl, const ImRect& r, float cell = 4.0f)
    {
        const ImU32 c0 = ImGui::ColorConvertFloat4ToU32(ImLerp(colors::child_fill, colors::text_inactive, 0.35f));
        const ImU32 c1 = colors::widget_track_u32();
        dl->AddRectFilled(r.Min, r.Max, c0);
        int row = 0;
        for (float y = r.Min.y; y < r.Max.y; y += cell, ++row) {
            const float y1 = ImMin(y + cell, r.Max.y);
            int colIdx = row & 1;
            for (float x = r.Min.x; x < r.Max.x; x += cell, ++colIdx) {
                if (colIdx & 1) continue;
                const float x1 = ImMin(x + cell, r.Max.x);
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x1, y1), c1);
            }
        }
    }

    void draw_sv_cursor(ImDrawList* dl, const ImVec2& c)
    {
        dl->AddCircle(c, 4.5f, IM_COL32(0, 0, 0, 255),   0, 2.0f);
        dl->AddCircle(c, 3.5f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
    }

    void draw_hbar_cursor(ImDrawList* dl, const ImRect& fill, float t)
    {
        float y = fill.Min.y + t * fill.GetHeight();
        if (y < fill.Min.y) y = fill.Min.y;
        if (y > fill.Max.y) y = fill.Max.y;
        dl->AddRectFilled(ImVec2(fill.Min.x - 1.f, y - 2.f),
                          ImVec2(fill.Max.x + 1.f, y + 2.f),
                          IM_COL32(0, 0, 0, 255));
        dl->AddRectFilled(ImVec2(fill.Min.x, y - 1.f),
                          ImVec2(fill.Max.x, y + 1.f),
                          IM_COL32(255, 255, 255, 255));
    }

    void draw_vbar_cursor(ImDrawList* dl, const ImRect& fill, float t)
    {
        float x = fill.Min.x + t * fill.GetWidth();
        if (x < fill.Min.x) x = fill.Min.x;
        if (x > fill.Max.x) x = fill.Max.x;
        dl->AddRectFilled(ImVec2(x - 2.f, fill.Min.y - 1.f),
                          ImVec2(x + 2.f, fill.Max.y + 1.f),
                          IM_COL32(0, 0, 0, 255));
        dl->AddRectFilled(ImVec2(x - 1.f, fill.Min.y),
                          ImVec2(x + 1.f, fill.Max.y),
                          IM_COL32(255, 255, 255, 255));
    }

    void draw_panel_frame(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                          const char* title)
    {
        const int ol = (int)min.x,     ot = (int)min.y;
        const int orr = (int)max.x - 1, ob = (int)max.y - 1;
        const int fl = ol + 2, ft = ot + 1, fr = orr - 1;

        dl->AddRectFilled(ImVec2((float)fl, (float)ft),
                          ImVec2((float)(fr + 1), (float)ob),
                          ImGui::GetColorU32(colors::child_fill));

        for (int i = 0; i < hdr_rows; ++i)
            dl->AddRectFilled(ImVec2((float)fl, (float)(ft + i)),
                              ImVec2((float)(fr + 1), (float)(ft + i + 1)),
                              colors::header_gradient_row(i, hdr_rows));

        dl->AddRectFilled(ImVec2((float)fl, (float)(ft + hdr_rows)),
                          ImVec2((float)(fr + 1), (float)(ft + hdr_rows + 1)),
                          colors::widget_inline_u32());

        dl->AddRect(ImVec2((float)ol, (float)ot),
                    ImVec2((float)(orr + 1), (float)(ob + 1)), IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);

        const ImU32 inl = colors::widget_inline_u32();
        const int il = ol + 1, it = ot + 1, ir = orr - 1, ib = ob - 1;
        dl->AddLine(ImVec2((float)il, (float)ib), ImVec2((float)ir, (float)ib), inl);
        dl->AddLine(ImVec2((float)il, (float)it), ImVec2((float)il, (float)ib), inl);
        dl->AddLine(ImVec2((float)ir, (float)it), ImVec2((float)ir, (float)ib), inl);

        if (title && title[0]) {
            ImFont* font = fonts::ui();
            const float fs = fonts::ui_size(font);
            const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, title);
            const ImVec2 tpos(ImFloor((float)fl + 6.0f),
                              ImFloor((float)ft + ((float)hdr_rows - tsz.y) * 0.5f));
            widgets::draw_outlined_text(dl, font, fs, tpos,
                colors::text_active_u32(), title);
        }
    }

    // кэш hsv а то hue скачет когда s/v около нуля
    struct hsv_cache {
        ImGuiID id  = 0;
        float   h = 0.0f, s = 0.0f, v = 0.0f;
    };
    hsv_cache g_hsv;

    void sync_hsv(ImGuiID id, const float col[4])
    {
        float cr, cg, cb;
        ImGui::ColorConvertHSVtoRGB(g_hsv.h, g_hsv.s, g_hsv.v, cr, cg, cb);
        const bool mismatch = fabsf(cr - col[0]) > 0.002f
                           || fabsf(cg - col[1]) > 0.002f
                           || fabsf(cb - col[2]) > 0.002f;
        if (g_hsv.id != id || mismatch) {
            float h, s, v;
            ImGui::ColorConvertRGBtoHSV(col[0], col[1], col[2], h, s, v);

            if (s > 0.0001f || g_hsv.id != id) g_hsv.h = h;
            if (v > 0.0001f || g_hsv.id != id) g_hsv.s = s;
            g_hsv.v  = v;
            g_hsv.id = id;
        }
    }
}

namespace widgets {

    float color_picker_width() { return color_size; }

    float color_picker_row_y()
    {
        return ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y;
    }

    void same_line_color_picker(float row_y, int slot_from_right, int slot_count)
    {
        if (slot_count < 1) slot_count = 1;
        if (slot_from_right < 0) slot_from_right = 0;
        if (slot_from_right > slot_count - 1) slot_from_right = slot_count - 1;

        float box = color_size;
        float g = color_gap;
        float group_w = slot_count * box + (slot_count - 1) * g;
        float slot_x = ImGui::GetWindowContentRegionMax().x
            - right_margin
            - group_w
            + slot_from_right * (box + g);

        ImGui::SameLine(slot_x);
        // +6 = выравнивание свотча с checkbox-боксом (box_oy)
        ImGui::SetCursorPosY(row_y + 6.f);
    }

    bool color_edit4(const char* id, float col[4])
    {
        if (!col || !id) return false;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems) return false;

        const ImGuiID widget_id = window->GetID(id);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(color_size, color_size);
        const ImRect bounds(pos, pos + size);

        ImGui::ItemSize(size);
        if (!ImGui::ItemAdd(bounds, widget_id)) return false;

        bool hovered = false, held = false;
        const bool pressed = ImGui::ButtonBehavior(bounds, widget_id, &hovered, &held);

        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImRect swf = fill_of(bounds);
            if (col[3] < 0.999f) draw_checkerboard(dl, swf, 3.0f);
            dl->AddRectFilled(swf.Min, swf.Max,
                ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3])));
            frame3(dl, bounds);
        }

        char popup_id[64];
        snprintf(popup_id, sizeof(popup_id), "##cpop_%u", widget_id);
        if (pressed) ImGui::OpenPopup(popup_id);

        bool changed = false;

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

        ImGui::SetNextWindowPos(ImVec2(bounds.Min.x, bounds.Max.y + 2.0f));
        ImGui::SetNextWindowSize(ImVec2(popup_w, popup_h));

        if (ImGui::BeginPopup(popup_id,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 wp = ImGui::GetWindowPos();
            const ImVec2 wsz = ImGui::GetWindowSize();

            draw_panel_frame(dl, wp, wp + wsz, "color picker");

            sync_hsv(widget_id, col);
            float& H = g_hsv.h;
            float& S = g_hsv.s;
            float& V = g_hsv.v;

            const float cx = wp.x + border + pad;
            const float cy = wp.y + border + (float)hdr_rows + 1.0f + pad;

            const ImRect sv_outer (ImVec2(cx, cy),
                                   ImVec2(cx + sv_size, cy + sv_size));
            const ImRect hue_outer(ImVec2(sv_outer.Max.x + gap, cy),
                                   ImVec2(sv_outer.Max.x + gap + bar_thick,
                                          cy + sv_size));
            const float  bar_y  = sv_outer.Max.y + gap;
            const ImRect a_outer(ImVec2(cx, bar_y),
                                 ImVec2(cx + inner_w, bar_y + bar_thick));
            const float  row_y  = a_outer.Max.y + gap;

            const ImRect sv_fill  = fill_of(sv_outer);
            const ImRect hue_fill = fill_of(hue_outer);
            const ImRect a_fill   = fill_of(a_outer);

            const ImVec2 mouse = ImGui::GetIO().MousePos;

            ImGui::SetCursorScreenPos(sv_outer.Min);
            ImGui::InvisibleButton("##sv", sv_outer.GetSize());
            if (ImGui::IsItemActive())
            {
                S = (mouse.x - sv_fill.Min.x) / sv_fill.GetWidth();
                if (S < 0.f) S = 0.f;
                if (S > 1.f) S = 1.f;
                float vv = (mouse.y - sv_fill.Min.y) / sv_fill.GetHeight();
                if (vv < 0.f) vv = 0.f;
                if (vv > 1.f) vv = 1.f;
                V = 1.f - vv;
                changed = true;
            }

            ImGui::SetCursorScreenPos(hue_outer.Min);
            ImGui::InvisibleButton("##hue", hue_outer.GetSize());
            if (ImGui::IsItemActive())
            {
                H = (mouse.y - hue_fill.Min.y) / hue_fill.GetHeight();
                if (H < 0.f) H = 0.f;
                if (H > 0.9999f) H = 0.9999f;
                changed = true;
            }

            ImGui::SetCursorScreenPos(a_outer.Min);
            ImGui::InvisibleButton("##alpha", a_outer.GetSize());
            if (ImGui::IsItemActive())
            {
                float a = (mouse.x - a_fill.Min.x) / a_fill.GetWidth();
                if (a < 0.f) a = 0.f;
                if (a > 1.f) a = 1.f;
                col[3] = a;
                changed = true;
            }

            if (changed)
                ImGui::ColorConvertHSVtoRGB(H, S, V, col[0], col[1], col[2]);

            {
                float hr, hg, hb;
                ImGui::ColorConvertHSVtoRGB(H, 1.0f, 1.0f, hr, hg, hb);
                const ImU32 hue_col = ImGui::ColorConvertFloat4ToU32(ImVec4(hr, hg, hb, 1.0f));

                dl->AddRectFilledMultiColor(sv_fill.Min, sv_fill.Max,
                    IM_COL32(255,255,255,255), hue_col, hue_col, IM_COL32(255,255,255,255));
                dl->AddRectFilledMultiColor(sv_fill.Min, sv_fill.Max,
                    IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
                    IM_COL32(0,0,0,255), IM_COL32(0,0,0,255));

                frame3(dl, sv_outer);
                draw_sv_cursor(dl, ImVec2(
                    sv_fill.Min.x + S * sv_fill.GetWidth(),
                    sv_fill.Min.y + (1.0f - V) * sv_fill.GetHeight()));
            }

            {
                int seg = 6;
                float seg_h = hue_fill.GetHeight() / (float)seg;
                for (int i = 0; i < seg; ++i) {
                    float r0, g0, b0, r1, g1, b1;
                    ImGui::ColorConvertHSVtoRGB((float)i       / seg, 1.0f, 1.0f, r0, g0, b0);
                    ImGui::ColorConvertHSVtoRGB((float)(i + 1) / seg, 1.0f, 1.0f, r1, g1, b1);
                    const ImU32 c0 = ImGui::ColorConvertFloat4ToU32(ImVec4(r0, g0, b0, 1.0f));
                    const ImU32 c1 = ImGui::ColorConvertFloat4ToU32(ImVec4(r1, g1, b1, 1.0f));
                    const float y0 = hue_fill.Min.y + seg_h * (float)i;
                    dl->AddRectFilledMultiColor(
                        ImVec2(hue_fill.Min.x, y0),
                        ImVec2(hue_fill.Max.x, y0 + seg_h),
                        c0, c0, c1, c1);
                }
                frame3(dl, hue_outer);
                draw_hbar_cursor(dl, hue_fill, H);
            }

            {
                draw_checkerboard(dl, a_fill);
                const ImU32 a0 = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 0.0f));
                const ImU32 a1 = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 1.0f));
                dl->AddRectFilledMultiColor(a_fill.Min, a_fill.Max, a0, a1, a1, a0);
                frame3(dl, a_outer);
                draw_vbar_cursor(dl, a_fill, col[3]);
            }

            {
                const ImRect prev_outer(ImVec2(cx, row_y),
                                        ImVec2(cx + 34.0f, row_y + row_h));
                const ImRect prev_fill = fill_of(prev_outer);
                draw_checkerboard(dl, prev_fill);
                dl->AddRectFilled(prev_fill.Min, prev_fill.Max,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3])));
                frame3(dl, prev_outer);

                ImFont* font = fonts::ui();
                const float fs = fonts::ui_size(font);

                char hex[16];
                snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                    (int)(col[0] * 255.0f + 0.5f),
                    (int)(col[1] * 255.0f + 0.5f),
                    (int)(col[2] * 255.0f + 0.5f));

                const ImVec2 hsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, hex);
                widgets::draw_outlined_text(dl, font, fs,
                    ImVec2(ImFloor(prev_outer.Max.x + gap),
                           ImFloor(row_y + (row_h - hsz.y) * 0.5f)),
                    colors::text_active_u32(), hex);

                char apct[16];
                snprintf(apct, sizeof(apct), "%d%%", (int)(col[3] * 100.0f + 0.5f));
                const ImVec2 asz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, apct);
                widgets::draw_outlined_text(dl, font, fs,
                    ImVec2(ImFloor(cx + inner_w - asz.x),
                           ImFloor(row_y + (row_h - asz.y) * 0.5f)),
                    colors::text_inactive_u32(), apct);
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        if (changed) ImGui::MarkItemEdited(widget_id);
        return changed;
    }

}
