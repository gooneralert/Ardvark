#pragma once

// player cache — scan / thread / CacheAllPlayers
// только из PlayerHandler.cpp

namespace {

bool IsIgnoredService(const std::string& cls)
{
    return cls == "ReplicatedStorage" || cls == "ServerStorage" ||
           cls == "ReplicatedFirst" || cls == "StarterPlayer" ||
           cls == "StarterPack" || cls == "StarterGui" ||
           cls == "CoreGui" || cls == "Chat" ||
           cls == "MaterialService" || cls == "Lighting" ||
           cls == "SoundService" || cls == "TextChatService";
}

bool IsLeafPartClass(const std::string& cls)
{
    return cls == "Part" || cls == "MeshPart" || cls == "UnionOperation" ||
           cls == "NegateOperation" || cls == "IntersectOperation" ||
           cls == "TrussPart" || cls == "WedgePart" || cls == "CornerWedgePart" ||
           cls == "Seat" || cls == "VehicleSeat" || cls == "SpawnLocation" ||
           cls == "Terrain";
}

bool CharacterOwnedByPlayer(std::uint64_t character_address)
{
    if (!g_Memory.IsValid(character_address))
        return false;
    if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
        return false;

    for (const auto& player : Cheat::Globals::Players->GetChildren()) {
        if (!g_Memory.IsValid(player.address))
            continue;
        const std::uint64_t model = g_Memory.Read<std::uint64_t>(
            player.address + ::Player::ModelInstance);
        if (model == character_address)
            return true;
    }
    return false;
}

void AddCharacter(const Cheat::Instance& model, Cheat::PlayerCache&& cache,
                  std::uint64_t localChar,
                  std::unordered_map<std::uint64_t, Cheat::PlayerCache>& target)
{
    if (model.address == localChar)
        return;

    cache.name = model.GetName();
    if (cache.name.empty())
        cache.name = "unknown";

    if (cache.humanoid)
    {
        std::string dn = Cheat::Humanoid(cache.humanoid->address).GetDisplayName();
        if (dn.empty() || dn == "Unknown")
            cache.displayName = cache.name;

        else
            cache.displayName = dn;
    }

    else
    {
        cache.displayName = cache.name;
    }

    cache.character = model.address;
    cache.team_folder = Cheat::PlayerHandler::ResolveTeamFolder(model.address);
    cache.is_player = CharacterOwnedByPlayer(model.address);

    target[model.address] = std::move(cache);
}

void ScanNode(const Cheat::Instance& node, int depth, int& budget,
              std::uint64_t localChar,
              std::unordered_map<std::uint64_t, Cheat::PlayerCache>& target)
{
    // глубже 12 уже мусор, budget кончится раньше
    if (depth > 12 || budget <= 0)
        return;

    auto kids = node.GetChildren();

    {
        Cheat::PlayerCache probe(node.address);
        if (PopulatePartsFromChildren(kids, probe)) {
            AddCharacter(node, std::move(probe), localChar, target);
            return;
        }
    }

    for (const auto& child : kids) {
        if (--budget <= 0)
            return;
        if (IsLeafPartClass(child.GetClassName()))
            continue;
        ScanNode(child, depth + 1, budget, localChar, target);
    }
}

void ScanTree(const Cheat::Instance& root, bool skipStorageServices,
              std::uint64_t localChar,
              std::unordered_map<std::uint64_t, Cheat::PlayerCache>& target)
{
    int budget = 40000;
    if (!skipStorageServices)
    {
        ScanNode(root, 0, budget, localChar, target);
        return;
    }

    for (const auto& service : root.GetChildren())
    {
        if (budget <= 0)
            break;
        if (IsIgnoredService(service.GetClassName()))
            continue;
        ScanNode(service, 1, budget, localChar, target);
    }
}

void CacheFromWorkspace(std::unordered_map<std::uint64_t, Cheat::PlayerCache>& target)
{
    const std::uint64_t localChar = LocalCharacterAddress();

    if (Cheat::Globals::Workspace && g_Memory.IsValid(Cheat::Globals::Workspace->address))
        ScanTree(*Cheat::Globals::Workspace, false, localChar, target);

    // workspace пустой, лезем в datamodel
    if (target.empty() && g_Memory.IsValid(Cheat::Globals::InstanceDataModel.address))
        ScanTree(Cheat::Globals::InstanceDataModel, true, localChar, target);
}

bool ChildNamedExists(const Cheat::Instance& parent, const char* name)
{
    auto c = parent.FindFirstChild(name);
    return c && g_Memory.IsValid(c->address);
}

int CountDirectHumanoidModels(const Cheat::Instance& root)
{
    int n = 0;
    for (const auto& child : root.GetChildren()) {
        if (child.GetClassName() != "Model")
            continue;
        if (ChildNamedExists(child, "Humanoid") || ChildNamedExists(child, "HumanoidRootPart"))
            ++n;
    }
    return n;
}

bool IsCharactersRootCandidate(const Cheat::Instance& node)
{
    const std::string cls = node.GetClassName();
    if (cls != "Model" && cls != "Folder")
        return false;

    const std::string name = node.GetName();
    if (name == "Buildings" || name == "Camera" || name == "Terrain" ||
        name == "Ignored" || name == "Debris" || name == "Map" ||
        name == "Effects" || name == "Sounds" || name == "Loots" ||
        name == "CurrentCamera")
        return false;

    const int hum = CountDirectHumanoidModels(node);
    if (hum >= 2)
        return true;
    // havoc: папка с нпц + стримнутые чары
    if (hum >= 1 && ChildNamedExists(node, "NPCs"))
        return true;
    return false;
}

std::uint64_t FindCharactersRootAddress()
{
    static std::uint64_t cached = 0;
    static ULONGLONG next_scan = 0;
    const ULONGLONG now = GetTickCount64();

    if (cached && g_Memory.IsValid(cached)) {
        Cheat::Instance root(cached);
        if (CountDirectHumanoidModels(root) >= 1 || ChildNamedExists(root, "NPCs"))
            return cached;
        cached = 0;
    }

    if (now < next_scan)
        return cached;
    next_scan = now + 1500;

    if (!Cheat::Globals::Workspace || !g_Memory.IsValid(Cheat::Globals::Workspace->address))
        return 0;

    for (const auto& child : Cheat::Globals::Workspace->GetChildren()) {
        if (!IsCharactersRootCandidate(child))
            continue;
        cached = child.address;
        return cached;
    }
    return 0;
}

bool LooksLikeBotModel(const Cheat::Instance& model)
{
    const bool active = ChildNamedExists(model, "ActiveScripts");
    const bool weld_link = ChildNamedExists(model, "WeldObjectsLink");
    const bool player_bits =
        ChildNamedExists(model, "MovementAnticheat") ||
        ChildNamedExists(model, "FakeCam") ||
        ChildNamedExists(model, "Animate");

    // у реальных игроков Animate/anticheat, у ботов ActiveScripts
    if (player_bits && !active)
        return false;
    if (active || weld_link)
        return true;
    return false;
}

// havoc боты живут под Workspace/<random Model>, не в Players
// мержим всегда, esp.bots гейтит только отрисовку
void MergeStreamedBots(std::unordered_map<std::uint64_t, Cheat::PlayerCache>& target)
{
    if (!Cheat::Visuals::HavocWorldEsp::IsActivePlace())
        return;

    std::uint64_t root_addr = FindCharactersRootAddress();
    if (!root_addr)
        return;

    std::unordered_set<std::uint64_t> known;
    known.reserve(target.size() * 2 + 8);
    for (const auto& [_, cache] : target)
    {
        if (cache.character)
            known.insert(cache.character);
    }

    std::uint64_t localChar = LocalCharacterAddress();
    Cheat::Instance root(root_addr);

    for (const auto& child : root.GetChildren())
    {
        if (child.GetClassName() != "Model")
            continue;
        if (child.address == localChar)
            continue;
        if (known.count(child.address))
            continue;
        if (CharacterOwnedByPlayer(child.address))
            continue;
        if (!LooksLikeBotModel(child))
            continue;

        Cheat::PlayerCache cache(child.address);
        if (!PopulateParts(child.address, cache))
            continue;
        if (!cache.humanoidRootPart && !cache.head)
            continue;

        AddCharacter(child, std::move(cache), localChar, target);

        // боты не из Players.ModelInstance
        auto it = target.find(child.address);
        if (it != target.end())
            it->second.is_player = false;
    }
}

std::uint64_t FindServiceByClass(const Cheat::Instance& dm, const char* cls)
{
    for (const auto& child : dm.GetChildren())
        if (child.GetClassName() == cls)
            return child.address;
    return 0;
}

bool LooksLikeDataModel(std::uint64_t dm)
{
    if (!dm)
        return false;
    Cheat::Instance inst(dm);
    if (inst.GetClassName() == "DataModel")
        return true;
    const std::string name = inst.GetName();
    return name == "Game" || name == "Ugc" || name == "LuaApp";
}

// ищем датамодель через FakeDM / VisualEngine
std::uint64_t ResolveDataModel()
{
    uintptr_t base = g_Memory.GetModuleBase();
    if (!base)
        return 0;

    std::uint64_t front = g_Memory.Read<std::uint64_t>(base + ::FakeDataModel::Pointer);
    if (front)
    {
        Cheat::Globals::FrontDataModel = front;
        std::uint64_t dm = g_Memory.Read<std::uint64_t>(front + ::FakeDataModel::RealDataModel);
        if (LooksLikeDataModel(dm))
            return dm;
        if (LooksLikeDataModel(front))
            return front;
    }

    std::uint64_t ve = g_Memory.Read<std::uint64_t>(base + ::VisualEngine::Pointer);
    if (ve)
    {
        std::uint64_t fake = g_Memory.Read<std::uint64_t>(ve + ::VisualEngine::FakeDataModel);
        if (fake)
        {
            std::uint64_t dm = g_Memory.Read<std::uint64_t>(fake + ::FakeDataModel::RealDataModel);
            if (LooksLikeDataModel(dm))
                return dm;

            dm = g_Memory.Read<std::uint64_t>(fake + ::RenderJob::RealDataModel);
            if (LooksLikeDataModel(dm))
                return dm;
            if (LooksLikeDataModel(fake))
                return fake;
        }
    }

    // раз в 5 сек орём в консоль что ждём
    static ULONGLONG s_last_diag = 0;
    ULONGLONG now = GetTickCount64();
    if (now - s_last_diag > 5000)
    {
        s_last_diag = now;
        Cheat::Console::Clear();
        Cheat::Console::Log(Cheat::Console::Color::Yellow, "waiting for datamodel");
        Cheat::Console::Ptr(Cheat::Console::Color::Yellow, "Front DM", front);
        Cheat::Console::Ptr(Cheat::Console::Color::Magenta, "VisualEngine", ve);
    }
    return 0;
}

void RefreshGlobals()
{
    std::uint64_t dm = ResolveDataModel();
    if (!dm)
        return;

    bool changed = false;
    if (dm != Cheat::Globals::InstanceDataModel.address)
    {
        Cheat::Globals::InstanceDataModel.address = dm;
        Cheat::Globals::Workspace = nullptr;
        Cheat::Globals::Players = nullptr;
        changed = true;
    }

    if (!Cheat::Globals::Workspace)
    {
        std::uint64_t ws = FindServiceByClass(Cheat::Globals::InstanceDataModel, "Workspace");
        if (ws)
        {
            Cheat::Globals::Workspace = std::make_shared<Cheat::Workspace>(ws);
            changed = true;
        }
    }
    if (!Cheat::Globals::Players)
    {
        std::uint64_t pl = FindServiceByClass(Cheat::Globals::InstanceDataModel, "Players");
        if (pl)
        {
            Cheat::Globals::Players = std::make_shared<Cheat::Players>(pl);
            changed = true;
        }
    }

    if (changed && Cheat::Globals::Workspace && Cheat::Globals::Players)
    {
        Cheat::Console::Clear();
        Cheat::Console::DumpWorld();
    }
}

}

