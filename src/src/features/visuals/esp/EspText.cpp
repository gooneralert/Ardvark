#include "pch.h"
#include "EspText.h"
#include "gui/resources/fonts/fonts.h"

#include <cmath>

namespace Cheat {
namespace Visuals {
namespace EspText {

void DrawTextWithOutline(ImDrawList* draw_list, ImFont* font, float font_size,
	ImVec2 pos, ImU32 color, const char* text)
{
	ImU32 shadow = IM_COL32(0, 0, 0, 255);
	font_size = fonts::snap_px(font_size);
	float x = std::floor(pos.x);
	float y = std::floor(pos.y);

	for (int i = -1; i <= 1; i++)
	{
		for (int j = -1; j <= 1; j++)
		{
			if (i == 0 && j == 0)
				continue;
			draw_list->AddText(font, font_size, ImVec2(x + i, y + j), shadow, text);
		}
	}

	draw_list->AddText(font, font_size, ImVec2(x, y), color, text);
}

} // namespace EspText
} // namespace Visuals
} // namespace Cheat
