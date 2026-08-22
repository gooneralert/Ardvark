#pragma once
#include <cstdint>

namespace Cheat {
namespace Features {

// Triggerbot — полный порт gelato triggerbot (src/core/cheat/features/combat/triggerbot).
// Триггерит клик когда парта под курсором попадает в прицел, с delay/click_duration
// и стейт-машиной arm->click_down->click_up.
class Triggerbot {
public:
    // каждый кадр из Menu::Render (рядом с Aim::Render)
    static void Render();

    // есть ли залоченная цель (для GUI-статуса "Target under crosshair")
    static bool HasTarget();

    // сброс стейта (клик ап) — на дестрой/смену карты
    static void Reset();
};

} // namespace Features
} // namespace Cheat