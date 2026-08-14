#pragma once

namespace ng
{
	// title/desc сверху, бар: кружки + hex. show_brush — кисть с colorpicker
	bool color_presets(const char* id, const char* title, const char* desc, float col[4], bool shown = true, bool show_brush = true);
}