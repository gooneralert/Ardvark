#pragma once

// player update — парты / UpdateCache / corpse
// только из PlayerHandler.cpp

namespace {

bool PartPtrValid(const std::shared_ptr<Cheat::Instance>& p)
{
    return p && g_Memory.IsValid(p->address);
}

bool AnyBodyPartValid(const Cheat::PlayerCache& c)
{
    return PartPtrValid(c.head) || PartPtrValid(c.humanoidRootPart) ||
           PartPtrValid(c.upperTorso) || PartPtrValid(c.lowerTorso) ||
           PartPtrValid(c.leftUpperArm) || PartPtrValid(c.rightUpperArm) ||
           PartPtrValid(c.leftUpperLeg) || PartPtrValid(c.rightUpperLeg) ||
           PartPtrValid(c.leftHand) || PartPtrValid(c.rightHand) ||
           PartPtrValid(c.leftFoot) || PartPtrValid(c.rightFoot);
}

void ClearBodyParts(Cheat::PlayerCache& cache)
{
    cache.head = cache.humanoidRootPart = cache.upperTorso = cache.lowerTorso = nullptr;
    cache.leftUpperArm = cache.leftLowerArm = cache.leftHand = nullptr;
    cache.rightUpperArm = cache.rightLowerArm = cache.rightHand = nullptr;
    cache.leftUpperLeg = cache.leftLowerLeg = cache.leftFoot = nullptr;
    cache.rightUpperLeg = cache.rightLowerLeg = cache.rightFoot = nullptr;
    cache.humanoid = nullptr;
    cache.toolName.clear();
    cache.isR6 = false;
}

void CopyBodyParts(Cheat::PlayerCache& dst, const Cheat::PlayerCache& src)
{
    dst.head = src.head;
    dst.humanoidRootPart = src.humanoidRootPart;
    dst.upperTorso = src.upperTorso;
    dst.lowerTorso = src.lowerTorso;
    dst.leftUpperArm = src.leftUpperArm;
    dst.leftLowerArm = src.leftLowerArm;
    dst.leftHand = src.leftHand;
    dst.rightUpperArm = src.rightUpperArm;
    dst.rightLowerArm = src.rightLowerArm;
    dst.rightHand = src.rightHand;
    dst.leftUpperLeg = src.leftUpperLeg;
    dst.leftLowerLeg = src.leftLowerLeg;
    dst.leftFoot = src.leftFoot;
    dst.rightUpperLeg = src.rightUpperLeg;
    dst.rightLowerLeg = src.rightLowerLeg;
    dst.rightFoot = src.rightFoot;
    if (!dst.humanoid)
        dst.humanoid = src.humanoid;
    dst.isR6 = src.isR6;
}

bool PopulatePartsFromChildren(const std::vector<Cheat::Instance>& parts, Cheat::PlayerCache& cache)
{
    std::uint64_t hum_addr = 0;

    for (const auto& part : parts)
    {
        std::string cls = part.GetClassName();
        if (cls == "Accessory")
            continue;
        if (cls == "Tool")
        {
            cache.toolName = part.GetName();
            continue;
        }

        if (cls == "Humanoid")
        {
            cache.humanoid = std::make_shared<Cheat::Instance>(part);
            hum_addr = part.address;
        }

        std::string nm = part.GetName();

        // r15 / r6 имена, просто мапим
        if (nm == "Head")
        {
            cache.head = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "HumanoidRootPart")
        {
            cache.humanoidRootPart = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "UpperTorso")
        {
            cache.upperTorso = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "LowerTorso")
        {
            cache.lowerTorso = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "LeftUpperArm")
        {
            cache.leftUpperArm = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "LeftLowerArm")
        {
            cache.leftLowerArm = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "LeftHand")
        {
            cache.leftHand = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "RightUpperArm")
        {
            cache.rightUpperArm = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "RightLowerArm")
        {
            cache.rightLowerArm = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "RightHand")
        {
            cache.rightHand = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "LeftUpperLeg")
        {
            cache.leftUpperLeg = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "LeftLowerLeg")
        {
            cache.leftLowerLeg = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "LeftFoot")
        {
            cache.leftFoot = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "RightUpperLeg")
        {
            cache.rightUpperLeg = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "RightLowerLeg")
        {
            cache.rightLowerLeg = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "RightFoot")
        {
            cache.rightFoot = std::make_shared<Cheat::Instance>(part);
        }

        // r6 имена другие
        else if (nm == "Torso")
        {
            cache.upperTorso = std::make_shared<Cheat::Instance>(part);
            cache.isR6 = true;
        }

        else if (nm == "Left Arm")
        {
            cache.leftUpperArm = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "Right Arm")
        {
            cache.rightUpperArm = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "Left Leg")
        {
            cache.leftUpperLeg = std::make_shared<Cheat::Instance>(part);
        }

        else if (nm == "Right Leg")
        {
            cache.rightUpperLeg = std::make_shared<Cheat::Instance>(part);
        }
    }

    if (!cache.humanoidRootPart && hum_addr)
    {
        std::uint64_t root = Cheat::Humanoid(hum_addr).GetRootPartAddress();
        if (g_Memory.IsValid(root))
            cache.humanoidRootPart = std::make_shared<Cheat::Instance>(root);
    }

    return AnyBodyPartValid(cache) || hum_addr != 0;
}

// собираем хэд/руки/ноги по детям модели
bool PopulateParts(std::uint64_t characterAddress, Cheat::PlayerCache& cache)
{
    if (!g_Memory.IsValid(characterAddress))
        return false;

    auto kids = Cheat::Instance(characterAddress).GetChildren();
    bool ok = PopulatePartsFromChildren(kids, cache);

    // иногда тело сидит во вложенных Model/Folder
    for (const auto& child : kids)
    {
        std::string cls = child.GetClassName();
        if (cls == "Model" || cls == "Folder" || cls == "Configuration")
            ok = PopulatePartsFromChildren(child.GetChildren(), cache) || ok;
    }
    return ok;
}

bool IsPlayerDead(const Cheat::PlayerCache& cache)
{
    if (cache.is_corpse)
        return true;
    if (!PartPtrValid(cache.humanoid))
        return false;

    Cheat::Humanoid hum(cache.humanoid->address);
    // 15 = dead, ну или хп уже 0
    if (hum.GetStateId() == 15)
        return true;
    return hum.GetHealth() <= 0.f;
}

bool RefreshCorpseFromCharacter(Cheat::PlayerCache& cache, std::uint64_t character)
{
    if (!g_Memory.IsValid(character))
        return false;

    Cheat::PlayerCache tmp = cache;
    ClearBodyParts(tmp);
    tmp.character = character;
    if (!PopulateParts(character, tmp) || !AnyBodyPartValid(tmp))
        return false;

    CopyBodyParts(cache, tmp);
    cache.character = character;
    cache.humanoid = tmp.humanoid;
    cache.toolName = std::move(tmp.toolName);
    cache.isR6 = tmp.isR6;
    return true;
}

bool RetainCorpseEntry(Cheat::PlayerCache& out, const Cheat::PlayerCache& prev)
{
    out = prev;
    out.is_corpse = true;

    if (RefreshCorpseFromCharacter(out, prev.character))
        return AnyBodyPartValid(out);

    // чара уже нет, держим последние парты пока живы в памяти
    if (AnyBodyPartValid(prev))
    {
        CopyBodyParts(out, prev);
        return true;
    }

    return false;
}

std::uint64_t LocalCharacterAddress()
{
    if (Cheat::Games::PhantomForces::IsActivePlace()) {
        const std::uint64_t pf = Cheat::Games::PhantomForces::LocalCharacter();
        if (g_Memory.IsValid(pf))
            return pf;
    }

    if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
        return 0;

    std::uint64_t local = g_Memory.Read<std::uint64_t>(
        Cheat::Globals::Players->address + ::Player::LocalPlayer);
    if (!g_Memory.IsValid(local))
        return 0;

    return g_Memory.Read<std::uint64_t>(local + ::Player::ModelInstance);
}

}

void Cheat::PlayerHandler::UpdateCache(const Instance& player,
                                       std::unordered_map<std::uint64_t, PlayerCache>& target)
{
    if (!g_Memory.IsValid(player.address))
        return;

    std::uint64_t char_addr = g_Memory.Read<std::uint64_t>(
        player.address + ::Player::ModelInstance);
    if (!g_Memory.IsValid(char_addr))
        return;

    PlayerCache cache(player.address);
    cache.name = player.GetName();
    if (cache.name.empty())
        return;

    cache.character = char_addr;
    cache.team_folder = ResolveTeamFolder(char_addr);
    cache.is_player = true;
    PopulateParts(char_addr, cache);

    // player dn / humanoid dn / username (после parts, humanoid уже есть)
    cache.displayName = Player(player.address).GetDisplayName();
    if ((cache.displayName.empty() || cache.displayName == "Unknown") && cache.humanoid)
        cache.displayName = Humanoid(cache.humanoid->address).GetDisplayName();
    if (cache.displayName.empty() || cache.displayName == "Unknown")
        cache.displayName = cache.name;

    target[player.address] = std::move(cache);
}
