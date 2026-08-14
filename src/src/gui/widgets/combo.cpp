#include "pch.h"
#include "combo.h"
#include "checkbox.h"
#include "text.h"
#include "imgui.h"
#include <stdio.h>
#include <string.h>

namespace widgets
{
    bool combo(const char* label, int* current, const std::vector<const char*>& items)
    {
        if (!current || items.empty())
            return false;

        ImGui::PushID(label);

        float width = ImGui::CalcItemWidth();
        if (width < 1.f)
            width = ImGui::GetContentRegionAvail().x;

        if (*current < 0) *current = 0;
        if (*current >= (int)items.size()) *current = (int)items.size() - 1;

        const char* preview = items[*current] ? items[*current] : "-";
        ImVec2 text_size = ImGui::CalcTextSize(preview);
        float height = text_size.y + 6.f;
        ImVec2 pos = ImGui::GetCursorScreenPos();

        bool open = ImGui::IsPopupOpen("##cbpop");
        ImGui::InvisibleButton("##cb", ImVec2(width, height));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            ImGui::OpenPopup("##cbpop");

        ImVec2 max(pos.x + width, pos.y + height);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, max, ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.f)));
        if (hovered || open)
            draw->AddRectFilled(pos, max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)));
        draw->AddRect(pos, max, ImGui::GetColorU32(ImVec4(0.22f, 0.22f, 0.22f, 1.f)));

        constexpr float pad_x = 6.f;
        text_outlined(draw, ImVec2(pos.x + pad_x, pos.y + (height - text_size.y) * 0.5f), ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.85f, 1.f)), preview);

        const char* arrow = open ? "-" : "+";
        ImVec2 arrow_size = ImGui::CalcTextSize(arrow);
        text_outlined(draw, ImVec2(max.x - pad_x - arrow_size.x, pos.y + (height - arrow_size.y) * 0.5f), ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.f)), arrow);

        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(pos.x, max.y + 1.f));
        ImGui::SetNextWindowSize(ImVec2(width, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 2.f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.08f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1.f, 1.f, 1.f, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.f, 1.f, 1.f, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1.f, 1.f, 1.f, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.f));

        if (ImGui::BeginPopup("##cbpop", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
        {
            for (int i = 0; i < (int)items.size(); ++i)
            {
                ImGui::PushID(i);
                bool sel = (*current == i);
                if (ImGui::Selectable(items[i], sel))
                {
                    *current = i;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        return changed;
    }

    static void build_preview(char* out, int out_size, bool* selected, const std::vector<const char*>& items)
    {
        out[0] = 0;
        bool first = true;
        for (int i = 0; i < (int)items.size(); ++i)
        {
            if (!selected[i])
                continue;
            int used = (int)strlen(out);
            int need = (int)strlen(items[i]) + (first ? 0 : 1);
            if (used + need + 1 >= out_size)
            {
                if (used + 4 <= out_size)
                    snprintf(out + used, out_size - used, "...");
                return;
            }
            if (first)
                snprintf(out, out_size, "%s", items[i]);
            else
                snprintf(out + used, out_size - used, ",%s", items[i]);
            first = false;
        }
        if (first)
            snprintf(out, out_size, "-");
    }

    bool multicombo(const char* id, bool* selected, const std::vector<const char*>& items)
    {
        ImGui::PushID(id);

        float width = ImGui::CalcItemWidth();
        if (width < 1.f)
            width = ImGui::GetContentRegionAvail().x;

        char preview[128];
        build_preview(preview, sizeof(preview), selected, items);

        ImVec2 text_size = ImGui::CalcTextSize(preview);
        float height = text_size.y + 6.f;
        ImVec2 pos = ImGui::GetCursorScreenPos();

        bool open = ImGui::IsPopupOpen("##mcpop");
        ImGui::InvisibleButton("##mc", ImVec2(width, height));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            ImGui::OpenPopup("##mcpop");

        ImVec2 max(pos.x + width, pos.y + height);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, max, ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.f)));
        if (hovered || open)
            draw->AddRectFilled(pos, max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)));
        draw->AddRect(pos, max, ImGui::GetColorU32(ImVec4(0.22f, 0.22f, 0.22f, 1.f)));

        constexpr float pad_x = 6.f;
        text_outlined(draw, ImVec2(pos.x + pad_x, pos.y + (height - text_size.y) * 0.5f), ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.85f, 1.f)), preview);

        const char* arrow = open ? "-" : "+";
        ImVec2 arrow_size = ImGui::CalcTextSize(arrow);
        text_outlined(draw, ImVec2(max.x - pad_x - arrow_size.x, pos.y + (height - arrow_size.y) * 0.5f), ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.f)), arrow);

        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(pos.x, max.y + 1.f));
        ImGui::SetNextWindowSize(ImVec2(width, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 2.f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.08f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1.f, 1.f, 1.f, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.f, 1.f, 1.f, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1.f, 1.f, 1.f, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.f));

        if (ImGui::BeginPopup("##mcpop", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
        {
            for (int i = 0; i < (int)items.size(); ++i)
            {
                ImGui::PushID(i);
                bool sel = selected[i];
                if (ImGui::Selectable(items[i], sel, ImGuiSelectableFlags_NoAutoClosePopups))
                {
                    selected[i] = !selected[i];
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        return changed;
    }

    bool checkbox_multicombo(const char* label, bool* value, bool* selected, const std::vector<const char*>& items)
    {
        float start_x = ImGui::GetCursorPosX();
        bool changed = checkbox(label, value);
        if (*value)
        {
            ImGui::SetCursorPosX(start_x);
            float width = ImGui::CalcItemWidth();
            if (width < 1.f)
                width = ImGui::GetContentRegionAvail().x;
            ImGui::PushItemWidth(width);
            changed |= multicombo("##mc", selected, items);
            ImGui::PopItemWidth();
        }
        return changed;
    }
}
