#pragma once

// player query — team / getters
// только из PlayerHandler.cpp

std::uint64_t Cheat::PlayerHandler::ResolveTeamFolder(std::uint64_t character_address)
{
    if (!g_Memory.IsValid(character_address))
        return 0;

    auto parent = Instance(character_address).GetParent();
    if (!parent || !g_Memory.IsValid(parent->address))
        return 0;

    if (Globals::Workspace && parent->address == Globals::Workspace->address)
        return 0;

    const std::string cls = parent->GetClassName();
    if (cls == "Workspace" || cls == "Players" || cls == "DataModel" ||
        cls == "Camera" || cls == "Terrain")
        return 0;

    if (cls != "Folder" && cls != "Model" && cls != "Configuration")
        return 0;

    return parent->address;
}

std::uint64_t Cheat::PlayerHandler::LocalTeamFolder()
{
    if (Games::PhantomForces::IsActivePlace()) {
        const std::uint64_t pf = Games::PhantomForces::LocalTeamFolder();
        if (pf)
            return pf;
    }
    return ResolveTeamFolder(LocalCharacterAddress());
}

bool Cheat::PlayerHandler::IsTeammate(const PlayerCache& cache, std::uint64_t local_team_folder)
{
    if (Games::PhantomForces::IsActivePlace())
        return Games::PhantomForces::IsTeammate(cache, local_team_folder);

    if (!local_team_folder || !cache.team_folder)
        return false;
    if (!cache.is_player)
        return false;
    return cache.team_folder == local_team_folder;
}

Cheat::PlayerCache Cheat::PlayerHandler::GetCachedPlayer(std::uint64_t address)
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto it = playerCache.find(address);
    if (it != playerCache.end())
        return it->second;
    return {};
}

std::unordered_map<std::uint64_t, Cheat::PlayerCache> Cheat::PlayerHandler::GetPlayerCache()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    return playerCache;
}

std::size_t Cheat::PlayerHandler::GetPlayerCount()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    return playerCache.size();
}
