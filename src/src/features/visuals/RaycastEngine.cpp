#include "pch.h"
#include "RaycastEngine.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

#undef GetClassName

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <climits>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Cheat::Features::RaycastEngine {
namespace {

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

double now_sec() {
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

Vector3 multiply_transpose(const float3x3& m, const Vector3& v) {
    return Vector3(
        m._11 * v.x + m._12 * v.y + m._13 * v.z,
        m._21 * v.x + m._22 * v.y + m._23 * v.z,
        m._31 * v.x + m._32 * v.y + m._33 * v.z);
}

float component(const Vector3& v, int axis)
{
    if (axis == 0)
        return v.x;

    if (axis == 1)
        return v.y;

    return v.z;
}

float3x3 identity_rotation()
{
    float3x3 r{};
    r._11 = r._22 = r._33 = 1.0f;
    return r;
}

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

// луч vs obb (сначала сфера, дешёвый отсев)
bool ray_intersects_obb(const Vector3& origin, const Vector3& dir,
                        const occluder_part& part, float max_distance, float* out_hit)
{
    Vector3 to_part = part.position - origin;
    float radius_sq = part.radius_sq;
    if (radius_sq < 0.f)
        radius_sq = 0.f;
    float sphere_radius = std::sqrt(radius_sq);
    float dist_sq = to_part.LengthSquared();
    float combined = sphere_radius + max_distance;
    if (dist_sq > combined * combined)
        return false;

    Vector3 local_origin = multiply_transpose(part.rotation, origin - part.position);
    Vector3 local_dir = multiply_transpose(part.rotation, dir);
    Vector3 half = part.half_size;

    float tmin = -(std::numeric_limits<float>::max)();
    float tmax = (std::numeric_limits<float>::max)();

    for (int axis = 0; axis < 3; ++axis)
    {
        float o = component(local_origin, axis);
        float d = component(local_dir, axis);
        float mn = -component(half, axis);
        float mx = component(half, axis);

        if (std::fabs(d) < 1e-6f)
        {
            if (o < mn || o > mx)
                return false;

            continue;
        }

        float t1 = (mn - o) / d;
        float t2 = (mx - o) / d;
        if (t1 > t2)
            std::swap(t1, t2);

        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax || tmax < 0.0f || tmin > max_distance)
            return false;
    }

    float hit = tmax;
    if (tmin >= 0.0f)
        hit = tmin;

    if (hit < 0.0f || hit > max_distance || tmin > tmax)
        return false;

    if (out_hit)
        *out_hit = hit;

    return true;
}

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

} // namespace

bool IsEnabled() {
    return g_Settings.misc.raycast_engine;
}

bool WantsCache()
{
    if (g_Settings.misc.raycast_engine)
        return true;
    return g_Settings.esp.chams &&
           g_Settings.esp.chams_mode == 4 &&
           g_Settings.esp.mesh_chams_occlusion;
}

void Reset() {
    cache_storage().store(std::make_shared<occluder_cache>(), std::memory_order_release);
    std::lock_guard<std::mutex> lock(builder_mutex());
    builder_storage().reset();
}

// подкачиваем окклюдеры потихоньку
void Tick() {
    if (!WantsCache())
        return;
    (void)get_occluder_cache();
}

void VisitOccluders(
    const Vector3& camera,
    float max_dist,
    std::size_t max_count,
    const std::function<void(const Vector3& pos, const Matrix4x4& rot, const Vector3& size)>& fn)
{
    if (!fn || max_count == 0 || max_dist <= 0.f)
        return;

    auto cache = get_occluder_cache();
    if (!cache || cache->parts.empty())
        return;

    const float max_dsq = max_dist * max_dist;
    struct Cand {
        float dsq;
        std::uint32_t idx;
    };
    std::vector<Cand> cands;
    cands.reserve((std::min)(max_count * 2, cache->parts.size()));

    for (std::uint32_t i = 0; i < (std::uint32_t)cache->parts.size(); ++i)
    {
        const auto& p = cache->parts[i];
        const float dx = p.position.x - camera.x;
        const float dy = p.position.y - camera.y;
        const float dz = p.position.z - camera.z;
        const float dsq = dx * dx + dy * dy + dz * dz;
        if (dsq > max_dsq)
            continue;
        cands.push_back({ dsq, i });
    }

    if (cands.empty())
        return;

    if (cands.size() > max_count)
    {
        std::nth_element(cands.begin(), cands.begin() + (std::ptrdiff_t)max_count, cands.end(),
                         [](const Cand& a, const Cand& b) { return a.dsq < b.dsq; });
        cands.resize(max_count);
    }

    for (const Cand& c : cands)
    {
        const auto& p = cache->parts[c.idx];
        Matrix4x4 rot(
            p.rotation._11, p.rotation._12, p.rotation._13, 0.f,
            p.rotation._21, p.rotation._22, p.rotation._23, 0.f,
            p.rotation._31, p.rotation._32, p.rotation._33, 0.f,
            0.f, 0.f, 0.f, 1.f);
        Vector3 size{
            p.half_size.x * 2.f,
            p.half_size.y * 2.f,
            p.half_size.z * 2.f
        };
        fn(p.position, rot, size);
    }
}

// видно ли игрока, сверху ещё сглаживание чтоб не дёргалось
Result IsPlayerVisible(const PlayerCache& player, const Vector3& camera_pos)
{
    if (!IsEnabled())
        return Result{};

    if (!std::isfinite(camera_pos.x) || !std::isfinite(camera_pos.y) || !std::isfinite(camera_pos.z))
        return Result{};

    // no occluders in the world yet -> nothing can hide the player; bail early
    // so we never run the heavy OBB raycast against an empty/half-built cache.
    auto occluders = get_occluder_cache();
    if (!occluders || occluders->parts.empty() || !occluders->complete)
        return Result{};

    struct smoothed_state
    {
        float smoothed = 1.0f;
        bool visible = true;
        double last_time = 0.0;
        double last_query_time = 0.0;
        int visible_confirmation = 0;
        int occluded_confirmation = 0;
        Result last_result{};
    };
    thread_local std::unordered_map<std::uint64_t, smoothed_state> smooth_map;

    smoothed_state& state = smooth_map[player.address];
    double now = now_sec();

    // не долбим каждый кадр
    if (state.last_query_time != 0.0 && (now - state.last_query_time) < 0.015)
        return state.last_result;

    float coverage = 1.0f;
    bool visible_raw = is_visible_impl(player, camera_pos, *occluders, coverage);

    float alpha = 0.28f;
    if (state.last_time == 0.0)
        alpha = 1.0f;

    else if (visible_raw)
        alpha = 0.4f;

    state.smoothed = state.smoothed * (1.0f - alpha) + coverage * alpha;

    if (visible_raw)
    {
        state.visible_confirmation = state.visible_confirmation + 1;
        if (state.visible_confirmation > 16)
            state.visible_confirmation = 16;
        state.occluded_confirmation = 0;
        if (state.smoothed < coverage)
            state.smoothed = coverage;
    }

    else
    {
        state.occluded_confirmation = state.occluded_confirmation + 1;
        if (state.occluded_confirmation > 16)
            state.occluded_confirmation = 16;
        state.visible_confirmation = 0;
    }

    // гистерезис, on/off пороги разные
    if (state.visible)
    {
        if (!visible_raw && state.smoothed <= 0.22f && state.occluded_confirmation >= 4)
            state.visible = false;
    }

    else
    {
        if ((state.smoothed >= 0.48f && state.visible_confirmation >= 2) ||
            state.visible_confirmation >= 3)
            state.visible = true;
    }

    if (state.last_time == 0.0)
    {
        state.visible = visible_raw;
        state.smoothed = coverage;
        state.visible_confirmation = visible_raw ? 1 : 0;
        state.occluded_confirmation = visible_raw ? 0 : 1;
    }

    state.last_time = now;
    state.last_query_time = now;
    state.last_result = Result{ state.visible, state.smoothed };
    return state.last_result;
}

} // namespace Cheat::Features::RaycastEngine
