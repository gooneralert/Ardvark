#pragma once

namespace ng
{
	// single select dropdown. shown=false -> схлопывается как слайдер
	// close_on_pick=false — список остаётся открыт, можно тыкать дальше
	bool select(const char* id, int* cur, const char* const items[], int count, bool shown = true, bool close_on_pick = true);
}
