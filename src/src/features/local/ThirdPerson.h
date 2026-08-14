#pragma once

namespace Cheat {
namespace Features {

class ThirdPerson {
public:
    static void Tick();
    static void Restore();
    static bool NeedsTick();
};

}
}
