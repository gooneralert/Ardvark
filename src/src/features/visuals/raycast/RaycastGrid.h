// pulled into RaycastEngine.cpp anon ns — dont compile alone

int cell_coord(float value, float inv_cell)
{
    if (!std::isfinite(value) || !std::isfinite(inv_cell) || std::fabs(inv_cell) < 1e-6f)
        return 0;

    float scaled = value * inv_cell;
    if (!std::isfinite(scaled))
    {
        if (scaled > 0.f)
            return INT_MAX / 4;

        return INT_MIN / 4;
    }

    // без этого grid улетает
    if (scaled < -1000000.f) scaled = -1000000.f;
    if (scaled > 1000000.f) scaled = 1000000.f;
    return (int)std::floor(scaled);
}

void insert_part_into_grid(occluder_cache& cache, std::uint32_t index, const occluder_part& part)
{
    if (cache.cell_size <= 0.0f)
        return;

    float inv = 1.0f / cache.cell_size;
    const float3x3& rot = part.rotation;
    float hx = part.half_size.x, hy = part.half_size.y, hz = part.half_size.z;
    Vector3 extent(
        std::fabs(rot._11) * hx + std::fabs(rot._12) * hy + std::fabs(rot._13) * hz,
        std::fabs(rot._21) * hx + std::fabs(rot._22) * hy + std::fabs(rot._23) * hz,
        std::fabs(rot._31) * hx + std::fabs(rot._32) * hy + std::fabs(rot._33) * hz);
    extent.x += 2.0f;
    extent.y += 2.0f;
    extent.z += 2.0f;

    Vector3 min = part.position - extent;
    Vector3 max = part.position + extent;
    int min_x = cell_coord(min.x, inv), min_y = cell_coord(min.y, inv), min_z = cell_coord(min.z, inv);
    int max_x = cell_coord(max.x, inv), max_y = cell_coord(max.y, inv), max_z = cell_coord(max.z, inv);

    // огромные партсы не кладём, убьют сетку
    if ((max_x - min_x) > 512 || (max_y - min_y) > 512 || (max_z - min_z) > 512)
        return;

    for (int x = min_x; x <= max_x; ++x)
        for (int y = min_y; y <= max_y; ++y)
            for (int z = min_z; z <= max_z; ++z)
                cache.grid[cell_key{ x, y, z }].push_back(index);
}
