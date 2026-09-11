#pragma once

#include "imgui.h"
#include <vector>

namespace widgets
{
<<<<<<< Updated upstream
=======
    // small inline icons drawn in front of the top-tab labels (matcha style)
    enum TabIcon
    {
        TABICON_NONE = 0,
        TABICON_CROSSHAIR,
        TABICON_EYE,
        TABICON_SLIDERS,
        TABICON_PERSON,
        TABICON_GEAR
    };

    // public icon painter (used by the sidebar nav too)
    void tab_icon(ImDrawList* draw, TabIcon kind, ImVec2 c, ImU32 col);

>>>>>>> Stashed changes
    void sidebar_tabs(const std::vector<const char*>& items, int* selected, float width);
    void horizontal_tabs(const std::vector<const char*>& items, int* selected, float width, float height);
}
