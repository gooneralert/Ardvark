#pragma once

#include <imgui.h>

namespace ng
{
	bool child_begin(const char* id, const char* title, float w, float h, float round = 10.f, bool scroll = false);
	void child_end();

	float child_head_h();
}