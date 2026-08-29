#include "pch.h"
#include "glass.h"
#include "esp_preview_window.h"
#include "widgets/text.h"
#include "imgui.h"
#include "features/visuals/ESPPreview.h"

namespace gui
{
    static const ImVec4 border_outer = ImVec4(0.13f, 0.13f, 0.13f, 1.f);

    void render_esp_preview_window(bool* open, ImVec2 anchor_pos, ImVec2 anchor_size, float anim)
    {
        if (!open || !*open)
            return;

        constexpr float title_h = 26.f;
        constexpr float margin = 3.f;
        constexpr float gap = 8.f;

        // Ã‘â‚¬ÃÂ°ÃÂ·ÃÂ¼ÃÂµÃ‘â‚¬ ÃÂ´ÃÂµÃ‘â‚¬ÃÂ¶ÃÂ¸ÃÂ¼ Ã‘ÂÃÂ²ÃÂ¾ÃÂ¹, ÃÂº ÃÂ¼ÃÂµÃÂ½Ã‘Å½ ÃÂ¿Ã‘â‚¬ÃÂ¸ÃÂ²Ã‘ÂÃÂ·ÃÂ°ÃÂ½ÃÂ° Ã‘â€šÃÂ¾ÃÂ»Ã‘Å’ÃÂºÃÂ¾ ÃÂ¿ÃÂ¾ÃÂ·ÃÂ¸Ã‘â€ ÃÂ¸Ã‘Â
        constexpr float win_w = 356.f;
        constexpr float win_h = 504.f;

        // slide-out: at anim=0 the window sits fully behind the main GUI and
        // emerges to the right as anim approaches 1
        const float slide = (1.f - anim) * (win_w + gap + 12.f);
        ImGui::SetNextWindowPos(ImVec2(anchor_pos.x + anchor_size.x + gap - slide, anchor_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);   // border is drawn manually so it gets masked too
        ImGui::PushStyleColor(ImGuiCol_Border, border_outer);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));  // bg drawn manually so it can be masked
        bool visible = ImGui::Begin("##esp_preview_window", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);   // border size + padding

        if (!visible)
        {
            ImGui::End();
            ImGui::PopStyleVar();   // alpha
            return;
        }

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Mask: nothing is drawn left of the menu's right edge, so the panel
        // looks like it slides out from underneath the menu instead of showing
        // through its translucent backdrop.
        const float reveal_x = anchor_pos.x + anchor_size.x;
        const ImVec2 clip_min(reveal_x, wp.y - 2.f);
        const ImVec2 clip_max(wp.x + ws.x + 2.f, wp.y + ws.y + 2.f);
        draw->PushClipRect(clip_min, clip_max, false);

        draw->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y),
            IM_COL32(14, 15, 18, (int)(255.f * 0.34f * anim)), 8.f);
        draw->AddRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y),
            IM_COL32(33, 33, 33, (int)(255 * anim)), 8.f, 0, 1.2f);

        // acrylic backdrop for this window (only once fully revealed; the
        // OS-level blur region cannot be clipped by the mask)
        if (anim > 0.99f)
            glass::add_rect(wp.x, wp.y, ws.x, ws.y, 8.f);

        draw->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + title_h), IM_COL32(20, 20, 20, (int)(255 * anim)));
        ImVec2 title_ts = ImGui::CalcTextSize("esp preview");
        widgets::text_outlined(draw,
            ImVec2(wp.x + (ws.x - title_ts.x) * 0.5f, wp.y + (title_h - title_ts.y) * 0.5f),
            IM_COL32(230, 230, 230, (int)(255 * anim)), "esp preview");

        ImVec2 xsz = ImGui::CalcTextSize("X");
        ImVec2 xmin(wp.x + ws.x - xsz.x - 14.f, wp.y);
        ImGui::SetCursorScreenPos(xmin);
        ImGui::InvisibleButton("##esp_preview_close", ImVec2(xsz.x + 14.f, title_h));
        bool xhov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            *open = false;
        widgets::text_outlined(draw, ImVec2(xmin.x + 7.f, wp.y + (title_h - xsz.y) * 0.5f),
            xhov ? IM_COL32(255, 255, 255, (int)(255 * anim))
                 : IM_COL32(160, 160, 160, (int)(255 * anim)), "X");

        float body_top = title_h + margin;
        float body_h = ws.y - body_top - margin;
        float body_w = ws.x - margin * 2.f;
        if (body_h < 1.f)
            body_h = 1.f;

        ImGui::SetCursorPos(ImVec2(margin, body_top));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##esp_preview_body", ImVec2(body_w, body_h),
            ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        // the child has its own draw list, so the mask has to be applied to it
        // as well, otherwise the preview would bleed over the menu
        ImGui::GetWindowDrawList()->PushClipRect(clip_min, clip_max, false);
        Cheat::Visuals::ESPPreview::Render();
        ImGui::GetWindowDrawList()->PopClipRect();
        ImGui::EndChild();
        ImGui::PopStyleVar();

        draw->PopClipRect();
        ImGui::End();
        ImGui::PopStyleVar();   // alpha
    }
}
