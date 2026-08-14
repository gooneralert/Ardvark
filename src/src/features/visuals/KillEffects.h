#pragma once

#include <cstdint>

namespace Cheat {
    namespace Visuals {
        namespace KillEffects {

            enum Effect : int {
                PixelBurst = 0,
                Confetti,
                Shatter,
                EffectCount
            };

            const char* const* EffectNames();
            inline int EffectNameCount() { return EffectCount; }

            const char* const* HitDataModeNames();
            inline int HitDataModeNameCount() { return 5; }

            void Tick();

            // hit chams fade 0 = нет, иначе 1..0
            void NotifyHitChams(std::uint64_t player_addr);
            float HitChamsFade(std::uint64_t player_addr);

        }
    }
}
