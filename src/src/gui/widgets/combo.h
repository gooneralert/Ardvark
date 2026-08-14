#pragma once

#include <vector>

namespace widgets
{
    bool combo(const char* label, int* current, const std::vector<const char*>& items);
    bool multicombo(const char* id, bool* selected, const std::vector<const char*>& items);
    bool checkbox_multicombo(const char* label, bool* value, bool* selected, const std::vector<const char*>& items);
}
