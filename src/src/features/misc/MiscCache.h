#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace MiscCache {

struct Cache {
	std::uint64_t datamodel = 0;
	std::uint64_t lighting = 0;
	std::uint64_t root = 0;
	std::uint64_t humanoid = 0;
	std::uint64_t character = 0;
	std::uint64_t last_refresh = 0;
};

Cache& Get();
std::uint64_t find_svc(const char* cls);
std::uint64_t local_chara();
void refresh_cache();

}
}
}
