#include "pch.h"
#include "NoShadow.h"
#include "LightingInvalidate.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

namespace Cheat {
	namespace Features {

		bool NoShadow::Apply(std::uint64_t lighting)
		{
			// ::Lighting::GlobalShadows was removed — no-shadow disabled.
			(void)lighting;
			return false;
		}

	}
}
