#include "stubs.h"

void matcha_ui_init_stubs()
{
	memory = &g_dummy_memory;
}

namespace helper
{
	void draw_text_outlined(ImDrawList* draw, ImFont* font, float font_size, ImVec2 pos, ImU32 col, const char* text_begin, const char* text_end)
	{
		if (!draw || !text_begin) return;
		draw->AddText(font, font_size, ImVec2(pos.x + 1.f, pos.y + 1.f), IM_COL32(0, 0, 0, 180), text_begin, text_end);
		draw->AddText(font, font_size, pos, col, text_begin, text_end);
	}

	void corner_box(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float thickness)
	{
		if (!draw) return;
		draw->AddRect(min, max, col, 0.f, 0, thickness);
	}
}
