#pragma once

#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/math/Math.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"

// Курсор для всего, что «целится» мышью:
// - GameWindow()/Cursor() — точный порт gelato _input_window/_input_cursor
//   (src/core/cheat/features/combat/aimbot/helpers.h): GetCursorPos +
//   ScreenToClient на окне WINDOWSCLIENT.
// - AimCursor() — слоистый курсор (один источник для аимбота, триггербота,
//   трейсеров и т.д.):
//     1) мышь игры из памяти (MouseService::MousePosition — порт gelato
//        c_mouse_service::get_mouse_position): в first person / mouse-lock
//        игра пинит её в центре вьюпорта, поэтому она не дрейфует за скрытым
//        системным курсором;
//     2) first person → центр вьюпорта (прицел всегда в центре);
//     3) фолбэк — OS-курсор, как в gelato.

namespace Cheat {
namespace GameCursor {

// как gelato _input_window: найти WINDOWSCLIENT по пиду роблокса, кэшировать
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

// как gelato _input_cursor: позиция курсора в клиентских координатах WINDOWSCLIENT
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

// мышь игры из памяти — порт gelato c_mouse_service::get_mouse_position
inline bool GameMouse(float& out_x, float& out_y)
{
    static std::uint64_t s_mouse_service = 0;

    if (!s_mouse_service || !g_Memory.IsValid(s_mouse_service))
    {
        s_mouse_service = 0;

        if (!Globals::InstanceDataModel.address ||
            !g_Memory.IsValid(Globals::InstanceDataModel.address))
            return false;

        auto ms = Instance(Globals::InstanceDataModel.address).FindFirstChild("MouseService");
        if (!ms || !g_Memory.IsValid(ms->address))
            return false;

        s_mouse_service = ms->address;
    }

    auto pos = g_Memory.Read<Vector2>(s_mouse_service + ::MouseService::MousePosition);
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y))
        return false;

    out_x = pos.x;
    out_y = pos.y;
    return true;
}

// позиция HumanoidRootPart локального персонажа (для first-person эвристики)
inline bool LocalRootPosition(Vector3& out)
{
    if (!Globals::Players || !g_Memory.IsValid(Globals::Players->address))
        return false;

    auto lp = g_Memory.Read<std::uint64_t>(Globals::Players->address + ::Player::LocalPlayer);
    if (!lp || !g_Memory.IsValid(lp))
        return false;

    auto model = g_Memory.Read<std::uint64_t>(lp + ::Player::ModelInstance);
    if (!model || !g_Memory.IsValid(model))
        return false;

    auto hrp = Instance(model).FindFirstChild("HumanoidRootPart");
    if (!hrp || !g_Memory.IsValid(hrp->address))
        return false;

    out = BasePart(hrp->address).GetPosition();
    return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
}

// слоистый курсор: мышь игры → центр вьюпорта в first person → OS-курсор
inline bool AimCursor(float& out_x, float& out_y)
{
    // камера + вьюпорт
    Camera cam{ 0 };
    float vw = 0.0f, vh = 0.0f;
    if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
    {
        if (auto cam_inst = Globals::Workspace->GetCurrentCamera())
        {
            if (g_Memory.IsValid(cam_inst->address))
            {
                cam = Camera(cam_inst->address);
                Vector2 vp = cam.GetViewportSize();
                if (std::isfinite(vp.x) && std::isfinite(vp.y) &&
                    vp.x > 1.0f && vp.y > 1.0f)
                {
                    vw = vp.x;
                    vh = vp.y;
                }
            }
        }
    }

    // 1) мышь игры из памяти (в first person она запинена в центр)
    float gx = 0.0f, gy = 0.0f;
    if (GameMouse(gx, gy) &&
        (vw <= 0.0f || (gx >= -8.0f && gy >= -8.0f &&
                        gx <= vw + 8.0f && gy <= vh + 8.0f)))
    {
        out_x = gx;
        out_y = gy;
        return true;
    }

    // 2) first person: камера у головы локального персонажа → прицел в центре
    if (vw > 0.0f)
    {
        Vector3 cam_pos = cam.GetPosition();
        Vector3 hrp{};
        if (LocalRootPosition(hrp) && (cam_pos - hrp).Length() < 2.0f)
        {
            out_x = vw * 0.5f;
            out_y = vh * 0.5f;
            return true;
        }
    }

    // 3) фолбэк — gelato _input_cursor
    return Cursor(out_x, out_y);
}

} // namespace GameCursor
} // namespace Cheat