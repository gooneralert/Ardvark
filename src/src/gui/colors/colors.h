#pragma once

#include "imgui.h"

namespace colors {
    extern ImVec4 accent;
    extern ImVec4 text_active;
    extern ImVec4 text_inactive;

    extern ImVec4 outer_border;
    extern ImVec4 inner_border;
    extern ImVec4 panel_fill;

    extern ImVec4 content_outer_border;
    extern ImVec4 content_inner_border;
    extern ImVec4 content_fill;
    extern ImVec4 child_fill;

    enum Theme : int {
        Theme_Default = 0,
        Theme_Gamesense,
        Theme_Fatality,
        Theme_Neverlose,
        Theme_Onetap,
        Theme_Primordial,
        Theme_Nixware,
        Theme_Aimware,
        Theme_Custom,
        Theme_Count
    };

    const char* const* ThemeNames();
    inline int ThemeNameCount() { return Theme_Count; }

    void ApplyPreset(int theme, float accent[4], float text_active[4], float text_inactive[4],
                     float outer_border[4], float inner_border[4], float panel_fill[4],
                     float content_outer[4], float content_inner[4],
                     float content_fill[4], float child_fill[4]);

    void SyncFromSettings(const float accent[4], const float text_active[4], const float text_inactive[4],
                          const float outer_border[4], const float inner_border[4], const float panel_fill[4],
                          const float content_outer[4], const float content_inner[4],
                          const float content_fill[4], const float child_fill[4]);

    ImU32 accent_u32(float alpha = 1.0f);
    ImU32 accent_gradient_row(int row, int row_count, float alpha = 1.0f);
    ImU32 header_gradient_row(int row, int row_count, float alpha = 1.0f);
    ImU32 title_gradient_row(int row, int row_count, float alpha = 1.0f);
    ImU32 text_active_u32(float alpha = 1.0f);
    ImU32 text_inactive_u32(float alpha = 1.0f);
    ImU32 widget_outline_u32(float alpha = 1.0f);
    ImU32 widget_inline_u32(float alpha = 1.0f);
    ImU32 widget_track_u32(float alpha = 1.0f);
    ImU32 widget_track_hover_u32(float alpha = 1.0f);
    ImU32 label_u32(float hover_t, float alpha = 1.0f);

    void apply_style();
    void draw_panel_background(float alpha = 1.0f);
}
