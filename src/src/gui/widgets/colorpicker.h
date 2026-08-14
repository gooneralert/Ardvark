#pragma once

namespace widgets
{
    bool color_picker(const char* label, float color[4]);
    bool color_edit(const char* label, float color[4]);
    bool checkbox_color(const char* label, bool* value, float color[4]);
    bool checkbox_color2(const char* label, bool* value, float color1[4], float color2[4]);
}
