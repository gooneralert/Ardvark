#pragma once
#include <cstdint>
#include "core/roblox/math/Math.h"

namespace Cheat {
    namespace Features {
        class Aim {
        public:
            // тик аима, ветки методов внутри
            static void Render();

            static std::uint64_t CurrentTarget();
            static int CurrentAimPart();
            static Vector3 CurrentAimPoint();
        };
    }
}
