#include "pch.h"
#include "widgets.h"
#include "../resources/fonts/fonts.h"
#include "../colors/colors.h"
#include "imgui_internal.h"

namespace {
    int* first_row_flag()
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return nullptr;
        return window->StateStorage.GetIntRef(window->GetID("##widgets_menu_row_first"), 1);
    }
}

namespace widgets {
    void menu_row_reset()
    {
        if (int* first = first_row_flag())
            *first = 1;
    }

    void menu_row()
    {
        ImGui::SetCursorPosX(k_menu_pad_x);

        int* first = first_row_flag();
        if (first && *first)
        {
            *first = 0;
            // первый ряд — не тянуть вверх (кнопки наезжают на бордер)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + k_menu_top_pad);
        }
        else
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - k_menu_row_pull);
        }
    }

    bool menu_color_row(const char* label, float color[4], const char* id)
    {
        if (!label || !color || !id)
            return false;

        menu_row();

        ImFont* font = fonts::ui();
        const float fs = fonts::ui_size(font);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, label);
        const float text_oy = ImFloor((k_menu_row_h - tsz.y) * 0.5f);

        draw_outlined_text(
            ImGui::GetWindowDrawList(), font, fs,
            ImVec2(ImFloor(pos.x), ImFloor(pos.y + text_oy)),
            colors::text_active_u32(), label);

        ImGui::Dummy(ImVec2(ImMax(tsz.x, 1.0f), k_menu_row_h));
        const float next_y = ImGui::GetCursorPosY();

        const float row_y = color_picker_row_y();
        same_line_color_picker(row_y, 0, 1);
        const bool changed = color_edit4(id, color);

        ImGui::SetCursorPosY(next_y);
        ImGui::SetCursorPosX(k_menu_pad_x);
        return changed;
    }
}
