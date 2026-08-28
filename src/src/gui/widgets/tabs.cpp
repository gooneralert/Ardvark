#include "pch.h"
#include "tabs.h"
#include "text.h"
#include "imgui.h"

namespace widgets
{
    void sidebar_tabs(const std::vector<const char*>& items, int* selected, float width)
    {
        constexpr float item_height = 34.f;
        constexpr float text_padding_x = 10.f;
        constexpr float active_inset = 2.f;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f, 0.f, 0.f, 0.f));

        ImGui::BeginChild("##sidebar_tabs", ImVec2(width, 0.f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

        ImDrawList* draw = ImGui::GetWindowDrawList();

        for (int i = 0; i < (int)items.size(); ++i)
        {
            bool active = (*selected == i);
            ImGui::PushID(i);

            ImVec2 item_pos = ImGui::GetCursorScreenPos();
            ImVec2 item_max(item_pos.x + width, item_pos.y + item_height);

            if (ImGui::Selectable("", active, 0, ImVec2(width, item_height)))
                *selected = i;

            if (ImGui::IsItemHovered())
                draw->AddRectFilled(item_pos, item_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)), 6.f);

            if (active)
                draw->AddRect(ImVec2(item_pos.x + active_inset, item_pos.y + active_inset), ImVec2(item_max.x - active_inset, item_max.y - active_inset), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.35f)), 6.f);

            ImVec2 text_size = ImGui::CalcTextSize(items[i]);
            ImU32 text_col = ImGui::GetColorU32(active ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f));
            text_outlined(draw, ImVec2(item_pos.x + text_padding_x, item_pos.y + (item_height - text_size.y) * 0.5f), text_col, items[i]);

            ImGui::PopID();
        }

        ImGui::EndChild();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    // matcha-style top tab bar: tall items spread across the full width,
    // active tab gets a rounded pill highlight
    void top_tabs(const std::vector<const char*>& items, int* selected, float width, float height)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        const float item_width = width / (float)items.size();
        constexpr float pill_inset = 4.f;

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f, 0.f, 0.f, 0.f));

        for (int i = 0; i < (int)items.size(); ++i)
        {
            bool active = (*selected == i);
            ImGui::PushID(i);

            ImVec2 item_min(origin.x + item_width * (float)i, origin.y);
            ImVec2 item_max(item_min.x + item_width, origin.y + height);

            ImGui::SetCursorScreenPos(item_min);
            ImGui::InvisibleButton("##top_tab", ImVec2(item_width, height));
            bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked())
                *selected = i;

            if (active)
                draw->AddRectFilled(ImVec2(item_min.x + pill_inset, item_min.y + pill_inset), ImVec2(item_max.x - pill_inset, item_max.y - pill_inset), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.14f)), (item_max.y - item_min.y) * 0.35f);
            else if (hovered)
                draw->AddRectFilled(ImVec2(item_min.x + pill_inset, item_min.y + pill_inset), ImVec2(item_max.x - pill_inset, item_max.y - pill_inset), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)), (item_max.y - item_min.y) * 0.35f);

            ImVec2 ts = ImGui::CalcTextSize(items[i]);
            ImU32 col = ImGui::GetColorU32(active ? ImVec4(1.f, 1.f, 1.f, 1.f) : (hovered ? ImVec4(0.85f, 0.85f, 0.85f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f)));
            text_outlined(draw, ImVec2(item_min.x + (item_width - ts.x) * 0.5f, item_min.y + (height - ts.y) * 0.5f), col, items[i]);

            ImGui::PopID();
        }

        ImGui::PopStyleColor(3);
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height));
        ImGui::Dummy(ImVec2(width, 0.f));
    }

    void horizontal_tabs(const std::vector<const char*>& items, int* selected, float width, float height)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.f, 1.f, 1.f, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1.f, 1.f, 1.f, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f, 0.f, 0.f, 0.f));

        ImGui::BeginChild("##horizontal_tabs", ImVec2(width, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

        float item_width = width / (float)items.size();

        for (int i = 0; i < (int)items.size(); ++i)
        {
            bool active = (*selected == i);
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f));

            ImVec2 item_pos = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("", active, 0, ImVec2(item_width, height)))
                *selected = i;

            ImVec2 text_size = ImGui::CalcTextSize(items[i]);
            text_outlined(ImGui::GetWindowDrawList(), ImVec2(item_pos.x + (item_width - text_size.x) * 0.5f, item_pos.y + (height - text_size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), items[i]);

            ImGui::PopStyleColor();
            ImGui::PopID();

            if (i + 1 < (int)items.size())
                ImGui::SameLine(0.f, 0.f);
        }

        ImGui::EndChild();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
}
