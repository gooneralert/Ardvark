#pragma once
#include <cstddef>
#include <cstdint>
#include <Windows.h>

namespace Cheat {
namespace Features {
namespace MagicBullet {
namespace mb {

	bool addr_ok(std::uintptr_t a);
	bool w_mem(std::uintptr_t a, const void* d, std::size_t s);
	std::size_t page_sz();
	DWORD query_protect(std::uintptr_t a);
	bool is_executable_protect(DWORD p);
	bool protect_remote(std::uintptr_t address, std::size_t size, DWORD protection,
		DWORD* old_protect = nullptr);
	bool write_protected(std::uintptr_t address, const void* data, std::size_t size);
	bool mark_cfg(std::uintptr_t t);

} // namespace mb
} // namespace MagicBullet
} // namespace Features
} // namespace Cheat
