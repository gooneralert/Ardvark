#pragma once

#include "imgui.h"

namespace Cheat {
namespace Visuals {
namespace EspBox {

// рёбра куба (углы 0..7) — и 3d бокс и chams wire
inline constexpr int k_box_edges[12][2] = {
	{0,1},{0,2},{0,4},{1,3},{1,5},{2,3},
	{2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
};

void SnapEspBox(float min_x, float min_y, float max_x, float max_y,
	float& x1, float& y1, float& x2, float& y2);

void DrawBox(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right,
	ImU32 color, float thick, bool outline);

void DrawCornerBox(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right,
	ImU32 color, float thick, bool outline);

// 8 экранных углов уже посчитаны
void DrawBox3DEdges(ImDrawList* draw_list, const ImVec2 pts[8],
	ImU32 color, float thick, bool outline);

} // namespace EspBox
} // namespace Visuals
} // namespace Cheat
