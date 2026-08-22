#include "pch.h"
#include "EngineChamsStyles.h"
#include "EngineChamsApply.h"

#include "core/roblox/offsets/Offsets.h"
#include "../EngineChamsOffsets.h"

#include <cstdint>

namespace Cheat {
namespace Visuals {
namespace EngineChams {
namespace detail {

// charm: Param = idx+1, ColorData white
std::uint32_t ColorParam(int idx)
{
	if (idx < 0)
	{
		idx = 0;
	}

	if (idx > 6)
	{
		idx = 6;
	}

	return (std::uint32_t)(idx + 1);
}

std::uint32_t StyleQueue(int style)
{
	namespace RQ = ChamsOffsets::RenderQueue;

	if (style == 5)
	{
		return RQ::Glass;
	}

	if (style == 6)
	{
		return RQ::GlassTint;
	}

	if (style == 7)
	{
		return RQ::Transparent;
	}

	if (style == 8)
	{
		return RQ::OnTopWithDepth;
	}

	// 0 default / 1-4 old / 9 hologram — AlwaysOnTop
	return RQ::AlwaysOnTop;
}

// charm material_set + queue styles из decrypt
//   ghost     fill=0 flags2=0  param=color
//   wireframe fill=1 flags2=0  param=white
//   mesh      fill=0 flags2=15 param=color
//   charwire  fill=1 flags2=7  param=color
//   glass/glaze/smoke/depth/hologram — см StyleQueue
bool ApplyStyleLayers(uintptr_t ent, int style, int color_idx)
{
	const std::uint32_t color_param = ColorParam(color_idx);

	if (style == 1)
	{
		ApplyLayers(ent, 0, color_param, 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 2)
	{
		ApplyLayers(ent, 1, ColorParam(6), 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 3)
	{
		ApplyLayers(ent, 0, color_param, 15u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 4)
	{
		ApplyLayers(ent, 1, color_param, 7u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 5)
	{
		// glass
		ApplyLayers(ent, 0, color_param, 15u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 6)
	{
		// glaze
		ApplyLayers(ent, 0, color_param, 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 7)
	{
		// smoke
		ApplyLayers(ent, 0, color_param, 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 8)
	{
		// depth
		ApplyLayers(ent, 0, color_param, 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 9)
	{
		// hologram
		ApplyLayers(ent, 1, color_param, 15u, 0xFFFFFFFFu);
		return true;
	}

	return false;
}

} // namespace detail
} // namespace EngineChams
} // namespace Visuals
} // namespace Cheat
