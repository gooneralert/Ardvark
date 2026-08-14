#pragma once
#include "core/roblox/offsets/Offsets.h"
#include <chrono>
#include <cstdint>
#include <cstddef>

namespace Cheat {
namespace Features {
namespace MagicBullet {
namespace mb {
	// magic stub, wallbang всегда, оффсеты state не трогать
#pragma pack(push, 4)
	struct RaycastState {
		std::uint32_t active = 0;
		std::uint32_t reserved = 0;
		float target_x = 0.f;
		float target_y = 0.f;
		float target_z = 0.f;
		float scale = 1.15f;
		std::uint64_t calls = 0;
	};
#pragma pack(pop)

	static_assert(offsetof(RaycastState, active) == 0x00, "active");
	static_assert(offsetof(RaycastState, target_x) == 0x08, "target");
	static_assert(offsetof(RaycastState, calls) == 0x18, "calls");

	struct Hook {
		std::uintptr_t thunk = 0;
		std::uintptr_t state = 0;
		std::uintptr_t originalFunction = 0;
		std::uintptr_t module_base = 0;
		bool thunk_owned = false;
		bool installed = false;
		bool active = false;
	};

	extern Hook g_hook;
	extern bool g_wallbang;
	extern std::chrono::steady_clock::time_point g_lastFail;

} // namespace mb
} // namespace MagicBullet
} // namespace Features
} // namespace Cheat
