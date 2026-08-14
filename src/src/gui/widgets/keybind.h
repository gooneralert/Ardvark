#pragma once

namespace widgets
{
    const char* key_name(int key);
    bool keybind(const char* id, int* key);
    bool checkbox_keybind(const char* label, bool* value, int* key);
}
