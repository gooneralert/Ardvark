#pragma once
#include <cstdint>
#include <unordered_map>
#include "core/player/PlayerHandler.h"

namespace Cheat {
namespace Games {
namespace PhantomForces {

// Phantom Forces (main)
inline constexpr std::uint64_t kPlaceId = 292439477ull;

// 0 = Phantoms (blue), 1 = Ghosts (orange), -1 = unknown
enum TeamSide : int {
    SideUnknown = -1,
    SidePhantom = 0,
    SideGhost = 1,
};

// placeId == 292439477 (авто, без чекбокса)
bool IsActivePlace();

// мержит живых из Workspace.Players в кэш (имена с лидерборда / Players)
void MergePlayers(std::unordered_map<std::uint64_t, PlayerCache>& target);

std::uint64_t LocalTeamFolder();
std::uint64_t LocalCharacter();
bool IsTeammate(const PlayerCache& cache, std::uint64_t local_team_folder);

TeamSide LocalSide();

} // namespace PhantomForces
} // namespace Games
} // namespace Cheat
