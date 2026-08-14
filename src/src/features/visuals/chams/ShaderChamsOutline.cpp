#include "pch.h"
#include "ShaderChamsInternal.h"

namespace Cheat {
namespace Visuals {
namespace ShaderChams {

ImU32 OutlineColor(int style, bool aim_highlight, const float* color_override)
{
	if (color_override)
		return detail::PaletteFromOverride(color_override).outline;

	if (aim_highlight)
		return detail::AimPalette().outline;

	if (style < 0) style = 0;
	if (style > StyleCount - 1) style = StyleCount - 1;

	if (style == Chromatic)
		return detail::HSVA(std::fmod((float)ImGui::GetTime() * 0.35f, 1.0f), 0.85f, 1.0f, 0.90f);

	return detail::DefFor(style).pal.outline;
}

}
}
}
