#include "pch.h"
#include "combo.h"
#include "checkbox.h"
#include "text.h"
#include "imgui.h"
#include <stdio.h>
#include <string.h>

namespace widgets
{
    // matcha-style combo box visuals (rounded dark box, preview + chevron)
    static void draw_combo_box(ImDrawList* draw, const ImVec2& bmin, const ImVec2& bmax,
                               const char* text, bool hovered)
    {
        draw->AddRectFilled(bmin, bmax, IM_COL32(30, 30, 34, 235), 8.f);
        if (hovered)
            draw->AddRectFilled(bmin, bmax, IM_COL32(255, 255, 255, 10), 8.f);
        draw->AddRect(bmin, bmax, IM_COL32(255, 255, 255, 26), 8.f);

        const float th = ImGui::GetTextLineHeight();
        text_outlined(draw, ImVec2(bmin.x + 10.f, (bmin.y + bmax.y - th) * 0.5f),
            ImGui::GetColorU32(ImVec4(0.90f, 0.90f, 0.92f, 1.f)), text);

        const float cx = bmax.x - 14.f;
        const float cy = (bmin.y + bmax.y) * 0.5f;
        const ImVec2 ch[3] = { ImVec2(cx - 3.5f, cy - 2.f), ImVec2(cx, cy + 2.5f), ImVec2(cx + 3.5f, cy - 2.f) };
        draw->AddPolyline(ch, 3, IM_COL32(255, 255, 255, 120), 0, 1.4f);
    }

    static void push_popup_style()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 2.f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.05f, 0.055f, 0.065f, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1.f, 1.f, 1.f, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.f, 1.f, 1.f, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1.f, 1.f, 1.f, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.f));
    }

    // shared layout: optional label left, box right-aligned, glass popup below
    static ImVec2 combo_layout(const char* label, const char* preview,
                               const ImVec2& pos, float width, float row_h, bool* out_hovered)
    {
        const bool has_label = label && !(label[0] == '#' && label[1] == '#');
        const float text_h = ImGui::GetTextLineHeight();
        const float box_w = has_label ? (190.f < width * 0.55f ? 190.f : width * 0.55f) : width;

        const ImVec2 bmax(pos.x + width, pos.y + row_h);
        const ImVec2 bmin(bmax.x - box_w, pos.y);

        if (has_label)
            text_outlined(ImGui::GetWindowDrawList(), ImVec2(pos.x, pos.y + (row_h - text_h) * 0.5f),
                ImGui::GetColorU32(ImVec4(0.92f, 0.92f, 0.94f, 1.f)), label);

        ImGui::SetCursorScreenPos(bmin);
        ImGui::InvisibleButton("##box", ImVec2(box_w, row_h));
        *out_hovered = ImGui::IsItemHovered();

        draw_combo_box(ImGui::GetWindowDrawList(), bmin, bmax, preview, *out_hovered);
        return bmin;
    }

    bool combo(const char* label, int* current, const std::vector<const char*>& items, float height)
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
        const float text_h = ImGui::GetTextLineHeight();
        const float row_h = height > 0.f ? height : text_h + 10.f;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        bool hovered = false;
        const ImVec2 bmin = combo_layout(label, preview, pos, width, row_h, &hovered);
        const ImVec2 bmax(bmin.x + (pos.x + width - bmin.x), pos.y + row_h);

        if (ImGui::IsItemClicked())
            ImGui::OpenPopup("##cbpop");

<<<<<<< Updated upstream
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
=======
        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(bmin.x, bmax.y + 2.f));
        ImGui::SetNextWindowSize(ImVec2(bmax.x - bmin.x, 0.f));
        push_popup_style();
>>>>>>> Stashed changes

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

    bool multicombo(const char* id, bool* selected, const std::vector<const char*>& items)
    {
        if (!selected || items.empty())
            return false;

        ImGui::PushID(id);

        float width = ImGui::CalcItemWidth();
        if (width < 1.f)
            width = ImGui::GetContentRegionAvail().x;

        // preview: first selected names + "+N"
        char preview[128] = "select";
        {
            int count = 0, off = 0;
            for (int i = 0; i < (int)items.size(); ++i)
            {
                if (!selected[i]) continue;
                ++count;
                if (count <= 2)
                {
                    const int w = snprintf(preview + off, sizeof(preview) - off, "%s%s",
                        off ? ", " : "", items[i] ? items[i] : "?");
                    if (w > 0) off += w;
                }
            }
            if (count > 2)
                snprintf(preview + off, sizeof(preview) - off, " +%d", count - 2);
            else if (count == 0)
                snprintf(preview, sizeof(preview), "select");
        }

        const bool has_label = !(id[0] == '#' && id[1] == '#');
        const float text_h = ImGui::GetTextLineHeight();
        const float row_h = text_h + 10.f;
        const float box_w = has_label ? (190.f < width * 0.55f ? 190.f : width * 0.55f) : width;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 bmax(pos.x + width, pos.y + row_h);
        const ImVec2 bmin(bmax.x - box_w, pos.y);

        if (has_label)
            text_outlined(ImGui::GetWindowDrawList(), ImVec2(pos.x, pos.y + (row_h - text_h) * 0.5f),
                ImGui::GetColorU32(ImVec4(0.92f, 0.92f, 0.94f, 1.f)), id);

        ImGui::SetCursorScreenPos(bmin);
        ImGui::InvisibleButton("#mc", ImVec2(box_w, row_h));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            ImGui::OpenPopup("##mcpop");

<<<<<<< Updated upstream
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
=======
        draw_combo_box(ImGui::GetWindowDrawList(), bmin, bmax, preview, hovered);

        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(bmin.x, bmax.y + 2.f));
        ImGui::SetNextWindowSize(ImVec2(box_w, 0.f));
        push_popup_style();

        if (ImGui::BeginPopup("##mcpop", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
        {
            // frosted-glass backdrop for the dropdown
            const ImVec2 pw = ImGui::GetWindowSize();
            glass::draw(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(),
                ImVec2(ImGui::GetWindowPos().x + pw.x, ImGui::GetWindowPos().y + pw.y),
                ImGui::GetStyle().PopupRounding);

>>>>>>> Stashed changes
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
