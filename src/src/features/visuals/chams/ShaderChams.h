#pragma once
#include "imgui.h"
#include <vector>

namespace Cheat {
namespace Visuals {
namespace ShaderChams {

enum Style : int {
    Plasma = 0,
    Ember,
    Toxic,
    Ice,
    Void,
    Neon,
    Gold,
    Matrix,
    Aurora,
    Blood,
    Ocean,
    Hologram,
    Sunset,
    Ghost,
    Chromatic,
    StyleCount
};

const char* const* StyleNames();
inline int StyleNameCount() { return StyleCount; }

// color_override rgba[4], если дали то стиль пофиг (трупы)
ImU32 OutlineColor(int style, bool aim_highlight = false,
                   const float* color_override = nullptr);

void DrawFill(ImDrawList* draw_list,
              const std::vector<std::vector<ImVec2>>& pieces,
              float time,
              int style,
              bool aim_highlight = false,
              bool lite = false,
              const float* color_override = nullptr);

void Triangulate(const std::vector<ImVec2>& poly,
                 std::vector<std::vector<ImVec2>>& out_tris);

}
}
}
