#pragma once
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include <Windows.h>
#include <cstdint>

namespace Cheat {
namespace Features {
namespace InfiniteJump {

// space жмёт Jump=true, только edge
inline void Apply(std::uint64_t humanoid, bool enabled)
{
	if (!enabled || !g_Memory.IsValid(humanoid))
		return;

	static bool was_down = false;
	bool down = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	if (down && !was_down)
		g_Memory.Write<bool>(humanoid + ::Humanoid::Jump, true);
	was_down = down;
}

} // namespace InfiniteJump
} // namespace Features
} // namespace Cheat
