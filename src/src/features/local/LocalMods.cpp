#include "pch.h"
#include "core/roblox/classes/Classes.h"
#include "LocalMods.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include <Windows.h>

#undef GetClassName

namespace Cheat {
    namespace Features {

        // просто вырубаем collide на частях
        void LocalMods::Noclip(std::uint64_t character, bool enabled)
        {
			if (!enabled || !g_Memory.IsValid(character))
				return;

			Cheat::Instance ch(character);
			for (const auto& part : ch.GetChildren())
			{
				std::string cls = part.GetClassName();
				if (cls == "Part" || cls == "MeshPart" || cls == "UnionOperation" ||
					cls == "BasePart" || cls == "TrussPart" || cls == "WedgePart" ||
					cls == "CornerWedgePart")
				{
					Cheat::BasePart(part.address).SetCanCollide(false);
				}
			}
        }

        // space жмёт Jump=true, только edge
        void LocalMods::InfiniteJump(std::uint64_t humanoid, bool enabled)
        {
			if (!enabled || !g_Memory.IsValid(humanoid))
				return;

			static bool was_down = false;
			bool down = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
			if (down && !was_down)
				g_Memory.Write<bool>(humanoid + ::Humanoid::Jump, true);
			was_down = down;
        }

    }
}
