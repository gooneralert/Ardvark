#pragma once

#include "checkbox.h"
#include "slider.h"
#include "combo.h"
#include "colorpicker.h"
#include "button.h"
#include "tabs.h"
#include "keybind.h"
#include "input.h"
#include "text.h"

// watermark badge (overlay, independent of the menu)
namespace widgets
{
    void watermark(float alpha = 1.f);
    bool  watermark_hit_test(float x, float y);
}
