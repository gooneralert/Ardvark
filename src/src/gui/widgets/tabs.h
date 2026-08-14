#pragma once

#include <vector>

namespace widgets
{
    void sidebar_tabs(const std::vector<const char*>& items, int* selected, float width);
    void horizontal_tabs(const std::vector<const char*>& items, int* selected, float width, float height);
}
