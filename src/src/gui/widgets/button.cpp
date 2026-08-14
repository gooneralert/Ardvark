#include "pch.h"
#include "button.h"
#include "text.h"
#include "imgui.h"

namespace widgets
{
    bool button(const char* label)
    {
        ImGui::PushID(label);

        float width = ImGui::CalcItemWidth();
        if (width < 1.f)
            width = ImGui::GetContentRegionAvail().x;

        constexpr float label_gap = 6.f;
        constexpr float pad_x = 6.f;
        constexpr float pad_y = 3.f;

        bool has_label = label[0] != '#' || label[1] != '#';
        const char* btn_text = has_label ? "..." : label + 2;
        if (!btn_text[0])
            btn_text = "...";

        ImVec2 label_size = has_label ? ImGui::CalcTextSize(label) : ImVec2(0.f, 0.f);
        ImVec2 btn_text_size = ImGui::CalcTextSize(btn_text);
        float row_h = btn_text_size.y + pad_y * 2.f;

        float label_w = has_label ? label_size.x + label_gap : 0.f;
        float btn_w = width - label_w;
        if (btn_w < 40.f) btn_w = 40.f;

        ImVec2 pos = ImGui::GetCursorScreenPos();

        if (has_label)
            text_outlined(ImGui::GetWindowDrawList(), ImVec2(pos.x, pos.y + (row_h - label_size.y) * 0.5f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)), label);

        ImVec2 btn_min(pos.x + label_w, pos.y);
        ImVec2 btn_max(btn_min.x + btn_w, btn_min.y + row_h);

        ImGui::SetCursorScreenPos(btn_min);
        ImGui::InvisibleButton("##btn", ImVec2(btn_w, row_h));
        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked();

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(btn_min, btn_max, ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.f)));
        if (held)
            draw->AddRectFilled(btn_min, btn_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.10f)));
        else if (hovered)
            draw->AddRectFilled(btn_min, btn_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
        draw->AddRect(btn_min, btn_max, ImGui::GetColorU32(ImVec4(0.22f, 0.22f, 0.22f, 1.f)));

        text_outlined(draw, ImVec2(btn_min.x + (btn_w - btn_text_size.x) * 0.5f, btn_min.y + (row_h - btn_text_size.y) * 0.5f), ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.f)), btn_text);

        ImGui::PopID();
        return clicked;
    }
}
