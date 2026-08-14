#include "pch.h"
#include "MouseSilent.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"

#include <cstdint>

namespace Cheat {
namespace Features {
namespace MouseSilent {
namespace {

bool g_aiming = false;
std::uint64_t g_mouse_service = 0;

bool world_to_screen(const Matrix4x4& m, const Vector2& dim, const Vector3& p, Vector2& out)
{
    float w = p.x * m.m[3][0] + p.y * m.m[3][1] + p.z * m.m[3][2] + m.m[3][3];
    if (w < 0.01f)
    {
        return false;
    }

    float x = p.x * m.m[0][0] + p.y * m.m[0][1] + p.z * m.m[0][2] + m.m[0][3];
    float y = p.x * m.m[1][0] + p.y * m.m[1][1] + p.z * m.m[1][2] + m.m[1][3];
    float inv = 1.0f / w;

    out.x = (dim.x * 0.5f) + (x * inv * dim.x * 0.5f);
    out.y = (dim.y * 0.5f) - (y * inv * dim.y * 0.5f);
    return true;
}

std::uint64_t resolve_mouse()
{
    if (g_mouse_service && g_Memory.IsValid(g_mouse_service))
    {
        return g_mouse_service;
    }

    if (!Globals::InstanceDataModel.address ||
        !g_Memory.IsValid(Globals::InstanceDataModel.address))
    {
        return 0;
    }

    auto ms = Instance(Globals::InstanceDataModel.address).FindFirstChild("MouseService");
    if (!ms || !g_Memory.IsValid(ms->address))
    {
        return 0;
    }

    g_mouse_service = ms->address;
    return g_mouse_service;
}

// InputObject + MousePosition, сначала 2 потом 1
bool write_mouse_pos(std::uint64_t mouse_service, float x, float y)
{
    auto try_input = [&](std::uintptr_t input_off) -> bool
    {
        std::uint64_t input = g_Memory.Read<std::uint64_t>(mouse_service + input_off);
        if (!input || input == (std::uint64_t)-1 || !g_Memory.IsValid(input))
        {
            return false;
        }

        float pos[2]{ x, y };
        return g_Memory.WriteRaw(
                   input + ::MouseService::MousePosition, pos, sizeof(pos)) == sizeof(pos);
    };

    if (try_input(::MouseService::InputObject2))
    {
        return true;
    }

    return try_input(::MouseService::InputObject);
}

bool resolve_view(Matrix4x4& out_view, Vector2& out_dims)
{
    if (!Globals::Workspace)
    {
        return false;
    }

    auto cam_ptr = Globals::Workspace->GetCurrentCamera();
    if (!cam_ptr || !g_Memory.IsValid(cam_ptr->address))
    {
        return false;
    }

    Camera cam(cam_ptr->address);
    out_dims = cam.GetViewportSize();
    if (out_dims.x < 1.f || out_dims.y < 1.f)
    {
        return false;
    }

    static uintptr_t base = 0;
    if (!base)
    {
        base = g_Memory.GetModuleBase();
    }

    if (!base)
    {
        return false;
    }

    uintptr_t ve = g_Memory.Read<uintptr_t>(base + ::VisualEngine::Pointer);
    if (!g_Memory.IsValid(ve))
    {
        return false;
    }

    out_view = g_Memory.Read<Matrix4x4>(ve + ::VisualEngine::ViewMatrix);
    return true;
}

} // namespace

void Restore()
{
    g_aiming = false;
}

void SetActive(bool on, const Vector3& world_target)
{
    if (!on)
    {
        Restore();
        return;
    }

    std::uint64_t ms = resolve_mouse();
    if (!ms)
    {
        return;
    }

    Matrix4x4 view{};
    Vector2 dims{};
    if (!resolve_view(view, dims))
    {
        return;
    }

    Vector2 target{};
    if (!world_to_screen(view, dims, world_target, target))
    {
        return;
    }

    if (!write_mouse_pos(ms, target.x, target.y))
    {
        g_mouse_service = 0; // сброс, на след кадре ищем заново
        return;
    }

    g_aiming = true;
}

bool Aiming()
{
    return g_aiming;
}

} // namespace MouseSilent
} // namespace Features
} // namespace Cheat
