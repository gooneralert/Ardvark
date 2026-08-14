void CollectParts(const Instance& node, std::vector<PartInfo>& out, int depth)
{
    if (depth > 6 || !g_Memory.IsValid(node.address))
        return;

    const std::string cls = node.GetClassName();
    if (IsPartClass(cls)) {
        BasePart bp(node.address);
        PartInfo pi;
        pi.addr = node.address;
        pi.pos = bp.GetPosition();
        pi.size = bp.GetSize();
        pi.volume = std::fabs(pi.size.x * pi.size.y * pi.size.z);
        if (pi.volume > 0.001f &&
            (std::fabs(pi.pos.x) + std::fabs(pi.pos.y) + std::fabs(pi.pos.z)) > 0.01f)
            out.push_back(pi);
    }

    if (cls == "Model" || cls == "Folder" || cls == "Configuration" || depth == 0) {
        for (const auto& c : node.GetChildren())
            CollectParts(c, out, depth + 1);
    }
}

bool PopulateCharacter(std::uint64_t model, PlayerCache& cache)
{
    std::vector<std::uint64_t> all_parts;
    for (const auto& child : Instance(model).GetChildren()) {
        // в roblox-ext только Part (не MeshPart)
        if (child.GetClassName() == "Part")
            all_parts.push_back(child.address);
    }
    if (all_parts.empty())
        return false;

    std::uint64_t head_part = 0;
    std::uint64_t torso_part = 0;
    std::vector<std::uint64_t> limb_parts;
    std::string player_name;

    for (const std::uint64_t part_addr : all_parts) {
        bool is_head = false;
        bool is_torso = false;
        Instance part(part_addr);

        for (const auto& pc : part.GetChildren()) {
            const std::string pccls = pc.GetClassName();
            if (pccls == "BillboardGui") {
                is_head = true;
                if (player_name.empty())
                    player_name = ReadBillboardName(part);
                break;
            }
            if (pccls == "SpotLight") {
                is_torso = true;
                break;
            }
        }

        if (is_head)
            head_part = part_addr;
        else if (is_torso)
            torso_part = part_addr;
        else
            limb_parts.push_back(part_addr);
    }

    if (!head_part && !torso_part)
        return false;

    cache.isR6 = true;
    if (head_part)
        cache.head = MakePart(head_part);
    if (torso_part) {
        cache.upperTorso = MakePart(torso_part);
        // отдельный shared_ptr — ESP is_hrp не должен скипать torso в chams
        cache.humanoidRootPart = MakePart(torso_part);
    }

    if (torso_part && !limb_parts.empty()) {
        BasePart torso_bp(torso_part);
        const Vector3 torso_pos = torso_bp.GetPosition();
        const Matrix4x4 rot = torso_bp.GetRotation();
        // как matrix3 в roblox-ext: right=col0, up=col1
        const float rx = rot.m[0][0], ry_r = rot.m[1][0], rz_r = rot.m[2][0];
        const float ux = rot.m[0][1], uy = rot.m[1][1], uz = rot.m[2][1];

        std::vector<std::pair<float, std::uint64_t>> arms, legs;
        for (const std::uint64_t lp : limb_parts) {
            const Vector3 pos = BasePart(lp).GetPosition();
            const float ox = pos.x - torso_pos.x;
            const float oy = pos.y - torso_pos.y;
            const float oz = pos.z - torso_pos.z;
            const float local_y = ox * ux + oy * uy + oz * uz;
            const float local_x = ox * rx + oy * ry_r + oz * rz_r;
            if (local_y > -0.5f)
                arms.push_back({ local_x, lp });
            else
                legs.push_back({ local_x, lp });
        }

        auto by_x = [](const std::pair<float, std::uint64_t>& a,
                       const std::pair<float, std::uint64_t>& b) {
            return a.first < b.first;
        };
        std::sort(arms.begin(), arms.end(), by_x);
        std::sort(legs.begin(), legs.end(), by_x);

        if (arms.size() >= 2) {
            cache.leftUpperArm = MakePart(arms[0].second);
            cache.rightUpperArm = MakePart(arms[1].second);
        } else if (arms.size() == 1) {
            cache.rightUpperArm = MakePart(arms[0].second);
        }

        if (legs.size() >= 2) {
            cache.leftUpperLeg = MakePart(legs[0].second);
            cache.rightUpperLeg = MakePart(legs[1].second);
        } else if (legs.size() == 1) {
            cache.rightUpperLeg = MakePart(legs[0].second);
        }
    }

    if (!player_name.empty()) {
        cache.name = player_name;
        cache.displayName = player_name;
    } else {
        const std::string model_name = Instance(model).GetName();
        if (!model_name.empty()) {
            cache.name = model_name;
            cache.displayName = model_name;
        }
    }

    return true;
}

void CollectBoardNames(std::uint64_t board, std::vector<std::string>& out)
{
    auto container = FindChildByName(board, "Container");
    if (!g_Memory.IsValid(container))
        return;

    for (const auto& row : Instance(container).GetChildren()) {
        if (row.GetName() != "DisplayPlayerScore")
            continue;
        auto label = FindChildByName(row.address, "TextPlayer");
        std::string name = ReadGuiText(label);
        if (name.empty() || name == "Player")
            continue;
        out.push_back(std::move(name));
    }
}

LeaderboardSnap ReadLeaderboard(const std::string& local_name)
{
    LeaderboardSnap snap;
    const std::uint64_t lp = LocalPlayerAddr();
    const std::uint64_t gui = FindPlayerGui(lp);
    if (!g_Memory.IsValid(gui))
        return snap;

    auto lb = FindChildByName(gui, "LeaderboardScreenGui");
    auto score = FindChildByName(lb, "DisplayScoreFrame");
    auto container = FindChildByName(score, "Container");
    if (!g_Memory.IsValid(container))
        return snap;

    auto phantom_board = FindChildByName(container, "DisplayPhantomBoard");
    auto ghost_board = FindChildByName(container, "DisplayGhostBoard");
    CollectBoardNames(phantom_board, snap.phantoms);
    CollectBoardNames(ghost_board, snap.ghosts);

    if (!local_name.empty()) {
        for (const auto& n : snap.phantoms) {
            if (n == local_name) {
                snap.local_side = SidePhantom;
                break;
            }
        }
        if (snap.local_side == SideUnknown) {
            for (const auto& n : snap.ghosts) {
                if (n == local_name) {
                    snap.local_side = SideGhost;
                    break;
                }
            }
        }
    }
    return snap;
}

std::unordered_map<std::uint64_t, std::string> BuildModelToPlayerName()
{
    std::unordered_map<std::uint64_t, std::string> map;
    std::uint64_t players = 0;
    if (Globals::Players && g_Memory.IsValid(Globals::Players->address))
        players = Globals::Players->address;
    else
        players = FindServiceByClass("Players");
    if (!g_Memory.IsValid(players))
        return map;

    const std::uint64_t ws_players = FindWorkspacePlayersFolder();

    for (const auto& p : Instance(players).GetChildren()) {
        if (p.GetClassName() != "Player")
            continue;
        const std::uint64_t model = g_Memory.Read<std::uint64_t>(
            p.address + Offsets::Player::ModelInstance);
        if (!g_Memory.IsValid(model))
            continue;
        // только живые под Workspace.Players (не труп в Ignore)
        if (g_Memory.IsValid(ws_players) &&
            !ModelUnderWorkspacePlayers(model, ws_players))
            continue;
        std::string name = p.GetName();
        if (!name.empty())
            map[model] = std::move(name);
    }
    return map;
}
