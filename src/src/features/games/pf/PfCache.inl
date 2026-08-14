std::uint64_t LocalCharacter()
{
    if (!IsActivePlace())
        return 0;

    const std::uint64_t ws_players = FindWorkspacePlayersFolder();
    const std::uint64_t lp = LocalPlayerAddr();

    // только ModelInstance под Workspace.Players (не труп в Ignore)
    if (g_Memory.IsValid(lp)) {
        const std::uint64_t mi = g_Memory.Read<std::uint64_t>(
            lp + Offsets::Player::ModelInstance);
        if (ModelUnderWorkspacePlayers(mi, ws_players))
            return mi;
    }

    // узкий fallback только чтобы не рисовать себя — НЕ для team folder
    if (!g_Memory.IsValid(ws_players))
        return 0;

    const Vector3 cam = CameraPos();
    float best_d2 = 4.f * 4.f;
    std::uint64_t best = 0;

    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        for (const auto& model : folder.GetChildren()) {
            if (model.GetClassName() != "Model")
                continue;
            std::vector<PartInfo> parts;
            CollectParts(model, parts, 0);
            if (parts.empty())
                continue;
            Vector3 c{};
            for (const auto& p : parts) {
                c.x += p.pos.x; c.y += p.pos.y; c.z += p.pos.z;
            }
            const float inv = 1.f / (float)parts.size();
            c.x *= inv; c.y *= inv; c.z *= inv;
            const float dx = c.x - cam.x;
            const float dy = c.y - cam.y;
            const float dz = c.z - cam.z;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best_d2) {
                best_d2 = d2;
                best = model.address;
            }
        }
    }
    return best;
}

void MergePlayers(std::unordered_map<std::uint64_t, PlayerCache>& target)
{
    if (!IsActivePlace())
        return;

    const std::uint64_t ws_players = FindWorkspacePlayersFolder();
    if (!g_Memory.IsValid(ws_players))
        return;

    const std::uint64_t lp = LocalPlayerAddr();
    std::string local_name;
    if (g_Memory.IsValid(lp))
        local_name = Instance(lp).GetName();

    const auto lb = ReadLeaderboard(local_name);
    const auto model_to_name = BuildModelToPlayerName();
    const std::uint64_t local_char = LocalCharacter();
    const std::uint64_t local_team = LocalTeamFolder();

    // имена с доски по стороне, ещё не занятые ModelInstance
    std::unordered_set<std::string> used_names;
    for (const auto& [m, n] : model_to_name)
        used_names.insert(n);

    auto take_lb_name = [&](TeamSide side) -> std::string {
        const auto& list = (side == SidePhantom) ? lb.phantoms : lb.ghosts;
        for (const auto& n : list) {
            if (n == local_name)
                continue;
            if (used_names.count(n))
                continue;
            used_names.insert(n);
            return n;
        }
        return {};
    };

    // разметить folder → side
    std::unordered_map<std::uint64_t, TeamSide> folder_side;
    Color3 local_col = FindLocalTeamColorPart();
    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        Color3 sample = SampleFolderColor(folder.address);
        TeamSide side = SideFromColor(sample);
        if (side == SideUnknown && local_team == folder.address)
            side = lb.local_side;
        if (side == SideUnknown && local_col.r + local_col.g + local_col.b > 0.01f) {
            // если цвет папки ближе к локальному Team Color — наша сторона
            // иначе противоположная
            // (второй папке назначим позже)
            side = SideFromColor(local_col);
            if (folder.address != local_team && side != SideUnknown)
                side = (side == SidePhantom) ? SideGhost : SidePhantom;
        }
        folder_side[folder.address] = side;
    }

    // если обе unknown — по local_team + lb
    if (g_Memory.IsValid(local_team) && lb.local_side != SideUnknown) {
        folder_side[local_team] = lb.local_side;
        for (auto& [fa, s] : folder_side) {
            if (fa != local_team)
                s = (lb.local_side == SidePhantom) ? SideGhost : SidePhantom;
        }
    }

    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;

        const TeamSide side = folder_side.count(folder.address)
            ? folder_side[folder.address] : SideUnknown;

        for (const auto& model : folder.GetChildren()) {
            if (model.GetClassName() != "Model")
                continue;
            if (model.address == local_char)
                continue;

            PlayerCache cache(model.address);
            cache.character = model.address;
            cache.team_folder = folder.address;
            cache.is_player = true;
            cache.isR6 = true;

            if (!PopulateCharacter(model.address, cache))
                continue;

            // имя: Billboard TextLabel → ModelInstance → лидерборд
            if (cache.name.empty() || cache.name == Instance(model.address).GetName()) {
                auto it = model_to_name.find(model.address);
                if (it != model_to_name.end()) {
                    cache.name = it->second;
                    cache.displayName = it->second;
                } else {
                    std::string n = take_lb_name(side);
                    if (!n.empty()) {
                        cache.name = n;
                        cache.displayName = n;
                    }
                }
            }
            if (cache.name.empty())
                continue;
            if (cache.name == local_name)
                continue;
            used_names.insert(cache.name);

            // ключ = адрес модели (у PF нет нормального Player→Character)
            target[model.address] = std::move(cache);
        }
    }
}
