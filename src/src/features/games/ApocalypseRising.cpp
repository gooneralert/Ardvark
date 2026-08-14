#include "pch.h"
#include "ApocalypseRising.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

#include <unordered_set>

namespace Cheat {
namespace Games {
namespace ApocalypseRising {
namespace {

std::uint64_t LocalPlayerAddr()
{
    if (!Globals::Players || !g_Memory.IsValid(Globals::Players->address))
        return 0;
    return g_Memory.Read<std::uint64_t>(
        Globals::Players->address + ::Player::LocalPlayer);
}

std::uint64_t LocalCharacter()
{
    const std::uint64_t lp = LocalPlayerAddr();
    if (!g_Memory.IsValid(lp))
        return 0;
    return g_Memory.Read<std::uint64_t>(lp + ::Player::ModelInstance);
}

std::shared_ptr<Instance> FindWorkspaceFolder(const char* name)
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return nullptr;
    return Globals::Workspace->FindFirstChild(name);
}

// минимальный парсинг R15 — без лишнего
bool PopulateParts(std::uint64_t model, PlayerCache& cache)
{
    if (!g_Memory.IsValid(model))
        return false;

    for (const auto& child : Instance(model).GetChildren())
    {
        const std::string cls = child.GetClassName();
        const std::string nm = child.GetName();

        if (cls == "Humanoid")
            cache.humanoid = std::make_shared<Instance>(child);

        if (nm == "Head")
            cache.head = std::make_shared<Instance>(child);
        else if (nm == "HumanoidRootPart")
            cache.humanoidRootPart = std::make_shared<Instance>(child);
        else if (nm == "UpperTorso")
            cache.upperTorso = std::make_shared<Instance>(child);
        else if (nm == "LowerTorso")
            cache.lowerTorso = std::make_shared<Instance>(child);
        else if (nm == "LeftUpperArm")
            cache.leftUpperArm = std::make_shared<Instance>(child);
        else if (nm == "LeftLowerArm")
            cache.leftLowerArm = std::make_shared<Instance>(child);
        else if (nm == "LeftHand")
            cache.leftHand = std::make_shared<Instance>(child);
        else if (nm == "RightUpperArm")
            cache.rightUpperArm = std::make_shared<Instance>(child);
        else if (nm == "RightLowerArm")
            cache.rightLowerArm = std::make_shared<Instance>(child);
        else if (nm == "RightHand")
            cache.rightHand = std::make_shared<Instance>(child);
        else if (nm == "LeftUpperLeg")
            cache.leftUpperLeg = std::make_shared<Instance>(child);
        else if (nm == "LeftLowerLeg")
            cache.leftLowerLeg = std::make_shared<Instance>(child);
        else if (nm == "LeftFoot")
            cache.leftFoot = std::make_shared<Instance>(child);
        else if (nm == "RightUpperLeg")
            cache.rightUpperLeg = std::make_shared<Instance>(child);
        else if (nm == "RightLowerLeg")
            cache.rightLowerLeg = std::make_shared<Instance>(child);
        else if (nm == "RightFoot")
            cache.rightFoot = std::make_shared<Instance>(child);
    }

    return cache.humanoidRootPart || cache.head;
}

// ModelInstance → Player
void BuildModelOwners(std::unordered_map<std::uint64_t, Instance>& out)
{
    out.clear();
    if (!Globals::Players || !g_Memory.IsValid(Globals::Players->address))
        return;

    for (const auto& player : Globals::Players->GetChildren())
    {
        if (player.GetClassName() != "Player")
            continue;
        const std::uint64_t model = g_Memory.Read<std::uint64_t>(
            player.address + ::Player::ModelInstance);
        if (g_Memory.IsValid(model))
            out[model] = player;
    }
}

void AddModel(
    const Instance& model,
    bool as_player,
    const Instance* owner,
    const std::unordered_set<std::uint64_t>& known_chars,
    std::uint64_t local_char,
    std::unordered_map<std::uint64_t, PlayerCache>& target)
{
    if (model.GetClassName() != "Model")
        return;
    if (model.address == local_char)
        return;
    if (known_chars.count(model.address))
        return;

    PlayerCache cache(owner ? owner->address : model.address);
    if (!PopulateParts(model.address, cache))
        return;

    if (owner)
    {
        cache.name = owner->GetName();
        cache.displayName = Player(owner->address).GetDisplayName();
        if (cache.displayName.empty() || cache.displayName == "Unknown")
            cache.displayName = cache.name;
        cache.is_player = true;
    }
    else
    {
        cache.name = model.GetName();
        if (cache.name.empty())
            cache.name = as_player ? "player" : "zombie";
        cache.displayName = cache.name;
        cache.is_player = as_player;
    }

    cache.character = model.address;
    target[cache.address] = std::move(cache);
}

} // namespace

bool IsActivePlace()
{
    if (!Globals::InstanceDataModel.address)
        return false;
    if (!g_Memory.IsValid(Globals::InstanceDataModel.address))
        return false;
    return Globals::InstanceDataModel.GetPlaceId() == kPlaceId;
}

void MergeEntities(std::unordered_map<std::uint64_t, PlayerCache>& target)
{
    if (!IsActivePlace())
        return;
    if (!g_Settings.esp.apocalypse)
        return;

    std::unordered_set<std::uint64_t> known_chars;
    known_chars.reserve(target.size() * 2 + 8);
    for (const auto& [_, cache] : target)
    {
        if (cache.character)
            known_chars.insert(cache.character);
    }

    const std::uint64_t local_char = LocalCharacter();

    std::unordered_map<std::uint64_t, Instance> owners;
    BuildModelOwners(owners);

    // игроки: Workspace.Characters (имена с Players.ModelInstance)
    if (auto chars = FindWorkspaceFolder("Characters"))
    {
        for (const auto& model : chars->GetChildren())
        {
            const Instance* owner = nullptr;
            auto it = owners.find(model.address);
            if (it != owners.end())
                owner = &it->second;
            AddModel(model, true, owner, known_chars, local_char, target);
            known_chars.insert(model.address);
        }
    }

    // зомби: Workspace.Zombies → bots (esp.bots)
    if (g_Settings.esp.bots)
    {
        if (auto zombies = FindWorkspaceFolder("Zombies"))
        {
            for (const auto& model : zombies->GetChildren())
            {
                AddModel(model, false, nullptr, known_chars, local_char, target);
                known_chars.insert(model.address);
            }
        }
    }
}

} // namespace ApocalypseRising
} // namespace Games
} // namespace Cheat
