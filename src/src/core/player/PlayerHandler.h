#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include "core/roblox/classes/Classes.h"

namespace Cheat {

    struct PlayerCache
    {
        std::string name;
        std::string displayName;
        std::string toolName;
        std::uint64_t address;
        std::uint64_t character = 0;
        std::uint64_t team_folder = 0;
        std::uint64_t player_address = 0; // реальный Player instance, для GetUserId/GetAccountAge/GetTeam
        std::int64_t user_id = 0;
        bool is_player = false;
        bool isR6 = false;
        bool is_corpse = false; // труп после смерти, для body-corpse esp

        std::shared_ptr<Instance> head;
        std::shared_ptr<Instance> humanoidRootPart;
        std::shared_ptr<Instance> upperTorso;
        std::shared_ptr<Instance> lowerTorso;
        std::shared_ptr<Instance> leftUpperArm;
        std::shared_ptr<Instance> leftLowerArm;
        std::shared_ptr<Instance> leftHand;
        std::shared_ptr<Instance> rightUpperArm;
        std::shared_ptr<Instance> rightLowerArm;
        std::shared_ptr<Instance> rightHand;
        std::shared_ptr<Instance> leftUpperLeg;
        std::shared_ptr<Instance> leftLowerLeg;
        std::shared_ptr<Instance> leftFoot;
        std::shared_ptr<Instance> rightUpperLeg;
        std::shared_ptr<Instance> rightLowerLeg;
        std::shared_ptr<Instance> rightFoot;
        std::shared_ptr<Instance> humanoid;

        PlayerCache() : address(0) {}
        explicit PlayerCache(std::uint64_t addr) : address(addr) {}
    };

    class PlayerHandler
    {
    public:
        PlayerHandler() = default;
        ~PlayerHandler() = default;

        static void UpdateCache(const Instance& player,
                                std::unordered_map<std::uint64_t, PlayerCache>& target);

        static void CacheAllPlayers();
        static PlayerCache GetCachedPlayer(std::uint64_t address);
        static bool IsPlayerCached(std::uint64_t address);
        static void ClearCache();
        static void RemoveFromCache(std::uint64_t address);

        static void StartCacheThread();
        static void StopCacheThread();
        static std::unordered_map<std::uint64_t, PlayerCache> GetPlayerCache();

        static std::uint64_t ResolveTeamFolder(std::uint64_t character_address);
        static std::uint64_t LocalTeamFolder();
        static bool IsTeammate(const PlayerCache& cache, std::uint64_t local_team_folder);

        template<typename F>
        static void ForEachPlayer(F&& fn)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            for (auto const& [address, cache] : playerCache)
                fn(cache);
        }

        static std::size_t GetPlayerCount();

    private:
        static std::unordered_map<std::uint64_t, PlayerCache> playerCache;
        static std::thread cacheThread;
        static std::atomic<bool> shouldRun;
        static std::mutex cacheMutex;
        static void CacheThreadLoop();
    };

}
