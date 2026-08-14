#include "pch.h"
#include "EspSkeleton.h"

namespace Cheat {
namespace Visuals {
namespace EspSkeleton {

void DrawSkeletonLine(ImDrawList* draw_list, ImVec2 a, ImVec2 b,
	ImU32 color, float thick, bool outline)
{
	if (thick < 1.0f) thick = 1.0f;
	if (outline)
		draw_list->AddLine(a, b, IM_COL32(0, 0, 0, 255), thick + 2.0f);
	draw_list->AddLine(a, b, color, thick);
}

} // namespace EspSkeleton
} // namespace Visuals
} // namespace Cheat
