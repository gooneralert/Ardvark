#pragma once

namespace ng
{
	// shown=false -> прячется с анимацией; fill/grab тоже плавно
	bool slider(const char* id, float* v, float mn, float mx, bool shown);
}