#pragma once

#include "imgui.h"

namespace Cheat {
namespace Visuals {
namespace EspText {

void DrawTextWithOutline(ImDrawList* draw_list, ImFont* font, float font_size,
	ImVec2 pos, ImU32 color, const char* text);

} // namespace EspText
} // namespace Visuals
} // namespace Cheat
