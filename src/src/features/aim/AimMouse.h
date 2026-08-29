#pragma once
#include <cstdint>
#include "app/Settings.h"
#include "core/roblox/math/Math.h"

namespace Cheat {
    namespace Features {
        // Точный порт mouse-ветки аимбота из booted-off-gelato:
        //   src/core/cheat/features/combat/aimbot/aimbot.cxx  (mouse branch, on_worker_tick @240Hz)
        //   src/core/cheat/features/combat/aimbot/helpers.h   (_input_cursor/_input_move_mouse)
        //   src/core/sdk/math/math.h                          (alpha_*, spring_update)
        // gelato тикает аимбот в СВОЁМ потоке на 240Hz — маленькие шаги дают
        // стабильную замкнутую петлю (камера не «фликает»). Рендер-поток только
        // публикует цель, вся математика движения крутится в worker-потоке.
        namespace AimMouse {
            // рендер-поток: опубликовать цель (экранная точка) на этот кадр
            void Publish(const Settings::AimbotConfig& cfg, const Vector2& best_screen,
                         const Vector2& viewport, bool first_person);

            // цель потеряна / ключ отпущен — сброс лока (аналог gelato clear_target)
            void Reset();

            // единый курсор аима: мышь игры из памяти (MouseService::MousePosition,
            // порт gelato c_mouse_service::get_mouse_position), фолбэк — OS-курсор
            // (gelato _input_cursor: GetCursorPos + ScreenToClient на WINDOWSCLIENT)
            bool CursorPos(float& out_x, float& out_y);
        }
    }
}