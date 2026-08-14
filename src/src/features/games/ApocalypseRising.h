#pragma once
#include <cstdint>
#include <unordered_map>
#include "core/player/PlayerHandler.h"

namespace Cheat {
namespace Games {
namespace ApocalypseRising {

// Apocalypse Rising (main)
inline constexpr std::uint64_t kPlaceId = 863266079ull;

bool IsActivePlace();

// Workspace.Characters → players, Workspace.Zombies → bots
void MergeEntities(std::unordered_map<std::uint64_t, PlayerCache>& target);

} // namespace ApocalypseRising
} // namespace Games
} // namespace Cheat
