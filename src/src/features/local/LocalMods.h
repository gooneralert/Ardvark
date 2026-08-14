#pragma once
#include <cstdint>

namespace Cheat {
namespace Features {

class LocalMods {
public:
    static void Noclip(std::uint64_t character, bool enabled);
    static void InfiniteJump(std::uint64_t humanoid, bool enabled);
};

}
}
