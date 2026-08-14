#include "pch.h"
#include "animation.h"
#include <cmath>

namespace animation {

    float ease_expo(float time, float delay, float duration)
    {
        if (duration <= 0.f)
            return 1.f;
        if (time <= delay)
            return 0.f;

        float t = (time - delay) / duration;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        if (t >= 1.f)
            return 1.f;

        // expo out, классика
        return 1.f - std::pow(2.f, -10.f * t);
    }

    float ease_cubic_out(float progress)
    {
        if (progress < 0.f) progress = 0.f;
        if (progress > 1.f) progress = 1.f;
        return 1.f - std::pow(1.f - progress, 3.f);
    }

    float ease_cubic_in(float progress)
    {
        if (progress < 0.f) progress = 0.f;
        if (progress > 1.f) progress = 1.f;
        return progress * progress * progress;
    }

}
