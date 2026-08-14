#pragma once

#include "imgui.h"

namespace Cheat {
namespace Visuals {
namespace EspSkeleton {

void DrawSkeletonLine(ImDrawList* draw_list, ImVec2 a, ImVec2 b,
	ImU32 color, float thick, bool outline);

} // namespace EspSkeleton
} // namespace Visuals
} // namespace Cheat