// полный проход: Players, если пусто то workspace сканом
void Cheat::PlayerHandler::CacheAllPlayers()
{
    if (!Globals::InstanceDataModel.address)
        return;

    std::unordered_map<std::uint64_t, PlayerCache> previous;
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        previous = playerCache;
    }

    std::unordered_map<std::uint64_t, PlayerCache> fresh;

    if (Games::PhantomForces::IsActivePlace())
    {
        // PF: чары в Workspace.Players, имена зашифрованы
        Games::PhantomForces::MergePlayers(fresh);
    }
    else
    {
        if (Globals::Players && g_Memory.IsValid(Globals::Players->address))
        {
            auto players = Globals::Players->GetChildren();
            fresh.reserve(players.size());
            for (const auto& player : players)
                UpdateCache(player, fresh);
        }

        bool any_parts = false;
        for (const auto& [addr, cache] : fresh)
        {
            if (cache.humanoidRootPart)
            {
                any_parts = true;
                break;
            }
        }

        if (!any_parts)
        {
            fresh.clear();
            CacheFromWorkspace(fresh);
        }
    }

    // боты havoc, мержим после Players
    MergeStreamedBots(fresh);

    // после смерти парты не кидаем, трупные чамсы/имя
    if (g_Settings.esp.body_corpse)
    {
        std::unordered_map<std::uint64_t, bool> still_players;
        if (Globals::Players && g_Memory.IsValid(Globals::Players->address))
        {
            for (const auto& player : Globals::Players->GetChildren())
                still_players[player.address] = true;
        }

        for (auto& [addr, cur] : fresh)
        {
            const auto prev_it = previous.find(addr);
            const bool dead = IsPlayerDead(cur);

            if (dead)
            {
                cur.is_corpse = true;
                // каждый тик перечитываем, иначе старые указатели
                if (!RefreshCorpseFromCharacter(cur, cur.character))
                {
                    if (prev_it != previous.end())
                    {
                        if (!AnyBodyPartValid(cur) && AnyBodyPartValid(prev_it->second))
                            CopyBodyParts(cur, prev_it->second);
                        if (!g_Memory.IsValid(cur.character) && prev_it->second.character)
                            cur.character = prev_it->second.character;
                        RefreshCorpseFromCharacter(cur, cur.character);
                        if (!AnyBodyPartValid(cur) && AnyBodyPartValid(prev_it->second))
                            CopyBodyParts(cur, prev_it->second);
                    }
                }
            }

            else
            {
                cur.is_corpse = false;
            }
        }

        // Character пустой после смерти, труп держим пока Player жив
        for (const auto& [addr, prev] : previous)
        {
            if (fresh.find(addr) != fresh.end())
                continue;
            if (!still_players.count(addr))
                continue;
            if (!prev.is_player && !prev.is_corpse)
                continue;

            PlayerCache corpse;
            if (RetainCorpseEntry(corpse, prev) && AnyBodyPartValid(corpse))
                fresh[addr] = std::move(corpse);
        }
    }

    std::lock_guard<std::mutex> lock(cacheMutex);
    playerCache.swap(fresh);
}

// кэш игроков, кормит есп/аим
void Cheat::PlayerHandler::CacheThreadLoop()
{
    while (shouldRun.load())
    {
        RefreshGlobals();
        CacheAllPlayers();
        Features::RaycastEngine::Tick();

        // raycast / mesh occluded, кэш стен чаще
        int ms = 250;
        if (Features::RaycastEngine::WantsCache())
            ms = 20;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

void Cheat::PlayerHandler::ClearCache()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    playerCache.clear();
}

void Cheat::PlayerHandler::StartCacheThread()
{
    if (shouldRun.load())
        return;
    shouldRun.store(true);
    cacheThread = std::thread(CacheThreadLoop);
}

void Cheat::PlayerHandler::StopCacheThread()
{
    if (!shouldRun.load())
        return;

    shouldRun.store(false);
    if (cacheThread.joinable())
        cacheThread.join();
}
