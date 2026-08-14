#pragma once

namespace ng
{
	// float окно в стиле shell: header + close, drag, resize
	// open == nullptr -> без креста
	bool float_panel_begin(
		const char* id,
		const char* title,
		bool* open,
		float def_w,
		float def_h,
		float min_w,
		float min_h
	);

	void float_panel_end();

	float float_panel_head_h();
}
