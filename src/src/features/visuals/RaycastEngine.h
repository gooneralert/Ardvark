#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include "core/player/PlayerHandler.h"
#include "core/roblox/math/Math.h"

namespace Cheat {
    namespace Features {
        namespace RaycastEngine {
            struct Result {
                bool  visible{ true };
                float coverage{ 1.0f };
            };

            bool IsEnabled();
            // raycast wallcheck или mesh occluded chams
            bool WantsCache();
            void Reset();
            void Tick();
            Result IsPlayerVisible(const PlayerCache& player, const Vector3& camera_pos);

            // ближайшие окклюдеры (AABB) для world-depth pre-pass
            void VisitOccluders(
                const Vector3& camera,
                float max_dist,
                std::size_t max_count,
                const std::function<void(const Vector3& pos, const Matrix4x4& rot, const Vector3& size)>& fn);
        }
    }
}
