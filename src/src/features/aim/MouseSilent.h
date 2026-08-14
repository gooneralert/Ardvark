#pragma once
#include "core/roblox/math/Math.h"
#include <cstdint>

namespace Cheat {
namespace Features {
namespace MouseSilent {

    // пишем экранную позу в InputObject
    void SetActive(bool on, const Vector3& world_target = {});
    void Restore();
    bool Aiming();

} // namespace MouseSilent
} // namespace Features
} // namespace Cheat
