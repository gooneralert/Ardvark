bool IsEnemyLabelColor(const Color3& c)
{
    const int r = (int)(c.r * 255.f + 0.5f);
    const int g = (int)(c.g * 255.f + 0.5f);
    const int b = (int)(c.b * 255.f + 0.5f);
    return std::abs(r - 255) <= 8 && std::abs(g - 10) <= 12 && std::abs(b - 20) <= 12;
}

// >0 = enemy folder, <0 = friendly, 0 = unknown
int ScoreFolderEnemy(std::uint64_t folder)
{
    int enemy = 0;
    int friendly = 0;
    for (const auto& model : Instance(folder).GetChildren()) {
        if (model.GetClassName() != "Model")
            continue;
        Color3 col{};
        if (!ReadModelBillboardColor(model, col))
            continue;
        if (IsEnemyLabelColor(col))
            ++enemy;
        else
            ++friendly;
    }
    if (enemy + friendly <= 0)
        return 0;
    return enemy - friendly;
}


Color3 SampleFolderColor(std::uint64_t folder)
{
    Color3 sum{};
    int n = 0;
    Instance f(folder);
    for (const auto& model : f.GetChildren()) {
        if (model.GetClassName() != "Model")
            continue;
        for (const auto& ch : model.GetChildren()) {
            if (!IsPartClass(ch.GetClassName()))
                continue;
            Color3 c = ReadPartColor(ch.address);
            const float lum = (c.r + c.g + c.b) / 3.f;
            if (lum < 0.08f || lum > 0.92f)
                continue;
            const bool blueish = c.b > c.r + 0.05f && c.b > c.g;
            const bool orangeish = c.r > c.b + 0.05f && c.r >= c.g;
            if (!blueish && !orangeish)
                continue;
            sum.r += c.r; sum.g += c.g; sum.b += c.b;
            ++n;
            if (n >= 12)
                break;
        }
        if (n >= 12)
            break;
    }
    if (n <= 0)
        return {};
    return Color3(sum.r / n, sum.g / n, sum.b / n);
}

// мин. дистанция цвета папки до ref (для Team Color при камуфляже)
float FolderColorDist2(std::uint64_t folder, const Color3& ref)
{
    float best = 1e9f;
    if (ref.r + ref.g + ref.b < 0.01f)
        return best;

    for (const auto& model : Instance(folder).GetChildren()) {
        if (model.GetClassName() != "Model")
            continue;
        for (const auto& ch : model.GetChildren()) {
            if (!IsPartClass(ch.GetClassName()))
                continue;
            Color3 c = ReadPartColor(ch.address);
            const float lum = (c.r + c.g + c.b) / 3.f;
            if (lum < 0.05f || lum > 0.95f)
                continue;
            const float d = ColorDist2(c, ref);
            if (d < best)
                best = d;
        }
    }
    return best;
}

TeamSide SideFromColor(const Color3& c)
{
    // Phantoms ≈ bright blue, Ghosts ≈ bright orange
    if (c.b > c.r && c.b > c.g)
        return SidePhantom;
    if (c.r > c.b && c.r >= c.g * 0.8f)
        return SideGhost;
    return SideUnknown;
}

Color3 FindLocalTeamColorPart()
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return {};

    auto cam = g_Memory.Read<std::uint64_t>(
        Globals::Workspace->address + Offsets::Workspace::CurrentCamera);
    if (!g_Memory.IsValid(cam))
        return {};

    for (const auto& m : Instance(cam).GetChildren()) {
        if (m.GetClassName() != "Model")
            continue;
        for (const auto& ch : m.GetChildren()) {
            if (ch.GetName() == "Team Color")
                return ReadPartColor(ch.address);
        }
    }
    return {};
}

// sticky side (лидерборд / Team Color) + team folder как в roblox-ext
static TeamSide g_sticky_side = SideUnknown;
static std::uint64_t g_sticky_team_folder = 0;
static std::uint64_t g_sticky_t0 = 0;
static std::uint64_t g_sticky_t1 = 0;

std::uint64_t ResolveTeamFolderForSide(TeamSide side, std::uint64_t ws_players)
{
    if (side == SideUnknown || !g_Memory.IsValid(ws_players))
        return 0;

    Color3 local_col = FindLocalTeamColorPart();
    std::uint64_t folders[2]{};
    int n = 0;
    std::uint64_t matched = 0;
    std::uint64_t opposite = 0;
    std::uint64_t closest_local = 0;
    float best_d = 1e9f;

    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        if (n < 2)
            folders[n++] = folder.address;

        Color3 sample = SampleFolderColor(folder.address);
        const TeamSide fs = SideFromColor(sample);
        if (fs == side)
            matched = folder.address;
        else if (fs != SideUnknown)
            opposite = folder.address;

        const float d = FolderColorDist2(folder.address, local_col);
        if (d < best_d) {
            best_d = d;
            closest_local = folder.address;
        }
    }

    if (g_Memory.IsValid(matched))
        return matched;

    if (g_Memory.IsValid(opposite) && n == 2)
        return (opposite == folders[0]) ? folders[1] : folders[0];

    // ближе к FP Team Color = наша папка (даже при камуфляже)
    if (n == 2 && g_Memory.IsValid(closest_local) && best_d < 1e8f) {
        const TeamSide local_from_col = SideFromColor(local_col);
        if (local_from_col == side || local_from_col == SideUnknown)
            return closest_local;
        return (closest_local == folders[0]) ? folders[1] : folders[0];
    }

    if (g_Memory.IsValid(closest_local) && best_d < 1e8f)
        return closest_local;

    return 0;
}
