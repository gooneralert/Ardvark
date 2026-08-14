#include "pch.h"
#include "checkbox.h"
#include "text.h"
#include "imgui.h"

namespace widgets
{
    bool checkbox(const char* label, bool* value)
    {
        constexpr float label_gap = 6.f;

        ImGui::PushID(label);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 text_size = ImGui::CalcTextSize(label);
        float box_size = text_size.y;

        ImGui::InvisibleButton("##cb", ImVec2(box_size + label_gap + text_size.x, box_size));
        bool clicked = ImGui::IsItemClicked();
        if (clicked)
            *value = !*value;
        bool hovered = ImGui::IsItemHovered();

        ImVec2 box_min = pos;
        ImVec2 box_max(box_min.x + box_size, box_min.y + box_size);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (*value)
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)));
        else
        {
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.35f)));
            if (hovered)
                draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
        }
        draw->AddRect(box_min, box_max, ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 1.f)));

        ImU32 text_col = ImGui::GetColorU32(*value ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f));
        text_outlined(draw, ImVec2(box_max.x + label_gap, pos.y), text_col, label);

        ImGui::PopID();
        return clicked;
    }
}
