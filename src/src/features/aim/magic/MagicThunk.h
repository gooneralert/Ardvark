#pragma once
#include <cstdint>
#include <vector>

namespace Cheat {
namespace Features {
namespace MagicBullet {
namespace mb {

	void append_u64(std::vector<std::uint8_t>& c, std::uint64_t v);
	void patch_rel32(std::vector<std::uint8_t>& c, std::size_t o, std::size_t t);
	std::vector<std::uint8_t> make_jmp_thunk(std::uintptr_t orig);
	std::vector<std::uint8_t> make_hook_thunk(std::uintptr_t state, std::uintptr_t orig);

} // namespace mb
} // namespace MagicBullet
} // namespace Features
} // namespace Cheat
