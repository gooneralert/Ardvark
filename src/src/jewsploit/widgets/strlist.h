#pragma once

namespace ng
{
	// высота под max_vis строк (+ шапка/отступы)
	float strlist_h(int max_vis = 5);

	// список, видно max_vis, дальше скролл
	bool strlist(
		const char* id,
		const char* title,
		char items[][128],
		int count,
		int* sel,
		float w,
		int max_vis = 5
	);
}