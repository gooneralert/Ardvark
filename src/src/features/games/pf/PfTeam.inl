bool IsActivePlace()
{
    if (!Globals::InstanceDataModel.address)
        return false;
    if (!g_Memory.IsValid(Globals::InstanceDataModel.address))
        return false;
    const bool on = Globals::InstanceDataModel.GetPlaceId() == kPlaceId;
    static bool s_was_on = false;
    if (on && !s_was_on)
        g_Settings.misc.teamcheck = true;
    s_was_on = on;
    if (!on) {
        g_sticky_side = SideUnknown;
        g_sticky_team_folder = 0;
        g_sticky_t0 = 0;
        g_sticky_t1 = 0;
    }
    return on;
}

TeamSide LocalSide()
{
    if (!IsActivePlace())
        return SideUnknown;

    const std::uint64_t lp = LocalPlayerAddr();
    std::string local_name;
    if (g_Memory.IsValid(lp))
        local_name = Instance(lp).GetName();

    auto lb = ReadLeaderboard(local_name);
    if (lb.local_side != SideUnknown) {
        g_sticky_side = lb.local_side;
        return g_sticky_side;
    }

    Color3 tc = FindLocalTeamColorPart();
    TeamSide from_col = SideFromColor(tc);
    if (from_col != SideUnknown) {
        g_sticky_side = from_col;
        return g_sticky_side;
    }

    return g_sticky_side;
}

std::uint64_t ResolveLocalFolderByLeaderboardNames(
    std::uint64_t ws_players, const LeaderboardSnap& lb)
{
    if (lb.local_side == SideUnknown || !g_Memory.IsValid(ws_players))
        return 0;

    auto name_on_side = [&](const std::string& name, TeamSide side) -> bool {
        const auto& list = (side == SidePhantom) ? lb.phantoms : lb.ghosts;
        for (const auto& n : list) {
            if (n == name)
                return true;
        }
        return false;
    };

    std::uint64_t best_folder = 0;
    int best_score = 0;

    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        int score = 0;
        for (const auto& model : folder.GetChildren()) {
            if (model.GetClassName() != "Model")
                continue;
            const std::string name = ReadModelBillboardName(model);
            if (name.empty())
                continue;
            if (name_on_side(name, lb.local_side))
                ++score;
            else if (name_on_side(name, lb.local_side == SidePhantom ? SideGhost : SidePhantom))
                --score;
        }
        if (score > best_score) {
            best_score = score;
            best_folder = folder.address;
        }
    }
    return best_score > 0 ? best_folder : 0;
}

std::uint64_t LocalTeamFolder()
{
    if (!IsActivePlace())
        return 0;

    const std::uint64_t ws_players = FindWorkspacePlayersFolder();
    if (!g_Memory.IsValid(ws_players))
        return 0;

    std::uint64_t folders[2]{};
    int n = 0;
    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        if (n < 2)
            folders[n++] = folder.address;
    }

    const std::uint64_t t0 = n > 0 ? folders[0] : 0;
    const std::uint64_t t1 = n > 1 ? folders[1] : 0;
    if (t0 != g_sticky_t0 || t1 != g_sticky_t1) {
        g_sticky_t0 = t0;
        g_sticky_t1 = t1;
        g_sticky_team_folder = 0;
    }

    // 1) TextLabel.TextColor: враг ≈ (255,10,20), своя папка = НЕ вражеская
    if (n == 2) {
        const int s0 = ScoreFolderEnemy(folders[0]);
        const int s1 = ScoreFolderEnemy(folders[1]);
        std::uint64_t local_folder = 0;
        if (s0 > 0 && s1 <= 0)
            local_folder = folders[1]; // 0 = enemy → local = 1
        else if (s1 > 0 && s0 <= 0)
            local_folder = folders[0];
        else if (s0 < 0 && s1 >= 0)
            local_folder = folders[0]; // 0 = friendly
        else if (s1 < 0 && s0 >= 0)
            local_folder = folders[1];

        if (g_Memory.IsValid(local_folder)) {
            g_sticky_team_folder = local_folder;
            return local_folder;
        }
    }

    // 2) лидерборд: имена с билбордов ↔ Phantom/Ghost board
    const std::uint64_t lp = LocalPlayerAddr();
    std::string local_name;
    if (g_Memory.IsValid(lp))
        local_name = Instance(lp).GetName();
    const auto lb = ReadLeaderboard(local_name);
    {
        const std::uint64_t by_lb = ResolveLocalFolderByLeaderboardNames(ws_players, lb);
        if (g_Memory.IsValid(by_lb)) {
            g_sticky_team_folder = by_lb;
            return by_lb;
        }
    }

    // 3) ModelInstance под Workspace.Players
    if (g_Memory.IsValid(lp)) {
        const std::uint64_t mi = g_Memory.Read<std::uint64_t>(
            lp + Offsets::Player::ModelInstance);
        if (ModelUnderWorkspacePlayers(mi, ws_players)) {
            auto parent = Instance(mi).GetParent();
            if (parent && parent->GetClassName() == "Folder") {
                g_sticky_team_folder = parent->address;
                return g_sticky_team_folder;
            }
        }
    }

    // 4) Team Color → папка
    if (lb.local_side != SideUnknown || LocalSide() != SideUnknown) {
        const TeamSide side = (lb.local_side != SideUnknown) ? lb.local_side : LocalSide();
        const std::uint64_t folder = ResolveTeamFolderForSide(side, ws_players);
        if (g_Memory.IsValid(folder)) {
            g_sticky_team_folder = folder;
            return folder;
        }
    }

    // 5) sticky (не nearest-to-cam — он переворачивал тимчек)
    if (g_Memory.IsValid(g_sticky_team_folder)) {
        auto p = Instance(g_sticky_team_folder).GetParent();
        if (p && p->address == ws_players)
            return g_sticky_team_folder;
        g_sticky_team_folder = 0;
    }

    return 0;
}

bool IsTeammate(const PlayerCache& cache, std::uint64_t local_team_folder)
{
    if (!IsActivePlace())
        return false;
    if (!local_team_folder || !cache.team_folder)
        return false;
    return cache.team_folder == local_team_folder;
}
