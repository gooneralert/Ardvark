#include "color_style.h"

namespace
{
	int g_cp_style = 0;
}

int ng::cp_style()
{
	return g_cp_style;
}

void ng::set_cp_style(int v)
{
	if (v < 0) v = 0;
	if (v > 1) v = 1;
	g_cp_style = v;
}