#pragma once

namespace gui
{
    void setup_style();
    void render();

    bool menu_visible();
    void set_menu_visible(bool v);
    bool any_window_visible();
    bool point_over_ui(float x, float y);

    bool menu_open();
    void set_menu_open(bool open);
    bool any_ui_open();
    ImVec2 menu_pos();      // last rendered menu position (0,0 until first frame)
bool music_visible();   // music player card currently toggled on
}
