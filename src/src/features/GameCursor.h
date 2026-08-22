#pragma once

#include <windows.h>
#include <cstring>

#include "core/memory/Memory.h"

// Точный порт gelato _input_window/_input_cursor (src/core/cheat/features/combat/aimbot/helpers.h).
// gelato читает курсор ВСЕГДА через GetCursorPos + ScreenToClient на окне WINDOWSCLIENT:
// так положение совпадает с вьюпортом роблокса даже когда мышь залочена (first-person
// mouse-lock держит системный курсор в центре WINDOWSCLIENT).
// Используется триггерботом и трейсерами (origin = mouse) — везде один источник.

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

} // namespace GameCursor
} // namespace Cheat