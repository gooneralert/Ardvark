#include "pch.h"
#include "colorpicker.h"
#include "text.h"
#include "imgui.h"

namespace widgets
{
    static void push_picker_style()
    {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.08f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.f, 6.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 6.f));
    }

    static void pop_picker_style()
    {
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(2);
    }

    static void draw_sv_marker(ImDrawList* draw, ImVec2 c)
    {
        constexpr float s = 3.f;
        ImVec2 a(c.x - s, c.y - s), b(c.x + s, c.y + s);
        draw->AddRect(ImVec2(a.x - 1.f, a.y - 1.f), ImVec2(b.x + 1.f, b.y + 1.f), IM_COL32(0, 0, 0, 255));
        draw->AddRect(a, b, IM_COL32(255, 255, 255, 255));
    }

    static void draw_hue_marker(ImDrawList* draw, float x0, float x1, float y)
    {
        draw->AddLine(ImVec2(x0 - 3.f, y), ImVec2(x1 + 3.f, y), IM_COL32(0, 0, 0, 255), 3.f);
        draw->AddLine(ImVec2(x0 - 2.f, y), ImVec2(x1 + 2.f, y), IM_COL32(255, 255, 255, 255), 1.f);
        draw->AddRectFilled(ImVec2(x0 - 3.f, y - 2.f), ImVec2(x0 - 1.f, y + 2.f), IM_COL32(255, 255, 255, 255));
        draw->AddRectFilled(ImVec2(x1 + 1.f, y - 2.f), ImVec2(x1 + 3.f, y + 2.f), IM_COL32(255, 255, 255, 255));
        draw->AddRect(ImVec2(x0 - 3.f, y - 2.f), ImVec2(x0 - 1.f, y + 2.f), IM_COL32(0, 0, 0, 255));
        draw->AddRect(ImVec2(x1 + 1.f, y - 2.f), ImVec2(x1 + 3.f, y + 2.f), IM_COL32(0, 0, 0, 255));
    }

    static bool alpha_bar_h(float* alpha, float width, float height, const ImVec4& rgb)
    {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##alpha", ImVec2(width, height));
        bool active = ImGui::IsItemActive();
        bool changed = false;

        if (active)
        {
            float t = (ImGui::GetIO().MousePos.x - pos.x) / width;
            if (t < 0.f) t = 0.f;
            if (t > 1.f) t = 1.f;
            if (*alpha != t)
            {
                *alpha = t;
                changed = true;
            }
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 max(pos.x + width, pos.y + height);
        draw->PushClipRect(pos, max, true);

        const float step = 4.f;
        for (float y = pos.y; y < max.y; y += step)
            for (float x = pos.x; x < max.x; x += step)
            {
                bool dark = (((int)((x - pos.x) / step) + (int)((y - pos.y) / step)) & 1) != 0;
                float x1 = x + step < max.x ? x + step : max.x;
                float y1 = y + step < max.y ? y + step : max.y;
                draw->AddRectFilled(ImVec2(x, y), ImVec2(x1, y1), dark ? IM_COL32(40, 40, 40, 255) : IM_COL32(70, 70, 70, 255));
            }

        ImU32 col_a0 = ImGui::GetColorU32(ImVec4(rgb.x, rgb.y, rgb.z, 0.f));
        ImU32 col_a1 = ImGui::GetColorU32(ImVec4(rgb.x, rgb.y, rgb.z, 1.f));
        draw->AddRectFilledMultiColor(pos, max, col_a0, col_a1, col_a1, col_a0);
        draw->AddRect(pos, max, ImGui::GetColorU32(ImVec4(0.22f, 0.22f, 0.22f, 1.f)));

        float x = pos.x + width * (*alpha);
        if (x < pos.x) x = pos.x;
        if (x > max.x - 1.f) x = max.x - 1.f;
        draw->AddLine(ImVec2(x, pos.y), ImVec2(x, max.y), IM_COL32(0, 0, 0, 255), 3.f);
        draw->AddLine(ImVec2(x, pos.y), ImVec2(x, max.y), IM_COL32(255, 255, 255, 255), 1.f);

        draw->PopClipRect();
        return changed;
    }

    static bool compact_picker(float color[4])
    {
        constexpr float sv_size = 104.f;
        constexpr float hue_w = 10.f;
        constexpr float gap = 4.f;
        constexpr float alpha_h = 10.f;
        const float total_w = sv_size + gap + hue_w;

        float h, s, v;
        ImGui::ColorConvertRGBtoHSV(color[0], color[1], color[2], h, s, v);

        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        bool changed = false;

        ImVec2 sv_min = origin;
        ImVec2 sv_max(origin.x + sv_size, origin.y + sv_size);
        ImGui::SetCursorScreenPos(sv_min);
        ImGui::InvisibleButton("##sv", ImVec2(sv_size, sv_size));
        if (ImGui::IsItemActive())
        {
            ImVec2 mp = ImGui::GetIO().MousePos;
            s = (mp.x - sv_min.x) / sv_size;
            v = 1.f - (mp.y - sv_min.y) / sv_size;
            if (s < 0.f) s = 0.f;
            if (s > 1.f) s = 1.f;
            if (v < 0.f) v = 0.f;
            if (v > 1.f) v = 1.f;
            changed = true;
        }

        float hr, hg, hb;
        ImGui::ColorConvertHSVtoRGB(h, 1.f, 1.f, hr, hg, hb);
        ImU32 hue_col = ImGui::GetColorU32(ImVec4(hr, hg, hb, 1.f));
        draw->AddRectFilledMultiColor(sv_min, sv_max, IM_COL32(255, 255, 255, 255), hue_col, hue_col, IM_COL32(255, 255, 255, 255));
        draw->AddRectFilledMultiColor(sv_min, sv_max, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 255), IM_COL32(0, 0, 0, 255));
        draw->AddRect(sv_min, sv_max, ImGui::GetColorU32(ImVec4(0.22f, 0.22f, 0.22f, 1.f)));
        draw_sv_marker(draw, ImVec2(sv_min.x + s * sv_size, sv_min.y + (1.f - v) * sv_size));

        ImVec2 hue_min(sv_max.x + gap, origin.y);
        ImVec2 hue_max(hue_min.x + hue_w, origin.y + sv_size);
        ImGui::SetCursorScreenPos(hue_min);
        ImGui::InvisibleButton("##hue", ImVec2(hue_w, sv_size));
        if (ImGui::IsItemActive())
        {
            float t = (ImGui::GetIO().MousePos.y - hue_min.y) / sv_size;
            if (t < 0.f) t = 0.f;
            if (t > 1.f) t = 1.f;
            h = t;
            if (h >= 1.f) h = 0.999f;
            changed = true;
        }

        static const ImU32 hues[7] = {
            IM_COL32(255, 0, 0, 255),
            IM_COL32(255, 255, 0, 255),
            IM_COL32(0, 255, 0, 255),
            IM_COL32(0, 255, 255, 255),
            IM_COL32(0, 0, 255, 255),
            IM_COL32(255, 0, 255, 255),
            IM_COL32(255, 0, 0, 255)
        };
        for (int i = 0; i < 6; ++i)
        {
            float y0 = hue_min.y + sv_size * (i / 6.f);
            float y1 = hue_min.y + sv_size * ((i + 1) / 6.f);
            draw->AddRectFilledMultiColor(ImVec2(hue_min.x, y0), ImVec2(hue_max.x, y1), hues[i], hues[i], hues[i + 1], hues[i + 1]);
        }
        draw->AddRect(hue_min, hue_max, ImGui::GetColorU32(ImVec4(0.22f, 0.22f, 0.22f, 1.f)));
        draw_hue_marker(draw, hue_min.x, hue_max.x, hue_min.y + h * sv_size);

        if (changed)
            ImGui::ColorConvertHSVtoRGB(h, s, v, color[0], color[1], color[2]);

        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + sv_size + gap));
        ImGui::Dummy(ImVec2(total_w, 0.f));
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + sv_size + gap));
        if (alpha_bar_h(&color[3], total_w, alpha_h, ImVec4(color[0], color[1], color[2], 1.f)))
            changed = true;

        return changed;
    }

    bool color_picker(const char* label, float color[4])
    {
        ImGui::PushID(label);
        push_picker_style();
        bool changed = compact_picker(color);
        pop_picker_style();
        ImGui::PopID();
        return changed;
    }

    bool color_edit(const char* label, float color[4])
    {
        ImGui::PushID(label);
        push_picker_style();
        bool changed = compact_picker(color);
        pop_picker_style();
        ImGui::PopID();
        return changed;
    }

    bool checkbox_color(const char* label, bool* value, float color[4])
    {
        constexpr float label_gap = 6.f;
        constexpr float swatch_w = 18.f;

        ImGui::PushID(label);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        float avail_w = ImGui::CalcItemWidth();
        if (avail_w < 1.f)
            avail_w = ImGui::GetContentRegionAvail().x;

        ImVec2 text_size = ImGui::CalcTextSize(label);
        float box_size = text_size.y;
        float row_h = box_size;

        ImGui::InvisibleButton("##cb", ImVec2(avail_w, row_h));
        bool hovered = ImGui::IsItemHovered();

        ImVec2 box_min = pos;
        ImVec2 box_max(box_min.x + box_size, box_min.y + box_size);
        ImVec2 sw_min(pos.x + avail_w - swatch_w, pos.y);
        ImVec2 sw_max(sw_min.x + swatch_w, sw_min.y + box_size);

        ImVec2 mouse = ImGui::GetIO().MousePos;
        bool over_sw = mouse.x >= sw_min.x && mouse.x <= sw_max.x && mouse.y >= sw_min.y && mouse.y <= sw_max.y;
        bool over_left = hovered && !over_sw;

        bool changed = false;
        if (over_left && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            *value = !*value;
            changed = true;
        }
        if (over_sw && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            ImGui::OpenPopup("##cp");

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (*value)
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)));
        else
        {
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.35f)));
            if (over_left)
                draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
        }
        draw->AddRect(box_min, box_max, ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 1.f)));

        ImU32 text_col = ImGui::GetColorU32(*value ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f));
        text_outlined(draw, ImVec2(box_max.x + label_gap, pos.y), text_col, label);

        draw->AddRectFilled(sw_min, sw_max, ImGui::GetColorU32(ImVec4(color[0], color[1], color[2], color[3])));
        draw->AddRect(sw_min, sw_max, ImGui::GetColorU32(over_sw ? ImVec4(1.f, 1.f, 1.f, 0.45f) : ImVec4(0.35f, 0.35f, 0.35f, 1.f)));

        push_picker_style();
        if (ImGui::BeginPopup("##cp", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (compact_picker(color))
                changed = true;
            ImGui::EndPopup();
        }
        pop_picker_style();

        ImGui::PopID();
        return changed;
    }

    bool checkbox_color2(const char* label, bool* value, float color1[4], float color2[4])
    {
        constexpr float label_gap = 6.f;
        constexpr float swatch_w = 18.f;
        constexpr float swatch_gap = 4.f;

        ImGui::PushID(label);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        float avail_w = ImGui::CalcItemWidth();
        if (avail_w < 1.f)
            avail_w = ImGui::GetContentRegionAvail().x;

        ImVec2 text_size = ImGui::CalcTextSize(label);
        float box_size = text_size.y;
        float row_h = box_size;

        ImGui::InvisibleButton("##cb", ImVec2(avail_w, row_h));
        bool hovered = ImGui::IsItemHovered();

        ImVec2 box_min = pos;
        ImVec2 box_max(box_min.x + box_size, box_min.y + box_size);
        ImVec2 sw2_min(pos.x + avail_w - swatch_w, pos.y);
        ImVec2 sw2_max(sw2_min.x + swatch_w, sw2_min.y + box_size);
        ImVec2 sw1_min(sw2_min.x - swatch_gap - swatch_w, pos.y);
        ImVec2 sw1_max(sw1_min.x + swatch_w, sw1_min.y + box_size);

        ImVec2 mouse = ImGui::GetIO().MousePos;
        auto over = [&](ImVec2 mn, ImVec2 mx) {
            return mouse.x >= mn.x && mouse.x <= mx.x && mouse.y >= mn.y && mouse.y <= mx.y;
        };
        bool over_sw1 = over(sw1_min, sw1_max);
        bool over_sw2 = over(sw2_min, sw2_max);
        bool over_left = hovered && !over_sw1 && !over_sw2;

        bool changed = false;
        if (over_left && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            *value = !*value;
            changed = true;
        }
        if (over_sw1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            ImGui::OpenPopup("##cp1");
        if (over_sw2 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            ImGui::OpenPopup("##cp2");

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (*value)
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)));
        else
        {
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.35f)));
            if (over_left)
                draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
        }
        draw->AddRect(box_min, box_max, ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 1.f)));

        ImU32 text_col = ImGui::GetColorU32(*value ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f));
        text_outlined(draw, ImVec2(box_max.x + label_gap, pos.y), text_col, label);

        draw->AddRectFilled(sw1_min, sw1_max, ImGui::GetColorU32(ImVec4(color1[0], color1[1], color1[2], color1[3])));
        draw->AddRect(sw1_min, sw1_max, ImGui::GetColorU32(over_sw1 ? ImVec4(1.f, 1.f, 1.f, 0.45f) : ImVec4(0.35f, 0.35f, 0.35f, 1.f)));
        draw->AddRectFilled(sw2_min, sw2_max, ImGui::GetColorU32(ImVec4(color2[0], color2[1], color2[2], color2[3])));
        draw->AddRect(sw2_min, sw2_max, ImGui::GetColorU32(over_sw2 ? ImVec4(1.f, 1.f, 1.f, 0.45f) : ImVec4(0.35f, 0.35f, 0.35f, 1.f)));

        push_picker_style();
        if (ImGui::BeginPopup("##cp1", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (compact_picker(color1))
                changed = true;
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("##cp2", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (compact_picker(color2))
                changed = true;
            ImGui::EndPopup();
        }
        pop_picker_style();

        ImGui::PopID();
        return changed;
    }
}
