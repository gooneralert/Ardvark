#pragma once
#include "ShaderChams.h"
#include "imgui_internal.h"
#include <cmath>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace ShaderChams {
namespace detail {

enum class AnimKind : int {
	ScanY = 0,
	ScanX,
	Cross,
	DualScan,
	Pulse,
	Flicker,
	Stripes,
	Rain,
	Wave,
	Rise,
	Ripple,
	Glitch,
	Sparkle,
	Diagonal,
	RainbowFlow,
};

struct Palette {
	ImU32 top, bot, band, band_core, outline;
};

struct StyleDef {
	AnimKind anim;
	float speed;
	float band_frac;
	float fresnel;
	Palette pal;
};

ImU32 MulAlpha(ImU32 c, float a);
ImU32 LerpCol(ImU32 a, ImU32 b, float t);
ImU32 HSVA(float h, float s, float v, float a);
float Hash11(float n);

Palette AimPalette();
StyleDef DefFor(int style);
Palette PaletteFromOverride(const float* c);

void Bounds(const std::vector<ImVec2>& poly, ImVec2& mn, ImVec2& mx);

}
}
}
}
