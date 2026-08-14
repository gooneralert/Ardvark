#include "pch.h"
#include "LightingInvalidate.h"
#include "core/roblox/offsets/Offsets.h"

namespace Cheat {
	namespace Features {

		std::uint64_t LightingInvalidate::render_view()
		{
			static const uintptr_t base = g_Memory.GetModuleBase();
			if (!base)
				return 0;

			std::uint64_t ve = g_Memory.Read<std::uint64_t>(base + ::VisualEngine::Pointer);
			if (!g_Memory.IsValid(ve))
				return 0;

			std::uint64_t rv = g_Memory.Read<std::uint64_t>(ve + ::VisualEngine::RenderView);
			return g_Memory.IsValid(rv) ? rv : 0;
		}

		void LightingInvalidate::invalidate()
		{
			std::uint64_t rv = render_view();
			if (!rv)
				return;
			g_Memory.Write<std::uint8_t>(rv + ::RenderView::LightingValid, 0);
		}

	}
}
