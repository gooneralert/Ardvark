#pragma once

#include <cstdint>
#include "core/roblox/math/Math.h"

namespace Cheat {
namespace Features {
namespace HitboxExpander {

void Tick();
void Render();
void Shutdown(); // рестор всех size

// для ESP: оригинал если мы раздули, иначе current
Vector3 SizeForEsp(std::uint64_t part_addr, const Vector3& current);

}
}
}
