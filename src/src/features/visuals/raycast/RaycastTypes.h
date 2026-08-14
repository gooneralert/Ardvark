// pulled into RaycastEngine.cpp anon ns — dont compile alone

using clock = std::chrono::steady_clock;

struct float3x3 {
    float _11{}, _12{}, _13{};
    float _21{}, _22{}, _23{};
    float _31{}, _32{}, _33{};
};

struct occluder_part {
    std::uint64_t primitive = 0;
    Vector3 position{};
    Vector3 half_size{};
    float3x3 rotation{};
    float radius_sq = 0.0f;
    mutable std::atomic<std::uint64_t> last_query_id{ 0 };

    occluder_part() = default;
    occluder_part(const occluder_part& o)
        : primitive(o.primitive), position(o.position), half_size(o.half_size),
          rotation(o.rotation), radius_sq(o.radius_sq) {
        last_query_id.store(o.last_query_id.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    occluder_part& operator=(const occluder_part& o) {
        if (this != &o) {
            primitive = o.primitive; position = o.position; half_size = o.half_size;
            rotation = o.rotation; radius_sq = o.radius_sq;
            last_query_id.store(o.last_query_id.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }
    occluder_part(occluder_part&& o) noexcept
        : primitive(o.primitive), position(o.position), half_size(o.half_size),
          rotation(o.rotation), radius_sq(o.radius_sq) {
        last_query_id.store(o.last_query_id.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    occluder_part& operator=(occluder_part&& o) noexcept {
        if (this != &o) {
            primitive = o.primitive; position = o.position; half_size = o.half_size;
            rotation = o.rotation; radius_sq = o.radius_sq;
            last_query_id.store(o.last_query_id.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }
};

struct cell_key {
    int x = 0, y = 0, z = 0;
    bool operator==(const cell_key& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct cell_key_hash {
    std::size_t operator()(const cell_key& k) const noexcept {
        std::size_t h = 0;
        auto combine = [&](std::size_t v) { h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2); };
        combine(std::hash<int>{}(k.x));
        combine(std::hash<int>{}(k.y));
        combine(std::hash<int>{}(k.z));
        return h;
    }
};

struct occluder_cache {
    std::vector<occluder_part> parts;
    std::unordered_map<cell_key, std::vector<std::uint32_t>, cell_key_hash> grid;
    float cell_size = 32.0f;
    bool complete = false;
    double build_time = 0.0;
};

// кэш стен билдим кусками, иначе фпс в ноль
struct occluder_builder {
    std::uint64_t primitives_base = 0;
    std::size_t cursor = 0;
    std::size_t max_scan = 0;
    bool active = false;
    bool complete = false;
    double last_step = 0.0;
    std::size_t consecutive_null_slots = 0;
    bool found_non_null_slot = false;
    occluder_cache building_cache;
    std::unordered_set<std::uint64_t> ignore_primitives;

    void reset() {
        *this = occluder_builder{};
    }

    void start(std::uint64_t base, std::unordered_set<std::uint64_t>&& ignore,
               float cell_size, std::size_t max_scan_count) {
        primitives_base = base;
        cursor = 0;
        max_scan = max_scan_count;
        active = base != 0 && max_scan_count > 0;
        complete = !active;
        last_step = 0.0;
        consecutive_null_slots = 0;
        found_non_null_slot = false;
        building_cache = occluder_cache{};
        building_cache.cell_size = cell_size;
        ignore_primitives = std::move(ignore);
        if (active)
            building_cache.parts.reserve((std::min)(max_scan_count, std::size_t{ 131072 }));
    }
};
