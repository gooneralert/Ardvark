#pragma once

namespace Cheat {
namespace Visuals {
namespace Crosshair {

    void Render();
    void NotifyInactive(); // роблокс ушёл из фокуса, курсор назад, тред не трогаем
    void Shutdown();       // полный стоп + виндовый курсор назад

} // namespace Crosshair
} // namespace Visuals
} // namespace Cheat
