#pragma once
#include "core/roblox/classes/Classes.h"
#include "core/memory/Memory.h"
#include <cstdint>
#include <string>

#undef GetClassName

namespace Cheat {
namespace Features {
namespace Noclip {

// просто вырубаем collide на частях
inline void Apply(std::uint64_t character, bool enabled)
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

} // namespace Noclip
} // namespace Features
} // namespace Cheat
