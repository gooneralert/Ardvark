// pulled into RaycastEngine.cpp anon ns — dont compile alone

// видна ли точка сквозь кэш стен
bool line_of_sight_clear(const Vector3& from, const Vector3& to,
                         const occluder_cache& cache,
                         const std::unordered_set<std::uint64_t>& ignore)
{
    if (!std::isfinite(from.x) || !std::isfinite(to.x))
        return true;

    Vector3 delta = to - from;
    float distance = delta.Length();
    if (!std::isfinite(distance) || distance <= 1e-5f)
        return true;
    delta /= distance;

    if (cache.parts.empty())
        return true;

    auto hit_blocks = [&](const occluder_part& part) -> bool {
        if (part.primitive && ignore.find(part.primitive) != ignore.end())
            return false;
        float hit = 0.0f;
        if (!ray_intersects_obb(from, delta, part, distance, &hit))
            return false;
        // края луча не считаем, иначе стены на камере/таргете
        if (hit <= 0.15f) return false;
        if ((distance - hit) <= 0.55f) return false;
        return true;
    };

    if (cache.cell_size <= 0.0f || cache.grid.empty()) {
        for (const auto& part : cache.parts)
            if (hit_blocks(part)) return false;
        return true;
    }

    static std::atomic<std::uint64_t> query_counter{ 1 };
    const std::uint64_t query_id = query_counter.fetch_add(1, std::memory_order_relaxed) + 1;

    const float cell_size = cache.cell_size;
    const float inv_cell = 1.0f / cell_size;

    int cell_x = cell_coord(from.x, inv_cell);
    int cell_y = cell_coord(from.y, inv_cell);
    int cell_z = cell_coord(from.z, inv_cell);
    const int end_x = cell_coord(to.x, inv_cell);
    const int end_y = cell_coord(to.y, inv_cell);
    const int end_z = cell_coord(to.z, inv_cell);

    int step_x = 0, step_y = 0, step_z = 0;
    float t_max_x = INFINITY, t_max_y = INFINITY, t_max_z = INFINITY;
    float t_delta_x = INFINITY, t_delta_y = INFINITY, t_delta_z = INFINITY;

    auto setup_axis = [&](float origin, float dir, int cell, int& step, float& t_max, float& t_delta) {
        if (std::fabs(dir) < 1e-6f) {
            step = 0; t_max = INFINITY; t_delta = INFINITY;
            return;
        }
        if (dir > 0.0f) {
            step = 1;
            t_max = ((static_cast<float>(cell) + 1.0f) * cell_size - origin) / dir;
        }

        else
        {
            step = -1;
            t_max = (static_cast<float>(cell) * cell_size - origin) / dir;
        }
        t_delta = cell_size / std::fabs(dir);
    };

    setup_axis(from.x, delta.x, cell_x, step_x, t_max_x, t_delta_x);
    setup_axis(from.y, delta.y, cell_y, step_y, t_max_y, t_delta_y);
    setup_axis(from.z, delta.z, cell_z, step_z, t_max_z, t_delta_z);

    const int max_steps = 1 + std::abs(end_x - cell_x) + std::abs(end_y - cell_y) + std::abs(end_z - cell_z);
    float t = 0.0f;

    for (int step = 0; step <= max_steps; ++step) {
        auto it = cache.grid.find(cell_key{ cell_x, cell_y, cell_z });
        if (it != cache.grid.end()) {
            for (std::uint32_t index : it->second) {
                if (index >= cache.parts.size()) continue;
                auto& part = cache.parts[index];
                const std::uint64_t last = part.last_query_id.load(std::memory_order_relaxed);
                if (last == query_id) continue;
                part.last_query_id.store(query_id, std::memory_order_relaxed);
                if (hit_blocks(part))
                    return false;
            }
        }

        if (cell_x == end_x && cell_y == end_y && cell_z == end_z)
            break;

        if (t_max_x < t_max_y) {
            if (t_max_x < t_max_z) { cell_x += step_x; t = t_max_x; t_max_x += t_delta_x; }

            else { cell_z += step_z; t = t_max_z; t_max_z += t_delta_z; }
        }

        else
        {
            if (t_max_y < t_max_z) { cell_y += step_y; t = t_max_y; t_max_y += t_delta_y; }

            else { cell_z += step_z; t = t_max_z; t_max_z += t_delta_z; }
        }

        if (t > distance)
            break;
    }

    return true;
}

bool part_world_pos(const std::shared_ptr<Instance>& part, Vector3& out)
{
    if (!part || !g_Memory.IsValid(part->address))
        return false;
    const Vector3 pos = BasePart(part->address).GetPosition();
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
        return false;
    out = pos;
    return true;
}

bool is_visible_impl(const PlayerCache& player, const Vector3& camera_pos,
                     const occluder_cache& occluders, float& out_coverage)
{
    if (occluders.parts.empty())
    {
        out_coverage = 1.0f;
        return true;
    }

    std::unordered_set<std::uint64_t> ignore;
    collect_cache_primitives(player, ignore);

    // локал тоже в игнор, иначе сам себя перекрывает
    if (Globals::Players && g_Memory.IsValid(Globals::Players->address))
    {
        std::uint64_t lp = g_Memory.Read<std::uint64_t>(
            Globals::Players->address + ::Player::LocalPlayer);
        if (g_Memory.IsValid(lp))
        {
            std::uint64_t character = g_Memory.Read<std::uint64_t>(
                lp + ::Player::ModelInstance);
            if (g_Memory.IsValid(character))
            {
                for (const auto& child : Instance(character).GetChildren())
                {
                    auto cls = child.GetClassName();
                    if (cls == "Part" || cls == "MeshPart" || cls == "WedgePart" ||
                        cls == "CornerWedgePart" || cls == "UnionOperation" || cls == "TrussPart")
                    {
                        std::uint64_t prim = g_Memory.Read<std::uint64_t>(
                            child.address + ::BasePart::Primitive);
                        if (g_Memory.IsValid(prim))
                            ignore.insert(prim);
                    }
                }
            }
        }
    }

    std::vector<Vector3> samples;
    samples.reserve(8);
    auto push = [&](const std::shared_ptr<Instance>& p) {
        Vector3 pos{};
        if (part_world_pos(p, pos))
            samples.push_back(pos);
    };

    push(player.head);
    push(player.humanoidRootPart);
    if (!player.isR6)
    {
        push(player.upperTorso);
        push(player.lowerTorso);
    }

    if (samples.empty())
    {
        out_coverage = 1.0f;
        return true;
    }

    if (line_of_sight_clear(camera_pos, samples.front(), occluders, ignore))
    {
        out_coverage = 1.0f;
        return true;
    }

    int clear = 0;
    int total = (int)samples.size();
    int required = (int)std::ceil(total * 0.10f);
    if (required < 1) required = 1;
    if (total <= 6) required = 1;
    if (required > total) required = total;

    int tested = 0;
    for (const auto& sample : samples)
    {
        ++tested;
        if (line_of_sight_clear(camera_pos, sample, occluders, ignore))
        {
            ++clear;
            if (clear >= required)
            {
                out_coverage = (float)clear / (float)tested;
                return true;
            }
        }

        if (clear + (total - tested) < required)
            break;
    }

    int div = tested;
    if (div < 1) div = 1;
    out_coverage = (float)clear / (float)div;
    return clear >= required;
}
