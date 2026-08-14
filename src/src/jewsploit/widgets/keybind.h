#pragma once

namespace ng
{
	// mode: 0 hold, 1 toggle (как в чите)
	// рисуется справа в ряд с прошлым контролом (чекбоксом)
	bool keybind(const char* id, int* vk, int* mode, bool shown);
}