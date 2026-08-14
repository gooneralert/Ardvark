#pragma once

#include "imgui.h"

namespace Cheat {
namespace Visuals {
namespace EspTracer {

// 0 bottom / 1 center / 2 mouse / 3 top
ImVec2 TracerOrigin(float overlay_w, float overlay_h, int mode);

void DrawTracer(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 color);

} // namespace EspTracer
} // namespace Visuals
} // namespace Cheat
