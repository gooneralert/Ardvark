#pragma once
#include <cstddef>
#include <cstdint>

namespace Cheat {
namespace Features {
namespace MagicBullet {
namespace mb {

	bool region_is_padding(std::uintptr_t a, std::size_t n);
	bool read_val(std::uintptr_t a, void* d, std::size_t s);
	std::uintptr_t find_cave_in_module(std::uintptr_t module_base, std::size_t need,
		std::uintptr_t min_offset, std::uintptr_t ignore);
	std::uintptr_t find_exec_cave(std::size_t need, std::uintptr_t,
		std::uintptr_t ignore = 0);
	std::uintptr_t alloc_exec_page();

} // namespace mb
} // namespace MagicBullet
} // namespace Features
} // namespace Cheat
