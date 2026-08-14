#include "pch.h"
#include "PhantomSilent.h"

#include "features/games/PhantomForces.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"

#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <thread>

namespace Cheat {
namespace Features {
namespace PhantomSilent {
namespace {

// --- как в сурсе ---
static bool ghascachedmatrix = false;
static float gcachedviewmatrix[16]{};

Vector3 CrossProduct(const Vector3& a, const Vector3& b)
{
    return a.Cross(b);
}

// ===== сурс — не менять =====
struct Matrix3x3 {
    float data[9]{};
};

// чуть чаще переписываем rotation (LookAt не трогаем)
static std::atomic<bool> g_writer_run{ false };
static std::atomic<bool> g_active{ false };
static std::atomic<std::uint64_t> g_cam_addr{ 0 };
static std::atomic<bool> g_cam_is_part{ false };
static Matrix3x3 g_last_matrix{};
static bool g_writer_started = false;

Matrix3x3 LookAtToMatrix(const Vector3& cameraPosition, const Vector3& targetPosition)
{
    Vector3 forward = (targetPosition - cameraPosition);
    forward.Normalize();
    Vector3 right = CrossProduct(Vector3(0.f, 1.f, 0.f), forward);
    right.Normalize();
    Vector3 up = CrossProduct(forward, right);
    Matrix3x3 lookAtMatrix{};
    lookAtMatrix.data[0] = -right.x;
    lookAtMatrix.data[1] = up.x;
    lookAtMatrix.data[2] = -forward.x;
    lookAtMatrix.data[3] = right.y;
    lookAtMatrix.data[4] = up.y;
    lookAtMatrix.data[5] = -forward.y;
    lookAtMatrix.data[6] = -right.z;
    lookAtMatrix.data[7] = up.z;
    lookAtMatrix.data[8] = -forward.z;
    return lookAtMatrix;
}

std::uint64_t PartPrimitive(std::uint64_t part)
{
    if (!g_Memory.IsValid(part))
        return 0;
    return g_Memory.Read<std::uint64_t>(part + ::BasePart::Primitive);
}

Vector3 GetPartPos(std::uint64_t part)
{
    const std::uint64_t prim = PartPrimitive(part);
    if (!g_Memory.IsValid(prim))
        return {};
    return g_Memory.Read<Vector3>(prim + ::Primitive::Position);
}

void SetRotation(std::uint64_t part_or_cam, const Matrix3x3& m, bool is_part)
{
    if (!g_Memory.IsValid(part_or_cam))
        return;

    if (is_part) {
        const std::uint64_t prim = PartPrimitive(part_or_cam);
        if (!g_Memory.IsValid(prim))
            return;
        g_Memory.WriteRaw(prim + ::Primitive::Rotation, m.data, sizeof(m.data));
    } else {
        g_Memory.WriteRaw(part_or_cam + ::Camera::Rotation, m.data, sizeof(m.data));
    }
}

void EnsureWriter()
{
    if (g_writer_started)
        return;
    g_writer_started = true;
    g_writer_run = true;
    std::thread([] {
        while (g_writer_run) {
            if (g_active) {
                const std::uint64_t addr = g_cam_addr.load();
                const bool is_part = g_cam_is_part.load();
                const Matrix3x3 m = g_last_matrix;
                if (g_Memory.IsValid(addr)) {
                    SetRotation(addr, m, is_part);
                    // в момент клика игра чаще сносит rotation — дожимаем
                    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
                        SetRotation(addr, m, is_part);
                }
            }
            // idle 10ms; на ЛКМ чуть плотнее
            Sleep((g_active && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) ? 1 : 10);
        }
    }).detach();
}

void run(const Vector3& targetpos)
{
    static uintptr_t s_base = 0;
    if (!s_base)
        s_base = g_Memory.GetModuleBase();
    if (!s_base)
        return;

    const std::uint64_t ve = g_Memory.Read<std::uint64_t>(s_base + ::VisualEngine::Pointer);
    if (!g_Memory.IsValid(ve))
        return;

    if (!ghascachedmatrix) {
        g_Memory.ReadRaw(ve + ::VisualEngine::ViewMatrix, gcachedviewmatrix, sizeof(gcachedviewmatrix));
        ghascachedmatrix = true;
    }
    (void)gcachedviewmatrix;

    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return;

    // FindFirstChildOfClass("Camera")
    std::uint64_t cameraparent = 0;
    for (const auto& ch : Globals::Workspace->GetChildren()) {
        if (ch.GetClassName() == "Camera") {
            cameraparent = ch.address;
            break;
        }
    }
    if (!g_Memory.IsValid(cameraparent)) {
        cameraparent = g_Memory.Read<std::uint64_t>(
            Globals::Workspace->address + ::Workspace::CurrentCamera);
    }
    if (!g_Memory.IsValid(cameraparent))
        return;

    // camera = cameraparent.FindFirstChild("Part"); if 0 → cameraparent
    std::uint64_t camera = 0;
    bool camera_is_part = false;
    for (const auto& ch : Instance(cameraparent).GetChildren()) {
        if (ch.GetName() == "Part") {
            camera = ch.address;
            camera_is_part = true;
            break;
        }
    }
    if (!g_Memory.IsValid(camera)) {
        camera = cameraparent;
        camera_is_part = false;
    }
    if (!g_Memory.IsValid(camera))
        return;

    Vector3 pos = camera_is_part
        ? GetPartPos(camera)
        : g_Memory.Read<Vector3>(camera + ::Camera::Position);

    Matrix3x3 targetmatrix = LookAtToMatrix(pos, targetpos);
    targetmatrix = Matrix3x3{ {
        targetmatrix.data[0],
        targetmatrix.data[1],
        targetmatrix.data[2],
        targetmatrix.data[3],
        0.01f,
        targetmatrix.data[5],
        targetmatrix.data[6],
        targetmatrix.data[7],
        targetmatrix.data[8],
    } };
    SetRotation(camera, targetmatrix, camera_is_part);

    g_last_matrix = targetmatrix;
    g_cam_addr = camera;
    g_cam_is_part = camera_is_part;
}
// ===== конец сурса =====

} // namespace

void SetActive(bool on, const Vector3& world_target)
{
    if (!on) {
        g_active = false;
        return;
    }
    if (!Games::PhantomForces::IsActivePlace()) {
        g_active = false;
        return;
    }
    if (world_target.LengthSquared() < 1e-6f)
        return;

    EnsureWriter();
    g_active = true;
    run(world_target);
}

} // namespace PhantomSilent
} // namespace Features
} // namespace Cheat
