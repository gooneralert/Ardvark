#pragma once

namespace ng
{
	// микро-child с ARGB. лкм = copy, даблклик = edit/paste
	// w <= 0 -> до правого края окна с pad 12
	bool argb_field(const char* id, float col[4], float w = -1.f);
}