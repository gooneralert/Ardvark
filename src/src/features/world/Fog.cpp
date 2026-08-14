#include "pch.h"
#include "Fog.h"
#include "LightingInvalidate.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

namespace Cheat {
	namespace Features {

		bool Fog::Apply(std::uint64_t lighting)
		{
			const auto& w = Cheat::g_Settings.world;
			bool dirty = false;

			if (w.fog)
			{
				float start = w.fog_start;
				if (start < 0.f) start = 0.f;
				if (start > 100.f) start = 100.f;

				float end = w.fog_end;
				if (end > 2000.f) end = 2000.f;
				if (end < start + 1.f) end = start + 1.f;

				dirty |= LightingInvalidate::write_if_changed<float>(lighting + ::Lighting::FogStart, start);
				dirty |= LightingInvalidate::write_if_changed<float>(lighting + ::Lighting::FogEnd, end);
				dirty |= LightingInvalidate::write_if_changed<Color3>(lighting + ::Lighting::FogColor,
					Color3(w.fog_color[0], w.fog_color[1], w.fog_color[2]));
			}

			return dirty;
		}

	}
}
