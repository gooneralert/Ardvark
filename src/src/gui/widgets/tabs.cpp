#include "pch.h"
#include "tabs.h"
#include "text.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

namespace widgets
{
    // draws a small line icon centered at c (matcha-style tab icons)
    static void draw_tab_icon(ImDrawList* draw, TabIcon kind, ImVec2 c, ImU32 col)
    {
        const float t = 1.3f;
        switch (kind)
        {
        case TABICON_CROSSHAIR: // circle + 4 ticks
            draw->AddCircle(c, 5.f, col, 0, t);
            draw->AddLine(ImVec2(c.x - 9.f, c.y), ImVec2(c.x - 5.f, c.y), col, t);
            draw->AddLine(ImVec2(c.x + 5.f, c.y), ImVec2(c.x + 9.f, c.y), col, t);
            draw->AddLine(ImVec2(c.x, c.y - 9.f), ImVec2(c.x, c.y - 5.f), col, t);
            draw->AddLine(ImVec2(c.x, c.y + 5.f), ImVec2(c.x, c.y + 9.f), col, t);
            break;
        case TABICON_EYE: // almond outline + pupil
            draw->PathArcTo(ImVec2(c.x, c.y + 4.4f), 8.2f, IM_PI * 1.16f, IM_PI * 1.84f, 12);
            draw->PathArcTo(ImVec2(c.x, c.y - 4.4f), 8.2f, IM_PI * 0.16f, IM_PI * 0.84f, 12);
            draw->PathStroke(col, 0, t);
            draw->AddCircleFilled(c, 2.4f, col);
            break;
        case TABICON_SLIDERS: // two lines with knobs
            draw->AddLine(ImVec2(c.x - 8.f, c.y - 3.5f), ImVec2(c.x + 8.f, c.y - 3.5f), col, t);
            draw->AddLine(ImVec2(c.x - 8.f, c.y + 3.5f), ImVec2(c.x + 8.f, c.y + 3.5f), col, t);
            draw->AddCircleFilled(ImVec2(c.x - 2.5f, c.y - 3.5f), 2.7f, IM_COL32(20, 20, 20, 255));
            draw->AddCircle(ImVec2(c.x - 2.5f, c.y - 3.5f), 2.7f, col, 0, t);
            draw->AddCircleFilled(ImVec2(c.x + 3.f, c.y + 3.5f), 2.7f, IM_COL32(20, 20, 20, 255));
            draw->AddCircle(ImVec2(c.x + 3.f, c.y + 3.5f), 2.7f, col, 0, t);
            break;
        case TABICON_PERSON: // head + shoulders
            draw->AddCircle(ImVec2(c.x, c.y - 3.2f), 3.f, col, 0, t);
            draw->PathArcTo(ImVec2(c.x, c.y + 10.f), 6.6f, IM_PI * 1.08f, IM_PI * 1.92f, 12);
            draw->PathStroke(col, 0, t);
            break;
        case TABICON_GEAR: // gear: ring + spokes + hub
        {
            draw->AddCircle(c, 4.2f, col, 0, t);
            for (int k = 0; k < 6; ++k)
            {
                const float a = k * IM_PI / 3.f + IM_PI * 0.08f;
                const float ca = std::cos(a), sa = std::sin(a);
                draw->AddLine(ImVec2(c.x + ca * 4.2f, c.y + sa * 4.2f), ImVec2(c.x + ca * 7.5f, c.y + sa * 7.5f), col, t);
            }
            break;
        }
        default:
            break;
        }
    }

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
    // active tab gets a rounded pill highlight. Optional per-tab icons are
    // drawn inline in front of the labels.
    void top_tabs(const std::vector<const char*>& items, int* selected, float width, float height, const TabIcon* icons)
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

            // center icon + label as a group (icon cell is 18px wide + 5px gap)
            const bool has_icon = icons && icons[i] != TABICON_NONE;
            const float group_w = ts.x + (has_icon ? 18.f + 5.f : 0.f);
            float x0 = item_min.x + (item_width - group_w) * 0.5f;
            const float cy = item_min.y + height * 0.5f;

            if (has_icon)
            {
                draw_tab_icon(draw, icons[i], ImVec2(x0 + 9.f, cy), col);
                x0 += 18.f + 5.f;
            }
            text_outlined(draw, ImVec2(x0, item_min.y + (height - ts.y) * 0.5f), col, items[i]);

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
