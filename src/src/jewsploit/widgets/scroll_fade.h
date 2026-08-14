#pragma once

#include <imgui.h>

namespace ng
{
	void scroll_fade(ImDrawList* dl, ImVec2 a, ImVec2 b, float top_k, float bot_k, ImU32 bg, float fade_h = 20.f);
	void scroll_fade_child(ImU32 bg, float fade_h = 20.f);
}
