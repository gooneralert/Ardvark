#pragma once
#include <cstddef>

namespace Cheat::Features::HitSoundsData {

struct Entry {
    const char* name;
    const unsigned char* data;
    std::size_t size;
};

int Count();
const Entry& At(int index);
const Entry* Entries();

}
