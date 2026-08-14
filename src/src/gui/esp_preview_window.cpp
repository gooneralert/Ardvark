#include "pch.h"
#include "esp_preview_window.h"
#include "widgets/text.h"
#include "imgui.h"
#include "features/visuals/ESPPreview.h"

namespace gui
{
    static const ImVec4 border_outer = ImVec4(0.13f, 0.13f, 0.13f, 1.f);

    void render_esp_preview_window(bool* open, ImVec2 anchor_pos, ImVec2 anchor_size)
    {
        if (!open || !*open)
            return;

        constexpr float title_h = 26.f;
        constexpr float margin = 3.f;
        constexpr float gap = 8.f;

        // размер держим свой, к меню привязана только позиция
        constexpr float win_w = 356.f;
        constexpr float win_h = 504.f;

        ImGui::SetNextWindowPos(ImVec2(anchor_pos.x + anchor_size.x + gap, anchor_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_outer);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.f));
        bool visible = ImGui::Begin("##esp_preview_window", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        if (!visible)
        {
            ImGui::End();
            return;
        }

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + title_h), IM_COL32(20, 20, 20, 255));
        ImVec2 title_ts = ImGui::CalcTextSize("esp preview");
        widgets::text_outlined(draw,
            ImVec2(wp.x + (ws.x - title_ts.x) * 0.5f, wp.y + (title_h - title_ts.y) * 0.5f),
            IM_COL32(230, 230, 230, 255), "esp preview");

        ImVec2 xsz = ImGui::CalcTextSize("X");
        ImVec2 xmin(wp.x + ws.x - xsz.x - 14.f, wp.y);
        ImGui::SetCursorScreenPos(xmin);
        ImGui::InvisibleButton("##esp_preview_close", ImVec2(xsz.x + 14.f, title_h));
        bool xhov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            *open = false;
        widgets::text_outlined(draw, ImVec2(xmin.x + 7.f, wp.y + (title_h - xsz.y) * 0.5f),
            xhov ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 160, 255), "X");

        float body_top = title_h + margin;
        float body_h = ws.y - body_top - margin;
        float body_w = ws.x - margin * 2.f;
        if (body_h < 1.f)
            body_h = 1.f;

        ImGui::SetCursorPos(ImVec2(margin, body_top));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##esp_preview_body", ImVec2(body_w, body_h),
            ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        Cheat::Visuals::ESPPreview::Render();
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::End();
    }
}
