#pragma once

#include <imgui.h>

#include <cstddef>
#include <initializer_list>
#include <span>

namespace col
{
    constexpr ImU32 white(int alpha)
    {
        return IM_COL32(255, 255, 255, alpha);
    }

    constexpr ImU32 border      = white(30);
    constexpr ImU32 panel       = white(10);
    constexpr ImU32 outline     = white(25);
    constexpr ImU32 divider     = white(8);
    constexpr ImU32 control     = white(15);
    constexpr ImU32 button      = white(17);
    constexpr ImU32 hovered     = white(24);
    constexpr ImU32 frame       = white(14);
    constexpr ImU32 check       = white(46);
    constexpr ImU32 popup       = IM_COL32(40, 40, 46, 255);
    constexpr ImU32 checked     = IM_COL32(165, 93, 94, 255);
    constexpr ImU32 fill        = IM_COL32(142, 75, 75, 255);
    constexpr ImU32 text        = IM_COL32(140, 140, 150, 255);
    constexpr ImU32 bright      = IM_COL32(175, 175, 185, 255);
    constexpr ImU32 dim         = IM_COL32(80, 80, 90, 255);
    constexpr ImU32 faint       = IM_COL32(89, 89, 97, 255);
    constexpr ImU32 accent      = IM_COL32(181, 96, 96, 255);
    constexpr ImU32 accent_dim  = IM_COL32(120, 67, 68, 255);
    constexpr ImU32 tint        = IM_COL32(181, 96, 96, 18);
    constexpr ImU32 yellow      = IM_COL32(181, 150, 57, 255);
    constexpr ImU32 red         = IM_COL32(195, 75, 75, 255);
    constexpr ImU32 green       = IM_COL32(83, 153, 98, 255);
    constexpr ImU32 error       = IM_COL32(255, 0, 0, 255);
}

namespace icon
{
    constexpr const char* combat    = "";
    constexpr const char* visuals   = "";
    constexpr const char* world     = "";
    constexpr const char* character = "";
    constexpr const char* options   = "";
    constexpr const char* configs   = "";
    constexpr const char* npc       = "";
    constexpr const char* teams     = "";
    constexpr const char* users     = "";
    constexpr const char* chevron   = "";
}

namespace ui
{
    using items = std::span<const char* const>;

    constexpr float fill_height = -1.0f;

    void column(int index);
    void begin_frame();
    float frame_content_bottom();
    void begin_panel(const char* title, float height = 0);
    void begin_panel(const char* id, items tabs, int& tab, float height = 0);
    void end_panel(float gap = 9);
    void begin_box(const char* id, float height, float padding = 14);
    void end_box();

    bool checkbox(const char* label, bool& value, ImU32 color = col::text);
    bool slider(const char* label, float& value, float min, float max, const char* format = "%.0f");
    bool slider(const char* label, int& value, int min, int max);
    bool combo(const char* label, int& current, items options);
    bool button(const char* label, float width = 0, float height = 26);
    int buttons(std::initializer_list<const char*> labels);
    bool input(const char* id, char* buf, size_t size, const char* hint = "");
    bool list(const char* id, items entries, int& selected, float height);
    void label(const char* text, ImU32 color = col::text);
    void row(const char* text, ImU32 color = col::text);
    void center(const char* text, ImU32 color = col::text);
    void badge(const char* text);
    void spacing(float height);
    void separator();

    void keybind(int& key);
    void color(ImVec4& value);
    void help(const char* tip);
    bool chip(const char* label);
    void value(const char* text, ImU32 color = col::text);

    void draw_badge(ImDrawList* dl, ImVec2 pos, const char* text);
}
