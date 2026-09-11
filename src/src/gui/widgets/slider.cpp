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
        const bool has_label = label && !(label[0] == '#' && label[1] == '#');
        ImGui::PushID(label);

        float width = ImGui::CalcItemWidth();
        if (width < 1.f)
            width = ImGui::GetContentRegionAvail().x;

        constexpr float row_h = 22.f;
        constexpr float box_w = 54.f;
        constexpr float gap   = 10.f;

        char value_buf[64];
        snprintf(value_buf, sizeof(value_buf), fmt ? fmt : "%.2f", *value);

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 ts = has_label ? ImGui::CalcTextSize(label) : ImVec2(0.f, 0.f);

        const float box_x0   = pos.x + width - box_w;
        const float track_x0 = pos.x + (has_label ? ts.x + gap : gap);
        const float track_x1 = box_x0 - gap;
        const float track_y  = pos.y + row_h * 0.5f;

        // interaction over the track
        ImGui::SetCursorScreenPos(ImVec2(track_x0 - 4.f, pos.y));
        ImGui::InvisibleButton("##trk", ImVec2((track_x1 + 4.f) - (track_x0 - 4.f), row_h));
        bool active = ImGui::IsItemActive();
        bool hovered = ImGui::IsItemHovered();
        bool changed = false;

        if (active && max > min && track_x1 > track_x0)
        {
            float t = (ImGui::GetIO().MousePos.x - track_x0) / (track_x1 - track_x0);
            if (t < 0.f) t = 0.f;
            if (t > 1.f) t = 1.f;
            const float nv = min + t * (max - min);
            if (nv != *value)
            {
                *value = nv;
                changed = true;
            }
        }

        float t = (max > min) ? ((*value - min) / (max - min)) : 0.f;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        const float fill_x = track_x0 + (track_x1 - track_x0) * t;

        ImDrawList* draw = ImGui::GetWindowDrawList();

<<<<<<< Updated upstream
        draw->AddRectFilled(bar_min, bar_max, ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.12f, 1.f)));
        if (hovered || active)
            draw->AddRectFilled(bar_min, bar_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)));
=======
        if (has_label)
            text_outlined(draw, ImVec2(pos.x, pos.y + (row_h - ts.y) * 0.5f),
                ImGui::GetColorU32(ImVec4(0.92f, 0.92f, 0.94f, 1.f)), label);
>>>>>>> Stashed changes

        // thin track + accent fill + white knob
        draw->AddRectFilled(ImVec2(track_x0, track_y - 1.5f), ImVec2(track_x1, track_y + 1.5f), IM_COL32(255, 255, 255, 30), 1.5f);
        if (t > 0.f)
<<<<<<< Updated upstream
            draw->AddRectFilled(bar_min, ImVec2(fill_x, bar_max.y), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.85f)));

        draw->AddRect(bar_min, bar_max, ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 1.f)));
=======
            draw->AddRectFilled(ImVec2(track_x0, track_y - 1.5f), ImVec2(fill_x, track_y + 1.5f), IM_COL32(232, 121, 249, 255), 1.5f);
        draw->AddCircleFilled(ImVec2(fill_x, track_y), 5.5f, IM_COL32(255, 255, 255, 255));

        // value box
        const ImVec2 b0(box_x0, pos.y), b1(box_x0 + box_w, pos.y + row_h);
        draw->AddRectFilled(b0, b1, IM_COL32(30, 30, 34, 235), 6.f);
        draw->AddRect(b0, b1, IM_COL32(255, 255, 255, 24), 6.f);
        const ImVec2 vs = ImGui::CalcTextSize(value_buf);
        draw->AddText(ImVec2((b0.x + b1.x - vs.x) * 0.5f, (b0.y + b1.y - vs.y) * 0.5f), IM_COL32(240, 240, 242, 255), value_buf);
>>>>>>> Stashed changes

        // normalize the cursor for the next row
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + row_h));

        ImGui::PopID();
        return changed;
    }
}
