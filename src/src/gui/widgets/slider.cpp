#include "pch.h"
#include "slider.h"
#include "text.h"
#include "imgui.h"
#include <stdio.h>

namespace widgets
{
    bool slider_int(const char* label, int* value, int min, int max)
    {
        float f = (float)*value;
        bool changed = slider_float(label, &f, (float)min, (float)max, "%.0f");
        *value = (int)(f + (f >= 0.f ? 0.5f : -0.5f));
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        return changed;
    }

    bool slider_float(const char* label, float* value, float min, float max, const char* fmt)
    {
        constexpr float bar_height = 14.f;
        constexpr float gap = 1.f;

        ImGui::PushID(label);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        float width = ImGui::CalcItemWidth();
        if (width < 1.f)
            width = ImGui::GetContentRegionAvail().x;
        ImVec2 text_size = ImGui::CalcTextSize(label);

        char value_buf[64];
        snprintf(value_buf, sizeof(value_buf), fmt ? fmt : "%.2f", *value);
        ImVec2 value_size = ImGui::CalcTextSize(value_buf);

        float value_overlap = value_size.y * 0.55f;
        float total_h = text_size.y + gap + bar_height + (value_size.y - value_overlap);
        ImGui::InvisibleButton("##sl", ImVec2(width, total_h));
        bool active = ImGui::IsItemActive();
        bool hovered = ImGui::IsItemHovered();
        bool changed = false;

        ImVec2 bar_min(pos.x, pos.y + text_size.y + gap);
        ImVec2 bar_max(pos.x + width, bar_min.y + bar_height);

        if (active && max > min)
        {
            float t = (ImGui::GetIO().MousePos.x - bar_min.x) / (bar_max.x - bar_min.x);
            if (t < 0.f) t = 0.f;
            if (t > 1.f) t = 1.f;
            float new_v = min + t * (max - min);
            if (new_v != *value)
            {
                *value = new_v;
                changed = true;
            }
        }

        float t = (max > min) ? ((*value - min) / (max - min)) : 0.f;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;

        float fill_x = bar_min.x + (bar_max.x - bar_min.x) * t;

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImU32 text_col = ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f));
        text_outlined(draw, pos, text_col, label);

        draw->AddRectFilled(bar_min, bar_max, ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.12f, 1.f)));
        if (hovered || active)
            draw->AddRectFilled(bar_min, bar_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)));

        if (t > 0.f)
            draw->AddRectFilled(bar_min, ImVec2(fill_x, bar_max.y), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.85f)));

        draw->AddRect(bar_min, bar_max, ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 1.f)));

        float value_x = fill_x - value_size.x * 0.5f;
        if (value_x < bar_min.x) value_x = bar_min.x;
        if (value_x + value_size.x > bar_max.x) value_x = bar_max.x - value_size.x;
        text_outlined(draw, ImVec2(value_x, bar_max.y - value_overlap), text_col, value_buf);

        ImGui::PopID();
        return changed;
    }
}
