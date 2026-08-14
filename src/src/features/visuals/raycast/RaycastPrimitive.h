// pulled into RaycastEngine.cpp anon ns — dont compile alone

bool read_rotation(std::uint64_t prim, float3x3& out)
{
    if (!prim)
        return false;

    float3x3 rot{};
    g_Memory.ReadRaw(prim + ::Primitive::Rotation, &rot, sizeof(rot));
    const float* values = reinterpret_cast<const float*>(&rot);
    for (int i = 0; i < 9; ++i)
    {
        if (!std::isfinite(values[i]))
            return false;
    }

    out = rot;
    return true;
}

bool read_position(std::uint64_t prim, Vector3& out)
{
    if (!prim)
        return false;

    Vector3 pos = g_Memory.Read<Vector3>(prim + ::Primitive::Position);
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
        return false;

    out = pos;
    return true;
}

bool read_size(std::uint64_t prim, Vector3& out)
{
    if (!prim)
        return false;

    Vector3 size = g_Memory.Read<Vector3>(prim + ::Primitive::Size);
    if (!std::isfinite(size.x) || !std::isfinite(size.y) || !std::isfinite(size.z))
        return false;

    out = size;
    return true;
}

bool has_valid_signature(std::uint64_t prim)
{
    if (!prim)
        return false;

    std::uint32_t raw = g_Memory.Read<std::uint32_t>(prim + ::Primitive::Validate);
    // 0x6 где-то в байтах
    if (raw == 0x6)
        return true;
    if ((raw & 0xFFu) == 0x6)
        return true;
    if (((raw >> 8) & 0xFFu) == 0x6)
        return true;
    if (((raw >> 16) & 0xFFu) == 0x6)
        return true;
    if (((raw >> 24) & 0xFFu) == 0x6)
        return true;

    return false;
}

bool can_collide(std::uint64_t prim)
{
    std::uint8_t flags = g_Memory.Read<std::uint8_t>(prim + ::Primitive::Flags);
    return (flags & (std::uint8_t)::PrimitiveFlags::CanCollide) != 0;
}
