#pragma once

#include <cstdint>

namespace Cheat {
namespace Visuals {
namespace EngineChams {
namespace detail {

std::uint32_t ColorParam(int idx);
std::uint32_t StyleQueue(int style);
bool ApplyStyleLayers(uintptr_t ent, int style, int color_idx);

} // namespace detail
} // namespace EngineChams
} // namespace Visuals
} // namespace Cheat
