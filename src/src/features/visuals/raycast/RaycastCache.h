// pulled into RaycastEngine.cpp anon ns — dont compile alone

bool append_occluder(std::uint64_t primitive, occluder_cache& cache)
{
    if (!primitive || !has_valid_signature(primitive) || !can_collide(primitive))
        return false;

    Vector3 size{};
    Vector3 position{};
    if (!read_size(primitive, size) || !read_position(primitive, position))
        return false;

    // мусорные / гигантские партсы
    if (size.x <= 0.f || size.y <= 0.f || size.z <= 0.f)
        return false;
    if (size.x > 500.f || size.y > 500.f || size.z > 500.f)
        return false;

    occluder_part part{};
    part.primitive = primitive;
    part.position = position;
    part.half_size = Vector3(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);
    float3x3 rot{};
    if (!read_rotation(primitive, rot))
        rot = identity_rotation();
    part.rotation = rot;
    part.radius_sq = part.half_size.LengthSquared();
    cache.parts.push_back(part);
    insert_part_into_grid(cache, (std::uint32_t)(cache.parts.size() - 1), cache.parts.back());
    return true;
}

void step_builder(occluder_builder& builder, std::size_t step_count)
{
    if (!builder.active || builder.complete || !builder.primitives_base)
        return;

    std::size_t processed = 0;
    while (processed < step_count && builder.cursor < builder.max_scan)
    {
        std::uint64_t primitive = g_Memory.Read<std::uint64_t>(
            builder.primitives_base + builder.cursor * sizeof(std::uint64_t));
        ++builder.cursor;
        ++processed;

        if (!primitive)
        {
            ++builder.consecutive_null_slots;
            // длинная дыра после живых слотов = конец массива
            if (builder.found_non_null_slot && builder.consecutive_null_slots >= 16384)
            {
                builder.complete = true;
                break;
            }

            continue;
        }

        builder.found_non_null_slot = true;
        builder.consecutive_null_slots = 0;

        if (builder.ignore_primitives.find(primitive) == builder.ignore_primitives.end())
            append_occluder(primitive, builder.building_cache);
    }

    if (builder.cursor >= builder.max_scan)
        builder.complete = true;

    if (builder.complete)
    {
        builder.active = false;
        builder.building_cache.complete = true;
        builder.building_cache.build_time = now_sec();
    }
}

std::atomic<std::shared_ptr<occluder_cache>>& cache_storage() {
    static std::atomic<std::shared_ptr<occluder_cache>> cached{ std::make_shared<occluder_cache>() };
    return cached;
}

occluder_builder& builder_storage() {
    static occluder_builder builder;
    return builder;
}

std::mutex& builder_mutex() {
    static std::mutex mu;
    return mu;
}

std::uint64_t part_primitive(const std::shared_ptr<Instance>& part)
{
    if (!part || !g_Memory.IsValid(part->address))
        return 0;

    std::uint64_t prim = g_Memory.Read<std::uint64_t>(
        part->address + ::BasePart::Primitive);
    if (!g_Memory.IsValid(prim))
        return 0;

    return prim;
}

void collect_cache_primitives(const PlayerCache& cache, std::unordered_set<std::uint64_t>& out)
{
    const std::shared_ptr<Instance>* parts[] = {
        &cache.head, &cache.humanoidRootPart, &cache.upperTorso, &cache.lowerTorso,
        &cache.leftUpperArm, &cache.leftLowerArm, &cache.leftHand,
        &cache.rightUpperArm, &cache.rightLowerArm, &cache.rightHand,
        &cache.leftUpperLeg, &cache.leftLowerLeg, &cache.leftFoot,
        &cache.rightUpperLeg, &cache.rightLowerLeg, &cache.rightFoot
    };

    for (const auto* p : parts)
    {
        std::uint64_t prim = part_primitive(*p);
        if (prim)
            out.insert(prim);
    }
}

void gather_player_primitives(std::unordered_set<std::uint64_t>& out)
{
    out.clear();
    PlayerHandler::ForEachPlayer([&](const PlayerCache& cache) {
        collect_cache_primitives(cache, out);
    });
}

std::uint64_t primitives_root()
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return 0;

    std::uint64_t world = g_Memory.Read<std::uint64_t>(
        Globals::Workspace->address + ::Workspace::World);
    if (!g_Memory.IsValid(world))
        return 0;

    std::uint64_t p2 = g_Memory.Read<std::uint64_t>(world + ::World::Primitives);
    if (g_Memory.IsValid(p2))
        return p2;

    // фоллбек если оффсет уехал
    std::uint64_t p1 = g_Memory.Read<std::uint64_t>(world + 0x240);
    if (g_Memory.IsValid(p1))
        return p1;

    return 0;
}

std::shared_ptr<occluder_cache> get_occluder_cache()
{
    auto& cache_ref = cache_storage();
    auto cache_ptr = cache_ref.load(std::memory_order_acquire);
    double now = now_sec();
    bool empty = !cache_ptr || cache_ptr->parts.empty();
    bool stale = cache_ptr && cache_ptr->complete && (now - cache_ptr->build_time) > 4.0;

    std::uint64_t base = primitives_root();
    auto& builder = builder_storage();

    {
        std::lock_guard<std::mutex> lock(builder_mutex());

        if (builder.active && builder.primitives_base != base)
            builder.reset();

        if ((empty || stale) && !builder.active && base)
        {
            std::unordered_set<std::uint64_t> ignore;
            gather_player_primitives(ignore);
            builder.start(base, std::move(ignore), 32.f, 524288);
        }

        if (builder.active)
        {
            if (builder.last_step <= 0.0 || (now - builder.last_step) >= 0.02)
            {
                builder.last_step = now;
                step_builder(builder, 2048);
            }

            if (builder.complete)
            {
                auto new_cache = std::make_shared<occluder_cache>(std::move(builder.building_cache));
                cache_ref.store(std::move(new_cache), std::memory_order_release);
                builder.reset();
            }
        }
    }

    return cache_ref.load(std::memory_order_acquire);
}
