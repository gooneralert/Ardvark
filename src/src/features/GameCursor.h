#pragma once

#include <windows.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/math/Math.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"

// 1:1 порт курсора gelato (src/core/cheat/features/combat/aimbot/helpers.h):
//   _input_window - find WINDOWSCLIENT by the roblox pid, cached
//   _input_cursor - GetCursorPos + ScreenToClient(WINDOWSCLIENT)
// gelato reads ONLY the OS cursor for everything that aims with the mouse.
//
// ONE deliberate deviation (the only one): in first person Roblox pins the
// physical cursor to the viewport center ONLY when the game actually locks the
// mouse (MouseBehavior = LockCenter). Many games do first person through custom
// camera scripts with the cursor left free and hidden - there the raw OS cursor
// drifts anywhere and is useless as an aim reference. Detection is physical
// (camera within 2 studs of the local head), so it works no matter HOW the game
// implements first person: use the viewport center as the crosshair, which is
// exactly what the player sees. Everything else is pure gelato _input_cursor.

namespace Cheat {
namespace GameCursor {

// like gelato _input_window: find WINDOWSCLIENT by the roblox pid, cached
inline HWND GameWindow()
{
    static HWND cached = nullptr;
    if (cached && IsWindow(cached))
        return cached;

    const DWORD pid = g_Memory.GetPID();
    if (!pid)
        return nullptr;

    struct finder_t { DWORD pid; HWND result; };
    finder_t finder{ pid, nullptr };

    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        auto* self = reinterpret_cast<finder_t*>(param);

        DWORD window_pid = 0;
        GetWindowThreadProcessId(hwnd, &window_pid);
        if (window_pid != self->pid || !IsWindowVisible(hwnd))
            return TRUE;

        char class_name[256]{};
        GetClassNameA(hwnd, class_name, sizeof(class_name));
        if (std::strcmp(class_name, "WINDOWSCLIENT") != 0)
            return TRUE;

        self->result = hwnd;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&finder));

    cached = finder.result;
    return cached;
}

// like gelato _input_cursor: cursor position in WINDOWSCLIENT client coords
inline bool Cursor(float& out_x, float& out_y)
{
    POINT point{};
    HWND hwnd = GameWindow();
    if (!hwnd || !GetCursorPos(&point) || !ScreenToClient(hwnd, &point))
        return false;

    out_x = static_cast<float>(point.x);
    out_y = static_cast<float>(point.y);
    return true;
}

// local HumanoidRootPart address, refreshed at most twice a second
inline std::uint64_t LocalRoot()
{
    static std::uint64_t s_hrp = 0;
    static std::chrono::steady_clock::time_point s_next{};

    const auto now = std::chrono::steady_clock::now();
    if (s_hrp && g_Memory.IsValid(s_hrp) && now < s_next)
        return s_hrp;

    s_hrp = 0;
    s_next = now + std::chrono::milliseconds(500);

    if (!Globals::Players || !g_Memory.IsValid(Globals::Players->address))
        return 0;

    auto lp = g_Memory.Read<std::uint64_t>(Globals::Players->address + ::Player::LocalPlayer);
    if (!lp || !g_Memory.IsValid(lp))
        return 0;

    auto model = g_Memory.Read<std::uint64_t>(lp + ::Player::ModelInstance);
    if (!model || !g_Memory.IsValid(model))
        return 0;

    auto hrp = Instance(model).FindFirstChild("HumanoidRootPart");
    if (!hrp || !g_Memory.IsValid(hrp->address))
        return 0;

    s_hrp = hrp->address;
    return s_hrp;
}

// gelato _input_cursor + first-person correction:
// camera sits at the local head -> the crosshair IS the viewport center
inline bool AimCursor(float& out_x, float& out_y)
{
    if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
    {
        if (auto cam_inst = Globals::Workspace->GetCurrentCamera())
        {
            if (g_Memory.IsValid(cam_inst->address))
            {
                Camera cam{ cam_inst->address };

                Vector2 vp = cam.GetViewportSize();
                if (std::isfinite(vp.x) && std::isfinite(vp.y) &&
                    vp.x > 1.0f && vp.y > 1.0f)
                {
                    if (std::uint64_t hrp = LocalRoot())
                    {
                        Vector3 head = BasePart(hrp).GetPosition();
                        Vector3 cam_pos = cam.GetPosition();
                        if (std::isfinite(head.x) && std::isfinite(head.y) &&
                            std::isfinite(head.z) &&
                            (cam_pos - head).Length() < 2.0f)
                        {
                            out_x = vp.x * 0.5f;
                            out_y = vp.y * 0.5f;
                            return true;
                        }
                    }
                }
            }
        }
    }

    return Cursor(out_x, out_y);
}

} // namespace GameCursor
} // namespace Cheat