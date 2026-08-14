#pragma once
#include <cstdint>
#include "core/roblox/math/Math.h"
#include "imgui.h"

namespace Cheat {
    namespace Visuals {
        namespace HavocWorldEsp {
            constexpr std::uint64_t kPlaceId = 16530963934ull;
            constexpr float kStudToMeter = 0.28f;
            constexpr float kMaxMeters = 400.f;

            bool IsActivePlace();

            // havoc: всегда метры, кап 400m (люди)
            inline float StudsToMeters(float studs)
            {
                return studs * kStudToMeter;
            }

            inline float MaxRangeStuds()
            {
                return kMaxMeters / kStudToMeter;
            }

            // true = слишком далеко (только на havoc place)
            bool BeyondRange(float dist_studs);

            void Render(ImDrawList* draw_list, ImFont* font, float font_size,
                        const Matrix4x4& view, const Vector2& viewport,
                        const Vector3& cam_pos, float overlay_w, float overlay_h,
                        float scale_x, float scale_y);
        }
    }
}
