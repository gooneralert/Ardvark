#pragma once

namespace Cheat::Features::HitSounds {

    const char* const* Names();
    int Count();
    const char* FileName(int index);

    void Play(int index);
    void PlayCurrent();

}
