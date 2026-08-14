#pragma once

namespace Cheat {
    namespace Features {
        class Explorer {
        public:
            static void Initialize();
            static void Shutdown();
            static void Render(float alpha = 1.0f);
        };
    }
}
