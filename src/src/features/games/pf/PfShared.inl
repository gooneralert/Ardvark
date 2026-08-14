/*
 * PF live (place 292439477) — что нашли через MCP:
 *
 * - Имена сервисов DataModel зашифрованы (Players/Teams и т.д. ищем по ClassName).
 * - Workspace.Players (Folder, имя "Players" стабильно) содержит РОВНО 2 team-folder.
 *   Имена папок ("Bright blue"/"Bright orange") теперь рандом — нельзя матчить по Name.
 * - Внутри team-folder: Model на каждого живого.
 * - Логика как в roblox-ext/cache.cpp (pf_mode):
 *     Head  = Part с BillboardGui (+ TextLabel = имя)
 *     Torso = Part с SpotLight
 *     Limbs = остальные Part; L/R по local_x/local_y относительно Torso
 *   Size у парт часто ~0 → ESP подставляет R6 sizes.
 * - Teamcheck (точный):
 *     BillboardGui/TextLabel.TextColor3 ≈ (255,10,20) = враг (как fragment/PF nametag).
 *     Своя папка = где теги НЕ вражеские; + сверка имён с лидербордом.
 *     Ближайшая к камере — НЕ используем (переворачивает тимчек у стены).
 * - PlaceId 292439477 — автоматически.
 */

struct PartInfo {
    std::uint64_t addr = 0;
    Vector3 pos{};
    Vector3 size{};
    float volume = 0.f;
};

struct TeamFolder {
    std::uint64_t addr = 0;
    TeamSide side = SideUnknown;
    Color3 sample_color{};
    bool has_color = false;
};

bool ModelUnderWorkspacePlayers(std::uint64_t model, std::uint64_t ws_players);
std::uint64_t FindWorkspacePlayersFolder();

std::uint64_t FindServiceByClass(const char* cls)
{
    if (!Globals::InstanceDataModel.address)
        return 0;
    for (const auto& c : Globals::InstanceDataModel.GetChildren()) {
        if (c.GetClassName() == cls)
            return c.address;
    }
    return 0;
}

std::uint64_t LocalPlayerAddr()
{
    auto players = Globals::Players;
    if (!players || !g_Memory.IsValid(players->address)) {
        const std::uint64_t svc = FindServiceByClass("Players");
        if (!g_Memory.IsValid(svc))
            return 0;
        return g_Memory.Read<std::uint64_t>(svc + Offsets::Player::LocalPlayer);
    }
    return g_Memory.Read<std::uint64_t>(
        players->address + Offsets::Player::LocalPlayer);
}

std::string ReadGuiText(std::uint64_t label)
{
    if (!g_Memory.IsValid(label))
        return {};
    return g_Memory.ReadString(label + Offsets::GuiObject::Text);
}

Color3 ReadPartColor(std::uint64_t part)
{
    if (!g_Memory.IsValid(part))
        return {};
    return g_Memory.Read<Color3>(part + Offsets::BasePart::Color3);
}

float ColorDist2(const Color3& a, const Color3& b)
{
    const float dr = a.r - b.r;
    const float dg = a.g - b.g;
    const float db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

bool IsPartClass(const std::string& cls)
{
    return cls == "Part" || cls == "MeshPart" || cls == "UnionOperation" ||
           cls == "WedgePart" || cls == "CornerWedgePart" || cls == "TrussPart";
}

std::shared_ptr<Instance> MakePart(std::uint64_t addr)
{
    return std::make_shared<Instance>(addr);
}

std::uint64_t FindBillboardTextLabel(const Instance& model)
{
    for (const auto& part : model.GetChildren()) {
        if (part.GetClassName() != "Part" && part.GetClassName() != "MeshPart")
            continue;
        for (const auto& c : part.GetChildren()) {
            if (c.GetClassName() != "BillboardGui")
                continue;
            for (const auto& bc : c.GetChildren()) {
                if (bc.GetClassName() == "TextLabel")
                    return bc.address;
            }
        }
    }
    return 0;
}

std::string ReadBillboardName(const Instance& part)
{
    for (const auto& c : part.GetChildren()) {
        if (c.GetClassName() != "BillboardGui")
            continue;
        for (const auto& bc : c.GetChildren()) {
            if (bc.GetClassName() != "TextLabel")
                continue;
            std::string t = ReadGuiText(bc.address);
            if (!t.empty())
                return t;
        }
    }
    return {};
}

std::string ReadModelBillboardName(const Instance& model)
{
    const std::uint64_t label = FindBillboardTextLabel(model);
    if (!g_Memory.IsValid(label))
        return {};
    return ReadGuiText(label);
}

bool ReadModelBillboardColor(const Instance& model, Color3& out)
{
    const std::uint64_t label = FindBillboardTextLabel(model);
    if (!g_Memory.IsValid(label))
        return false;
    out = g_Memory.Read<Color3>(label + Offsets::GuiObject::TextColor3);
    if (!std::isfinite(out.r) || !std::isfinite(out.g) || !std::isfinite(out.b))
        return false;
    if (out.r < 0.f) out.r = 0.f;
    if (out.r > 1.f) out.r = 1.f;
    if (out.g < 0.f) out.g = 0.f;
    if (out.g > 1.f) out.g = 1.f;
    if (out.b < 0.f) out.b = 0.f;
    if (out.b > 1.f) out.b = 1.f;
    return true;
}


std::uint64_t FindWorkspacePlayersFolder()
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return 0;
    for (const auto& c : Globals::Workspace->GetChildren()) {
        if (c.GetClassName() == "Folder" && c.GetName() == "Players")
            return c.address;
    }
    return 0;
}

std::uint64_t FindPlayerGui(std::uint64_t lp)
{
    if (!g_Memory.IsValid(lp))
        return 0;
    for (const auto& c : Instance(lp).GetChildren()) {
        if (c.GetClassName() == "PlayerGui")
            return c.address;
    }
    return 0;
}

std::uint64_t FindChildByName(std::uint64_t parent, const char* name)
{
    if (!g_Memory.IsValid(parent))
        return 0;
    for (const auto& c : Instance(parent).GetChildren()) {
        if (c.GetName() == name)
            return c.address;
    }
    return 0;
}

struct LeaderboardSnap {
    std::vector<std::string> phantoms;
    std::vector<std::string> ghosts;
    TeamSide local_side = SideUnknown;
};

Vector3 CameraPos()
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return {};
    auto cam = g_Memory.Read<std::uint64_t>(
        Globals::Workspace->address + Offsets::Workspace::CurrentCamera);
    if (!g_Memory.IsValid(cam))
        return {};
    return g_Memory.Read<Vector3>(cam + Offsets::Camera::Position);
}

bool ModelUnderWorkspacePlayers(std::uint64_t model, std::uint64_t ws_players)
{
    if (!g_Memory.IsValid(model) || !g_Memory.IsValid(ws_players))
        return false;
    auto parent = Instance(model).GetParent();
    if (!parent || parent->GetClassName() != "Folder")
        return false;
    auto grand = parent->GetParent();
    return grand && grand->address == ws_players;
}
