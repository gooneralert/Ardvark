#pragma once

#include <cstdint>

namespace Cheat {
namespace Visuals {
namespace EngineChams {
namespace detail {

bool EntAlive(uintptr_t ent);
bool EntKnown(uintptr_t ent);
bool LayerAlive(uintptr_t layer);

void DropLayersLocked(uintptr_t ent);
void RestoreLayersLocked(uintptr_t ent);
void RestoreAll();
void RestoreOne(uintptr_t ent);
void DropDeadLocked(uintptr_t ent);

void ApplyLayers(uintptr_t ent, std::uint8_t fill, std::uint32_t param, std::uint32_t flags2, std::uint32_t color);
void ApplyEntity(uintptr_t ent);

} // namespace detail
} // namespace EngineChams
} // namespace Visuals
} // namespace Cheat
