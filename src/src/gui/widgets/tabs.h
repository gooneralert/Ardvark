#pragma once

#include <vector>

namespace widgets
{
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

    void sidebar_tabs(const std::vector<const char*>& items, int* selected, float width);
    void horizontal_tabs(const std::vector<const char*>& items, int* selected, float width, float height);
    void top_tabs(const std::vector<const char*>& items, int* selected, float width, float height, const TabIcon* icons = nullptr);
}
