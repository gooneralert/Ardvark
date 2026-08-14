#pragma once
#include "core/roblox/math/Math.h"
#include <cstdint>

namespace Cheat {
    namespace Features {

        // свой хук как RaycastSilent, wallbang всегда
        namespace MagicBullet {

            bool Install();
            void Remove();
            void Ensure(bool want = true);

            void SetActive(bool on, const Vector3& world_target = {});

            bool Ready();
            bool Aiming();

        }
    }
}
