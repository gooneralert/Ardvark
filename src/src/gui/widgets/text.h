#pragma once

#include "imgui.h"

namespace widgets
{
    inline void text_outlined(ImDrawList* draw, ImVec2 pos, ImU32 col, const char* text)
    {
        if (!draw || !text)
            return;

        ImU32 outline = IM_COL32(0, 0, 0, 220);
        const float o = 1.f;
        draw->AddText(ImVec2(pos.x - o, pos.y), outline, text);
        draw->AddText(ImVec2(pos.x + o, pos.y), outline, text);
        draw->AddText(ImVec2(pos.x, pos.y - o), outline, text);
        draw->AddText(ImVec2(pos.x, pos.y + o), outline, text);
        draw->AddText(pos, col, text);
    }

    inline void draw_outlined_text(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, ImU32 color, const char* text, float extra_width = 0.0f)
    {
        (void)extra_width;
        if (!draw_list || !text)
            return;
        if (!font)
            font = ImGui::GetFont();
        if (font_size <= 0.f)
            font_size = font->LegacySize > 0.f ? font->LegacySize : ImGui::GetFontSize();

        ImU32 outline = IM_COL32(0, 0, 0, 220);
        const float o = 1.f;
        draw_list->AddText(font, font_size, ImVec2(pos.x - o, pos.y), outline, text);
        draw_list->AddText(font, font_size, ImVec2(pos.x + o, pos.y), outline, text);
        draw_list->AddText(font, font_size, ImVec2(pos.x, pos.y - o), outline, text);
        draw_list->AddText(font, font_size, ImVec2(pos.x, pos.y + o), outline, text);
        draw_list->AddText(font, font_size, pos, color, text);
    }
}
