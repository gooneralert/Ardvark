#pragma once

#include "app/Settings.h"
#include "imgui.h"

#include <utility>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace PreviewAimDots {

inline ImU32 TierColor(int tier, int alpha)
{
	if (tier == Settings::PART_PRIMARY)
		return IM_COL32(90, 160, 255, alpha);
	if (tier == Settings::PART_SECONDARY)
		return IM_COL32(100, 220, 130, alpha);
	if (tier == Settings::PART_TERTIARY)
		return IM_COL32(245, 210, 80, alpha);
	return IM_COL32(0, 0, 0, 0);
}

inline void DrawFade(ImDrawList* dl, const ImVec2& c, int tier, bool hover)
{
	if (tier == Settings::PART_OFF) return;

	const float glow = hover ? 11.0f : 9.0f;
	const float core = hover ? 2.4f : 2.0f;
	const int steps = 14;
	for (int i = steps; i >= 1; --i) {
		const float t = (float)i / (float)steps;
		const float r = core + (glow - core) * t;
		const float falloff = (1.0f - t) * (1.0f - t);
		const int a = (int)((hover ? 42.0f : 28.0f) * falloff);
		if (a <= 0) continue;
		dl->AddCircleFilled(c, r, TierColor(tier, a), 32);
	}
	dl->AddCircleFilled(c, core, TierColor(tier, hover ? 160 : 110), 24);
	dl->AddCircleFilled(c, core * 0.45f, TierColor(tier, hover ? 210 : 150), 16);
}

// uv centers -> screen, ближайший part в радиусе
inline int HitTest(const ImVec2& mouse, float hit_r,
	const std::vector<std::pair<int, std::pair<float, float>>>& centers,
	const ImVec2& origin, float img_w, float img_h)
{
	int best = -1;
	float best_d2 = hit_r * hit_r;
	for (const auto& entry : centers)
	{
		int part = entry.first;
		if (part < 0 || part >= Settings::AIM_PART_COUNT)
			continue;
		ImVec2 c(
			origin.x + entry.second.first * img_w,
			origin.y + entry.second.second * img_h);
		float dx = mouse.x - c.x;
		float dy = mouse.y - c.y;
		float d2 = dx * dx + dy * dy;
		if (d2 < best_d2)
		{
			best_d2 = d2;
			best = part;
		}
	}
	return best;
}

} // namespace PreviewAimDots
} // namespace Visuals
} // namespace Cheat

namespace PreviewAimDots = Cheat::Visuals::PreviewAimDots;
