#include "pch.h"
#include "Fly.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "renderer/Renderer.h"
#include "app/Settings.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>

#undef GetClassName

namespace {

using namespace Cheat;

// Ported verbatim from charm-main's movement module:
//   * `fly`                     (movement.cpp, ~line 3973)
//   * `get_camera_fly_basis`    (movement.cpp, ~line 1153)
//   * `fly_direction_from_keys` (movement.cpp, ~line 1225)
//   * `write_velocity_stable`   (movement.cpp, ~line 1181)
//
// Velocity-based flight: while the bind is engaged the workspace gravity is
// hidden and the local HumanoidRootPart primitive is pushed along the camera
// basis scaled by (max(0.1, fly_speed) * kMoveSpeedMultiplier).

constexpr float kMoveSpeedMultiplier = 2.5f;   // charm kMovementSpeedMultiplier
constexpr float kFallbackGravity     = 192.2f; // fallback workspace gravity

std::atomic<bool> g_run{ false };
std::thread       g_th;

bool key_down(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// key modes: 0 = hold, 1 = toggle, 2 = always (mirrors the fly UI / Speed)
bool key_gate(int key, int mode, bool& tog, bool& was)
{
    if (mode == 2 || key == 0)
        return true;

    const bool down = key_down(key);
    if (mode == 1)
    {
        if (down && !was)
            tog = !tog;
        was = down;
        return tog;
    }

    was = down;
    return down;
}

// charm workspace.SetWorkspaceGravity() pointer chain (Workspace::World).
std::uint64_t world_instance()
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return 0;
    return g_Memory.Read<std::uint64_t>(Globals::Workspace->address + ::Workspace::World);
}

// charm: Players().GetLocalPlayer().GetCharacter().FindFirstChild("HumanoidRootPart")
// resolved down to its primitive (SetPartVelocity writes to the primitive).
std::uint64_t local_root_primitive()
{
    if (!Globals::Players || !g_Memory.IsValid(Globals::Players->address))
        return 0;

    const std::uint64_t lp = g_Memory.Read<std::uint64_t>(Globals::Players->address + ::Players::LocalPlayer);
    if (!g_Memory.IsValid(lp))
        return 0;

    const std::uint64_t character = g_Memory.Read<std::uint64_t>(lp + ::Player::ModelInstance);
    if (!g_Memory.IsValid(character))
        return 0;

    std::uint64_t hrp = 0;
    auto hum = Cheat::Instance(character).FindFirstChild("Humanoid");
    if (hum && g_Memory.IsValid(hum->address))
        hrp = Cheat::Humanoid(hum->address).GetRootPartAddress();

    if (!g_Memory.IsValid(hrp))
    {
        auto root = Cheat::Instance(character).FindFirstChild("HumanoidRootPart");
        if (root)
            hrp = root->address;
    }

    if (!g_Memory.IsValid(hrp))
        return 0;

    return g_Memory.Read<std::uint64_t>(hrp + ::BasePart::Primitive);
}

Vector3 normalize_or(Vector3 v, const Vector3& fallback)
{
    const float len_sq = v.LengthSquared();
    if (len_sq > 1e-6f && std::isfinite(len_sq))
        return v.Normalized();
    return fallback;
}

// charm get_camera_fly_basis(): columns of the camera's 3x3 rotation.
void camera_fly_basis(std::uint64_t cam, Vector3& forward, Vector3& right, Vector3& up)
{
    if (!g_Memory.IsValid(cam))
    {
        forward = { 0.0f, 0.0f, -1.0f };
        right   = { 1.0f, 0.0f, 0.0f };
        up      = { 0.0f, 1.0f, 0.0f };
        return;
    }

    float rot[9]{};
    g_Memory.ReadRaw(cam + ::Camera::Rotation, rot, sizeof(rot));

    right   = normalize_or(Vector3(rot[0], rot[3], rot[6]), Vector3(1.0f, 0.0f, 0.0f));
    up      = normalize_or(Vector3(rot[1], rot[4], rot[7]), Vector3(0.0f, 1.0f, 0.0f));
    forward = normalize_or(Vector3(rot[2], rot[5], rot[8]), Vector3(0.0f, 0.0f, 1.0f));
}

// charm fly_direction_from_keys() — W/S/A/D + Space/Ctrl/Shift.
Vector3 fly_direction_from_keys(const Vector3& forward, const Vector3& right, const Vector3& up)
{
    Vector3 dir(0.0f, 0.0f, 0.0f);

    if (key_down('W')) dir = dir - forward;
    if (key_down('S')) dir = dir + forward;
    if (key_down('A')) dir = dir - right;
    if (key_down('D')) dir = dir + right;
    if (key_down(VK_SPACE)) dir = dir + up;
    if (key_down(VK_CONTROL) || key_down(VK_LSHIFT) || key_down(VK_SHIFT))
        dir = dir - up;

    return dir;
}

// Writes the velocity as a short, dense burst (no ~1 ms wall-clock throttle).
// The previous hammer blocked the loop for up to 1 ms, so a direction change
// wasn't sampled until that hammer finished — that was the last bit of delay
// felt when not in shiftlock. A fixed burst returns in microseconds, so the
// main loop re-reads keys/camera almost immediately, while still writing far
// more often than a physics tick so the direction can't get overridden.
void write_velocity(std::uint64_t primitive, const Vector3& velocity)
{
    if (!g_Memory.IsValid(primitive))
        return;

    const std::uint64_t addr = primitive + ::Primitive::AssemblyLinearVelocity;
    for (int i = 0; i < 256; ++i)
    {
        // The character can be freed/destroyed mid-burst (respawn, R15 swap).
        // Re-check each write so we never keep hammering freed memory, which
        // corrupts Roblox's heap and crashes the game a few minutes later.
        if (!g_Memory.IsValid(primitive))
            break;
        g_Memory.Write<Vector3>(addr, velocity);
    }
}

void restore_gravity_robust(bool& overridden, float original)
{
    if (!overridden)
        return;

    const std::uint64_t world = world_instance();
    if (!world)
    {
        overridden = false;
        return;
    }

    // A single write sometimes doesn't stick (the game keeps gravity at 0),
    // so write the original value back three times, ~0.1s apart.
    for (int i = 0; i < 3; ++i)
    {
        g_Memory.Write<float>(world + ::World::Gravity, original);
        if (i < 2)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    overridden = false;
}

void fly_loop()
{
    bool  gravity_overridden = false;
    float original_gravity   = kFallbackGravity;
    bool  fly_active         = false;
    bool  fly_was            = false;

    while (g_run.load(std::memory_order_relaxed))
    {
        const auto& s = g_Settings.misc;

        if (!s.fly)
        {
            restore_gravity_robust(gravity_overridden, original_gravity);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        const bool engaged = key_gate(s.fly_key, s.fly_key_mode, fly_active, fly_was);
        if (!engaged)
        {
            restore_gravity_robust(gravity_overridden, original_gravity);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const std::uint64_t primitive = local_root_primitive();
        if (!primitive)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const std::uint64_t world = world_instance();
        if (s.fly_gravity)
        {
            // toggle ON: zero the workspace gravity while engaged (charm behaviour)
            if (!gravity_overridden && world)
            {
                original_gravity   = g_Memory.Read<float>(world + ::World::Gravity);
                g_Memory.Write<float>(world + ::World::Gravity, 0.0f);
                gravity_overridden = true;
            }
        }
        else
        {
            // toggle OFF: leave gravity completely untouched
            restore_gravity_robust(gravity_overridden, original_gravity);
        }

        std::uint64_t cam = 0;
        if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
            cam = g_Memory.Read<std::uint64_t>(Globals::Workspace->address + ::Workspace::CurrentCamera);

        Vector3 forward, right, up;
        camera_fly_basis(cam, forward, right, up);

        const Vector3 direction = fly_direction_from_keys(forward, right, up);

        if (direction.LengthSquared() > 1e-6f)
        {
            const Vector3 norm   = direction.Normalized();
            const float   speed  = (s.fly_speed > 0.1f ? s.fly_speed : 0.1f) * kMoveSpeedMultiplier;
            write_velocity(primitive,
                Vector3(norm.x * speed, norm.y * speed, norm.z * speed));
        }
        else
        {
            write_velocity(primitive, Vector3(0.0f, 0.0f, 0.0f));
        }

        // No sleep here: keep writing the velocity continuously so the
        // humanoid/physics never get a gap in which to override the direction
        // (this is what caused the "delayed direction" feel out of shiftlock).
        // write_velocity() already hammers for ~1ms per pass, so the loop still
        // cycles at its natural minimum rate without a dead band.
        std::this_thread::yield();
    }

    restore_gravity_robust(gravity_overridden, original_gravity);
}

} // namespace

void Cheat::Features::Fly::Start()
{
    if (g_run.load())
        return;
    g_run.store(true);
    g_th = std::thread(fly_loop);
}

void Cheat::Features::Fly::Stop()
{
    g_run.store(false);
    if (g_th.joinable())
        g_th.join();
}