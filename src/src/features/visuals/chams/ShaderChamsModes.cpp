#include "pch.h"
#include "ShaderChamsInternal.h"

namespace Cheat {
namespace Visuals {
namespace ShaderChams {
namespace detail {

static const char* k_names[] = {
	"plasma", "ember", "toxic", "ice", "void",
	"neon", "gold", "matrix", "aurora", "blood",
	"ocean", "hologram", "sunset", "ghost", "chromatic",
};
static_assert(sizeof(k_names) / sizeof(k_names[0]) == StyleCount, "style names");

ImU32 MulAlpha(ImU32 c, float a)
{
	int na = (int)((float)((c >> IM_COL32_A_SHIFT) & 0xFF) * a + 0.5f);
	if (na < 0) na = 0;
	if (na > 255) na = 255;
	return (c & ~IM_COL32_A_MASK) | ((ImU32)na << IM_COL32_A_SHIFT);
}

ImU32 LerpCol(ImU32 a, ImU32 b, float t)
{
	if (t < 0.f) t = 0.f;
	if (t > 1.f) t = 1.f;

	ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
	ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
	return ImGui::ColorConvertFloat4ToU32(ImVec4(
		ca.x + (cb.x - ca.x) * t,
		ca.y + (cb.y - ca.y) * t,
		ca.z + (cb.z - ca.z) * t,
		ca.w + (cb.w - ca.w) * t));
}

ImU32 HSVA(float h, float s, float v, float a)
{
	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(std::fmod(h, 1.0f), s, v, r, g, b);
	return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

float Hash11(float n)
{
	const float s = std::sin(n * 127.1f) * 43758.5453f;
	return s - std::floor(s);
}

Palette AimPalette()
{
	return {
		IM_COL32(255, 90, 90, 130),
		IM_COL32(140, 20, 30, 100),
		IM_COL32(255, 180, 180, 150),
		IM_COL32(255, 230, 230, 200),
		IM_COL32(255, 70, 70, 235),
	};
}

// цвета/аним под каждый стиль
StyleDef DefFor(int style)
{
	switch (style)
	{
	case Ember:     return { AnimKind::Rise,        1.55f, 0.14f, 0.70f, {
		IM_COL32(255,170,60,130), IM_COL32(160,30,10,105),
		IM_COL32(255,220,120,160), IM_COL32(255,250,200,220), IM_COL32(255,140,50,235) }};
	case Toxic:     return { AnimKind::Rain,        1.25f, 0.10f, 0.50f, {
		IM_COL32(160,255,70,120), IM_COL32(20,90,20,100),
		IM_COL32(200,255,120,150), IM_COL32(230,255,180,210), IM_COL32(140,255,80,230) }};
	case Ice:       return { AnimKind::Ripple,      1.10f, 0.08f, 0.85f, {
		IM_COL32(220,245,255,140), IM_COL32(80,140,220,95),
		IM_COL32(200,240,255,150), IM_COL32(255,255,255,210), IM_COL32(180,230,255,235) }};
	case Void:      return { AnimKind::Flicker,     2.40f, 0.20f, 0.45f, {
		IM_COL32(160,80,255,120), IM_COL32(20,8,40,110),
		IM_COL32(200,140,255,140), IM_COL32(240,200,255,190), IM_COL32(180,100,255,230) }};
	case Neon:      return { AnimKind::Cross,       1.70f, 0.12f, 0.60f, {
		IM_COL32(255,60,200,125), IM_COL32(40,220,255,105),
		IM_COL32(255,180,255,160), IM_COL32(255,255,255,220), IM_COL32(255,80,220,235) }};
	case Gold:      return { AnimKind::Pulse,       0.95f, 0.20f, 0.80f, {
		IM_COL32(255,220,100,135), IM_COL32(140,80,20,105),
		IM_COL32(255,240,160,150), IM_COL32(255,255,220,210), IM_COL32(255,200,80,235) }};
	case Matrix:    return { AnimKind::Stripes,     2.10f, 0.045f, 0.35f, {
		IM_COL32(40,220,80,100), IM_COL32(5,40,10,95),
		IM_COL32(80,255,120,170), IM_COL32(180,255,180,220), IM_COL32(60,255,100,230) }};
	case Aurora:    return { AnimKind::Wave,        0.85f, 0.18f, 0.70f, {
		IM_COL32(80,255,190,120), IM_COL32(180,80,255,105),
		IM_COL32(160,255,230,150), IM_COL32(230,255,255,200), IM_COL32(140,255,210,230) }};
	case Blood:     return { AnimKind::Glitch,      1.80f, 0.10f, 0.50f, {
		IM_COL32(220,40,50,130), IM_COL32(60,5,10,110),
		IM_COL32(255,90,90,150), IM_COL32(255,180,180,200), IM_COL32(220,40,50,235) }};
	case Ocean:     return { AnimKind::ScanX,       0.90f, 0.24f, 0.65f, {
		IM_COL32(60,200,255,120), IM_COL32(10,40,120,105),
		IM_COL32(120,230,255,145), IM_COL32(200,250,255,210), IM_COL32(80,210,255,230) }};
	case Hologram:  return { AnimKind::DualScan,    1.55f, 0.09f, 0.80f, {
		IM_COL32(100,230,255,90), IM_COL32(40,80,160,80),
		IM_COL32(180,255,255,130), IM_COL32(255,255,255,190), IM_COL32(120,240,255,220) }};
	case Sunset:    return { AnimKind::ScanY,       0.70f, 0.30f, 0.75f, {
		IM_COL32(255,160,60,125), IM_COL32(200,40,120,105),
		IM_COL32(255,200,140,145), IM_COL32(255,240,200,205), IM_COL32(255,140,80,230) }};
	case Ghost:     return { AnimKind::Sparkle,     1.30f, 0.08f, 0.90f, {
		IM_COL32(240,245,255,95), IM_COL32(140,150,180,65),
		IM_COL32(255,255,255,140), IM_COL32(255,255,255,200), IM_COL32(220,230,255,200) }};
	case Chromatic: return { AnimKind::RainbowFlow, 0.65f, 0.16f, 0.65f, {
		IM_COL32(255,80,180,120), IM_COL32(80,120,255,105),
		IM_COL32(255,255,255,140), IM_COL32(255,255,255,210), IM_COL32(255,255,255,230) }};
	case Plasma:
	default:        return { AnimKind::Diagonal,    1.20f, 0.18f, 0.65f, {
		IM_COL32(110,235,255,125), IM_COL32(70,55,210,100),
		IM_COL32(190,255,255,145), IM_COL32(240,255,255,210), IM_COL32(150,245,255,230) }};
	}
}

Palette PaletteFromOverride(const float* c)
{
	int r = (int)(c[0] * 255.f);
	int g = (int)(c[1] * 255.f);
	int b = (int)(c[2] * 255.f);
	float a = c[3];
	if (a < 0.f) a = 0.f;
	if (a > 1.f) a = 1.f;

	int rd = (int)(r * 0.40f);
	int gd = (int)(g * 0.40f);
	int bd = (int)(b * 0.40f);
	int rh = r + 50;
	int gh = g + 50;
	int bh = b + 50;
	if (rh > 255) rh = 255;
	if (gh > 255) gh = 255;
	if (bh > 255) bh = 255;

	return {
		IM_COL32(r, g, b, (int)(a * 140.f)),
		IM_COL32(rd, gd, bd, (int)(a * 105.f)),
		IM_COL32(rh, gh, bh, (int)(a * 165.f)),
		IM_COL32(rh, gh, bh, (int)(a * 220.f)),
		IM_COL32(r, g, b, (int)(a * 235.f)),
	};
}

}

const char* const* StyleNames() { return detail::k_names; }

}
}
}
