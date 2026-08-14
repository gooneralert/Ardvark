#pragma once

#include "../imgui.h"

#define IMADD_ANIMATIONS_SPEED	0.07f

#include <vector>

namespace ImAdd
{
	void    SeparatorText(const char* label, float thickness = 1.0f);
	void    VSeparator(float margin = 0.0f, float thickness = 1.0f);
}
