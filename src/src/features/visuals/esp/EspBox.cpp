#include "pch.h"
#include "EspBox.h"

#include <cmath>

namespace Cheat {
namespace Visuals {
namespace EspBox {

void SnapEspBox(float min_x, float min_y, float max_x, float max_y,
	float& x1, float& y1, float& x2, float& y2)
{
	x1 = std::floor(min_x);
	y1 = std::floor(min_y);
	x2 = std::ceil(max_x);
	y2 = std::ceil(max_y);
	if (x2 <= x1) x2 = x1 + 1.0f;
	if (y2 <= y1) y2 = y1 + 1.0f;
}

// обычный бокс, опционально с чёрной обводкой
void DrawBox(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right,
	ImU32 color, float thick, bool outline)
{
	float x1, y1, x2, y2;
	SnapEspBox(top_left.x, top_left.y, bottom_right.x, bottom_right.y, x1, y1, x2, y2);
	if (thick < 0.5f) thick = 0.5f;

	if (outline) {
		if (thick <= 1.01f) {
			draw_list->AddRect(ImVec2(x1 - 1, y1 - 1), ImVec2(x2 + 1, y2 + 1),
				IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);
			draw_list->AddRect(ImVec2(x1 + 1, y1 + 1), ImVec2(x2 - 1, y2 - 1),
				IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);
		} else {
			draw_list->AddRect(ImVec2(x1, y1), ImVec2(x2, y2),
				IM_COL32(0, 0, 0, 255), 0.0f, 0, thick + 2.0f);
		}
	}
	draw_list->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), color, 0.0f, 0, thick);
}

void DrawCornerBox(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right,
	ImU32 color, float thick, bool outline)
{
	float x1, y1, x2, y2;
	SnapEspBox(top_left.x, top_left.y, bottom_right.x, bottom_right.y, x1, y1, x2, y2);
	if (thick < 0.5f) thick = 0.5f;
	float lw = std::floor((x2 - x1) * 0.25f);
	float lh = std::floor((y2 - y1) * 0.25f);
	if (lw < 2.f) lw = 2.f;
	if (lh < 2.f) lh = 2.f;

	struct Seg { ImVec2 a, b; };
	Seg segs[8] = {
		{ {x1, y1}, {x1 + lw, y1} }, { {x1, y1}, {x1, y1 + lh} },
		{ {x2 - lw, y1}, {x2, y1} }, { {x2, y1}, {x2, y1 + lh} },
		{ {x1, y2 - lh}, {x1, y2} }, { {x1, y2}, {x1 + lw, y2} },
		{ {x2, y2 - lh}, {x2, y2} }, { {x2 - lw, y2}, {x2, y2} },
	};
	ImU32 black = IM_COL32(0, 0, 0, 255);
	if (outline) {
		const float ot = thick + 2.0f;
		for (const auto& s : segs) draw_list->AddLine(s.a, s.b, black, ot);
	}
	for (const auto& s : segs) draw_list->AddLine(s.a, s.b, color, thick);
}

void DrawBox3DEdges(ImDrawList* draw_list, const ImVec2 pts[8],
	ImU32 color, float thick, bool outline)
{
	if (thick < 0.5f) thick = 0.5f;
	if (outline) {
		const ImU32 black = IM_COL32(0, 0, 0, 255);
		const float ot = thick + 2.0f;
		for (const auto& e : k_box_edges)
			draw_list->AddLine(pts[e[0]], pts[e[1]], black, ot);
	}
	for (const auto& e : k_box_edges)
		draw_list->AddLine(pts[e[0]], pts[e[1]], color, thick);
}

} // namespace EspBox
} // namespace Visuals
} // namespace Cheat
