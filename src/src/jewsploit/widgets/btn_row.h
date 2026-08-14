#pragma once

namespace ng
{
	// ряд равных кнопок как load/delete/save. return индекс клика или -1
	int btn_row(const char* id, const char* const labels[], int n, float h = 30.f);
}