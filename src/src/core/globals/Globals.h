#pragma once
#include <cstdint>
#include "core/roblox/classes/Classes.h"

namespace Cheat
{
    /* globals */
    namespace Globals
    {
        inline uintptr_t ClientBase{ 0 };
        inline uintptr_t FrontDataModel{ 0 };

        inline Cheat::DataModel InstanceDataModel{};
        inline std::shared_ptr<Cheat::Workspace> Workspace{};
        inline std::shared_ptr<Cheat::Players> Players{};
    }
}
