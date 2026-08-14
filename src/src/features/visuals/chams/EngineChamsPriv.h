#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace EngineChams {
namespace detail {

struct LayerBackup {
	std::uint8_t fillmode = 0;
	std::uint32_t matflags = 0;
	std::uint32_t param = 0;
	std::uint32_t flags2 = 0;
	std::uint32_t color = 0;
};

extern std::mutex g_mtx;
extern std::unordered_map<uintptr_t, std::uint32_t> g_saved;
extern std::unordered_map<uintptr_t, LayerBackup> g_layers;
extern std::unordered_map<uintptr_t, std::vector<uintptr_t>> g_ent_layers;
extern std::unordered_map<uintptr_t, int> g_miss; // дохлый ent — не сразу дропаем
extern uintptr_t g_vt;
extern int g_applied_style;

} // namespace detail
} // namespace EngineChams
} // namespace Visuals
} // namespace Cheat
