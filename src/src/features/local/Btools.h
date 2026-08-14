#pragma once

namespace Cheat {
namespace Features {

// Btools — injects a fake Hammer/Grab/Clone tool into the Workspace
// MouseCommand pipeline (shared_ptr slots). Port of the C# FoulzExternal
// implementation, using offsets from ManualOffsets::Btools + geeg Offsets.h.
class Btools {
public:
    static void Tick();
    static void Shutdown();

private:
    Btools() = delete;
};

} // namespace Features
} // namespace Cheat