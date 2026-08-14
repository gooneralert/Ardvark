#include "pch.h"
#include "EspTracer.h"

namespace Cheat {
namespace Visuals {
namespace EspTracer {

ImVec2 TracerOrigin(float overlay_w, float overlay_h, int mode)
{
	// 0 bottom / 1 center / 2 mouse / 3 top
	if (mode == 1)
		return ImVec2(overlay_w * 0.5f, overlay_h * 0.5f);

	if (mode == 2)
	{
		ImVec2 m = ImGui::GetIO().MousePos;
		return ImVec2(m.x, m.y);
	}

	if (mode == 3)
		return ImVec2(overlay_w * 0.5f, 0.f);

	return ImVec2(overlay_w * 0.5f, overlay_h);
}

void DrawTracer(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 color)
{
	dl->AddLine(from, to, IM_COL32(0, 0, 0, 200), 2.5f);
	dl->AddLine(from, to, color, 1.0f);
}

} // namespace EspTracer
} // namespace Visuals
} // namespace Cheat
