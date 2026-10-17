#pragma once

#include <imgui.h>

// renamed from `gui` to `mmenu`: Ardvark already owns `namespace gui`
// (src/gui/gui.h) and both declare a render()/size(), which would collide.
namespace mmenu
{
    void init();
    void render();
    ImVec2 size();
}
