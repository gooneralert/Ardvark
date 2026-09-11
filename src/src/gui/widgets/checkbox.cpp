#include "pch.h"
#include "checkbox.h"
#include "text.h"
#include "imgui.h"

namespace widgets
{
    // matcha-style row: label left, pill switch right
    bool checkbox(const char* label, bool* value)
    {
        const bool has_label = label && !(label[0] == '#' && label[1] == '#');

        ImGui::PushID(label);

        float width = ImGui::CalcItemWidth();
        if (width < 1.f)
            width = ImGui::GetContentRegionAvail().x;

        constexpr float row_h = 20.f;
        constexpr float sw_h  = 18.f;
        constexpr float sw_w  = 36.f;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 sw_min(pos.x + width - sw_w, pos.y + (row_h - sw_h) * 0.5f);
        const ImVec2 sw_max(sw_min.x + sw_w, sw_min.y + sw_h);

        ImGui::InvisibleButton("##sw", ImVec2(width, row_h));
        bool clicked = ImGui::IsItemClicked();
        if (clicked)
            *value = !*value;
        bool hovered = ImGui::IsItemHovered();

        ImDrawList* draw = ImGui::GetWindowDrawList();
<<<<<<< Updated upstream
        if (*value)
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)));
        else
        {
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.35f)));
            if (hovered)
                draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
        }
        draw->AddRect(box_min, box_max, ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 1.f)));
=======
>>>>>>> Stashed changes

        const ImU32 track = *value
            ? IM_COL32(232, 121, 249, 235)
            : IM_COL32(255, 255, 255, hovered ? 36 : 24);
        draw->AddRectFilled(sw_min, sw_max, track, sw_h * 0.5f);

        const float knob = 12.f;
        const float kx = *value ? (sw_max.x - 3.f - knob) : (sw_min.x + 3.f);
        draw->AddCircleFilled(ImVec2(kx + knob * 0.5f, sw_min.y + sw_h * 0.5f), knob * 0.5f, IM_COL32(233, 233, 236, 255));

        if (has_label)
        {
            const ImU32 text_col = ImGui::GetColorU32(*value ? ImVec4(0.94f, 0.94f, 0.96f, 1.f)
                              : (hovered ? ImVec4(0.86f, 0.86f, 0.89f, 1.f) : ImVec4(0.78f, 0.78f, 0.81f, 1.f)));
            text_outlined(draw, ImVec2(pos.x, pos.y + (row_h - ImGui::GetTextLineHeight()) * 0.5f), text_col, label);
        }

        ImGui::PopID();
        return clicked;
    }
}
