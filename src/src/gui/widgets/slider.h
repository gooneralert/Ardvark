#pragma once

namespace widgets
{
    bool slider_int(const char* label, int* value, int min, int max);
    bool slider_float(const char* label, float* value, float min, float max, const char* fmt = "%.1f");
}
