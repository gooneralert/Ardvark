#include "pch.h"
#include "ShaderChams.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace Cheat {
namespace Visuals {
namespace ShaderChams {
namespace {

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

void Bounds(const std::vector<ImVec2>& poly, ImVec2& mn, ImVec2& mx)
{
	mn = mx = poly[0];
	for (const auto& p : poly)
	{
		if (p.x < mn.x) mn.x = p.x;
		if (p.y < mn.y) mn.y = p.y;
		if (p.x > mx.x) mx.x = p.x;
		if (p.y > mx.y) mx.y = p.y;
	}
}

template<typename Side, typename Intersect>
std::vector<ImVec2> ClipHalf(const std::vector<ImVec2>& in, Side side, Intersect isect)
{
	std::vector<ImVec2> out;
	int n = (int)in.size();
	if (n < 3)
		return out;

	for (int i = 0; i < n; ++i)
	{
		const ImVec2& cur  = in[i];
		const ImVec2& prev = in[(i + n - 1) % n];
		bool cin = side(cur)  >= 0.0f;
		bool pin = side(prev) >= 0.0f;

		if (cin)
		{
			if (!pin)
				out.push_back(isect(prev, cur));
			out.push_back(cur);
		}

		else if (pin)
		{
			out.push_back(isect(prev, cur));
		}
	}
	return out;
}

std::vector<ImVec2> ClipYBand(const std::vector<ImVec2>& poly, float y0, float y1)
{
	if (y1 < y0)
		std::swap(y0, y1);

	auto lo = ClipHalf(poly,
		[y0](const ImVec2& p) { return p.y - y0; },
		[y0](const ImVec2& a, const ImVec2& b)
		{
			float t = (y0 - a.y) / (b.y - a.y + 1e-6f);
			return ImVec2(a.x + (b.x - a.x) * t, y0);
		});
	return ClipHalf(lo,
		[y1](const ImVec2& p) { return y1 - p.y; },
		[y1](const ImVec2& a, const ImVec2& b)
		{
			float t = (y1 - a.y) / (b.y - a.y + 1e-6f);
			return ImVec2(a.x + (b.x - a.x) * t, y1);
		});
}

std::vector<ImVec2> ClipXBand(const std::vector<ImVec2>& poly, float x0, float x1)
{
	if (x1 < x0)
		std::swap(x0, x1);

	auto lo = ClipHalf(poly,
		[x0](const ImVec2& p) { return p.x - x0; },
		[x0](const ImVec2& a, const ImVec2& b)
		{
			float t = (x0 - a.x) / (b.x - a.x + 1e-6f);
			return ImVec2(x0, a.y + (b.y - a.y) * t);
		});
	return ClipHalf(lo,
		[x1](const ImVec2& p) { return x1 - p.x; },
		[x1](const ImVec2& a, const ImVec2& b)
		{
			float t = (x1 - a.x) / (b.x - a.x + 1e-6f);
			return ImVec2(x1, a.y + (b.y - a.y) * t);
		});
}

std::vector<ImVec2> ClipDiagBand(const std::vector<ImVec2>& poly, float d0, float d1)
{
	if (d1 < d0)
		std::swap(d0, d1);

	auto lo = ClipHalf(poly,
		[d0](const ImVec2& p) { return (p.x + p.y) - d0; },
		[d0](const ImVec2& a, const ImVec2& b)
		{
			float da = a.x + a.y;
			float db = b.x + b.y;
			float t = (d0 - da) / (db - da + 1e-6f);
			return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
		});
	return ClipHalf(lo,
		[d1](const ImVec2& p) { return d1 - (p.x + p.y); },
		[d1](const ImVec2& a, const ImVec2& b)
		{
			float da = a.x + a.y;
			float db = b.x + b.y;
			float t = (d1 - da) / (db - da + 1e-6f);
			return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
		});
}

void FillGradient(ImDrawList* dl, const std::vector<ImVec2>& poly,
                  ImU32 top, ImU32 bot, bool horizontal) {
    if (poly.size() < 3) return;
    ImVec2 mn, mx;
    Bounds(poly, mn, mx);
    const int vtx0 = dl->VtxBuffer.Size;
    dl->AddConvexPolyFilled(poly.data(), (int)poly.size(), top);
    const int vtx1 = dl->VtxBuffer.Size;
    if (vtx1 > vtx0) {
        const ImVec2 p0(mn.x, mn.y);
        const ImVec2 p1 = horizontal ? ImVec2(mx.x, mn.y) : ImVec2(mn.x, mx.y);
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, vtx0, vtx1, p0, p1, top, bot);
    }
}

void FillAll(ImDrawList* dl, const std::vector<std::vector<ImVec2>>& pieces,
             ImU32 top, ImU32 bot, bool horizontal) {
    for (const auto& piece : pieces) {
        if (piece.size() < 3) continue;
        FillGradient(dl, piece, top, bot, horizontal);
    }
}

void PaintY(ImDrawList* dl, const std::vector<std::vector<ImVec2>>& pieces,
            float y0, float y1, ImU32 col) {
    for (const auto& piece : pieces) {
        if (piece.size() < 3) continue;
        auto s = ClipYBand(piece, y0, y1);
        if (s.size() >= 3)
            dl->AddConvexPolyFilled(s.data(), (int)s.size(), col);
    }
}

void PaintX(ImDrawList* dl, const std::vector<std::vector<ImVec2>>& pieces,
            float x0, float x1, ImU32 col) {
    for (const auto& piece : pieces) {
        if (piece.size() < 3) continue;
        auto s = ClipXBand(piece, x0, x1);
        if (s.size() >= 3)
            dl->AddConvexPolyFilled(s.data(), (int)s.size(), col);
    }
}

void PaintDiag(ImDrawList* dl, const std::vector<std::vector<ImVec2>>& pieces,
               float d0, float d1, ImU32 col) {
    for (const auto& piece : pieces) {
        if (piece.size() < 3) continue;
        auto s = ClipDiagBand(piece, d0, d1);
        if (s.size() >= 3)
            dl->AddConvexPolyFilled(s.data(), (int)s.size(), col);
    }
}

void PaintXYCell(ImDrawList* dl, const std::vector<std::vector<ImVec2>>& pieces,
                 float x0, float x1, float y0, float y1, ImU32 col) {
    for (const auto& piece : pieces) {
        if (piece.size() < 3) continue;
        auto s = ClipYBand(ClipXBand(piece, x0, x1), y0, y1);
        if (s.size() >= 3)
            dl->AddConvexPolyFilled(s.data(), (int)s.size(), col);
    }
}

// сами полоски/пульсы
void RunAnim(ImDrawList* dl,
             const std::vector<std::vector<ImVec2>>& pieces,
             const ImVec2& mn, const ImVec2& mx,
             float time, const StyleDef& def, const Palette& pal, float pulse_a)
{
	float w = mx.x - mn.x;
	float h = mx.y - mn.y;
	if (w < 1.f) w = 1.f;
	if (h < 1.f) h = 1.f;

	float phase = std::fmod(time * def.speed, 1.0f);
	ImU32 band = MulAlpha(pal.band, pulse_a);
	ImU32 core = MulAlpha(pal.band_core, pulse_a);

	float bf = def.band_frac;
	if (bf < 0.03f) bf = 0.03f;
	if (bf > 0.5f) bf = 0.5f;

    switch (def.anim) {
    case AnimKind::ScanY: {
        const float bh = h * bf;
        const float cy = mn.y + (h + bh) * phase - bh * 0.5f;
        PaintY(dl, pieces, cy - bh * 0.5f, cy + bh * 0.5f, band);
        PaintY(dl, pieces, cy - bh * 0.14f, cy + bh * 0.14f, core);
        break;
    }
    case AnimKind::ScanX: {

        const float bw = w * bf;
        const float cx = mn.x + (w + bw) * phase - bw * 0.5f;
        PaintX(dl, pieces, cx - bw * 0.5f, cx + bw * 0.5f, band);
        const float p2 = std::fmod(phase + 0.55f, 1.0f);
        const float cx2 = mn.x + (w + bw * 0.6f) * p2 - bw * 0.3f;
        PaintX(dl, pieces, cx2 - bw * 0.25f, cx2 + bw * 0.25f, MulAlpha(core, 0.7f));
        break;
    }
    case AnimKind::Cross: {
        const float bh = h * bf, bw = w * bf;
        const float cy = mn.y + (h + bh) * phase - bh * 0.5f;
        const float cx = mn.x + (w + bw) * std::fmod(phase * 1.35f, 1.0f) - bw * 0.5f;
        PaintY(dl, pieces, cy - bh * 0.5f, cy + bh * 0.5f, band);
        PaintX(dl, pieces, cx - bw * 0.5f, cx + bw * 0.5f, MulAlpha(band, 0.85f));
        PaintY(dl, pieces, cy - bh * 0.12f, cy + bh * 0.12f, core);
        PaintX(dl, pieces, cx - bw * 0.12f, cx + bw * 0.12f, core);
        break;
    }
    case AnimKind::DualScan: {
        const float bh = h * bf;
        const float c0 = mn.y + (h + bh) * phase - bh * 0.5f;
        const float c1 = mn.y + (h + bh) * std::fmod(phase + 0.5f, 1.0f) - bh * 0.5f;
        PaintY(dl, pieces, c0 - bh * 0.5f, c0 + bh * 0.5f, band);
        PaintY(dl, pieces, c1 - bh * 0.5f, c1 + bh * 0.5f, MulAlpha(band, 0.75f));
        PaintY(dl, pieces, c0 - bh * 0.15f, c0 + bh * 0.15f, core);

        if (Hash11(std::floor(time * 28.0f)) > 0.82f)
            PaintY(dl, pieces, mn.y, mx.y, MulAlpha(core, 0.12f));
        break;
    }
    case AnimKind::Pulse: {

        const float t = 0.5f + 0.5f * std::sin(time * def.speed * 6.2831853f);
        const float r = (0.15f + 0.85f * t) * (std::max)(w, h) * 0.55f;
        const float cy = (mn.y + mx.y) * 0.5f;
        PaintY(dl, pieces, cy - r, cy + r, MulAlpha(band, 0.35f + 0.35f * t));
        PaintY(dl, pieces, cy - r * 0.35f, cy + r * 0.35f, MulAlpha(core, 0.45f));
        break;
    }
    case AnimKind::Flicker: {
        const float gate = Hash11(std::floor(time * 18.0f * def.speed));
        if (gate > 0.35f) {
            const float a = 0.25f + gate * 0.75f;
            PaintY(dl, pieces, mn.y, mx.y, MulAlpha(band, a * 0.35f));
        }

        if (Hash11(std::floor(time * 11.0f) + 3.7f) > 0.72f) {
            const float yh = mn.y + h * Hash11(std::floor(time * 9.0f));
            PaintY(dl, pieces, yh, yh + h * 0.08f, core);
        }
        break;
    }
    case AnimKind::Stripes: {
        const float step = (std::max)(3.0f, h * bf);
        const float off = std::fmod(time * def.speed * h * 0.55f, step * 2.0f);
        for (float y = mn.y - step * 2.0f + off; y < mx.y + step; y += step * 2.0f)
            PaintY(dl, pieces, y, y + step * 0.55f, band);

        const float tick = mn.y + std::fmod(time * def.speed * h, h);
        PaintY(dl, pieces, tick, tick + step * 0.35f, core);
        break;
    }
    case AnimKind::Rain: {
        int cols = 7;
        float cw = w / (float)cols;
        for (int i = 0; i < cols; ++i) {
            const float seed = (float)i * 17.13f;
            const float p = std::fmod(phase + Hash11(seed), 1.0f);
            const float drop_h = h * (0.12f + 0.18f * Hash11(seed + 1.0f));
            const float cy = mn.y + (h + drop_h) * p - drop_h * 0.5f;
            const float x0 = mn.x + cw * (float)i + cw * 0.15f;
            const float x1 = x0 + cw * 0.55f;
            for (const auto& piece : pieces) {
                if (piece.size() < 3) continue;
                auto s = ClipYBand(ClipXBand(piece, x0, x1), cy - drop_h * 0.5f, cy + drop_h * 0.5f);
                if (s.size() >= 3)
                    dl->AddConvexPolyFilled(s.data(), (int)s.size(), band);
            }
        }
        break;
    }
    case AnimKind::Wave: {
        int segs = 8;
        float seg_w = w / (float)segs;
        float bh = h * bf;
        for (int i = 0; i < segs; ++i) {
            const float u = (float)i / (float)segs;
            const float wave = 0.5f + 0.5f * std::sin((u * 4.0f + time * def.speed) * 6.2831853f);
            const float cy = mn.y + h * (0.25f + 0.5f * wave);
            const float x0 = mn.x + seg_w * (float)i;
            const float x1 = x0 + seg_w + 1.0f;
            for (const auto& piece : pieces) {
                if (piece.size() < 3) continue;
                auto s = ClipYBand(ClipXBand(piece, x0, x1), cy - bh * 0.5f, cy + bh * 0.5f);
                if (s.size() >= 3)
                    dl->AddConvexPolyFilled(s.data(), (int)s.size(), band);
            }
        }
        break;
    }
    case AnimKind::Rise: {

        for (int i = 0; i < 4; ++i) {
            const float sp = 0.55f + 0.35f * (float)i;
            const float p = std::fmod(time * def.speed * sp + (float)i * 0.21f, 1.0f);
            const float bh = h * (bf * (0.7f + 0.2f * (float)i));

            const float cy = mx.y - (h + bh) * p + bh * 0.5f;
            const float a = 0.45f + 0.15f * (float)(3 - i);
            PaintY(dl, pieces, cy - bh * 0.5f, cy + bh * 0.5f, MulAlpha(band, a));
        }

        if (Hash11(std::floor(time * 22.0f)) > 0.55f)
            PaintY(dl, pieces, mn.y, mn.y + h * 0.12f, MulAlpha(core, 0.55f));
        break;
    }
    case AnimKind::Ripple: {
        const float cx = (mn.x + mx.x) * 0.5f;
        const float cy = (mn.y + mx.y) * 0.5f;
        const float max_r = 0.5f * std::sqrt(w * w + h * h);
        for (int ring = 0; ring < 3; ++ring) {
            const float p = std::fmod(phase + (float)ring * 0.33f, 1.0f);
            const float r = p * max_r;
            const float th = (std::max)(2.5f, max_r * bf);

            PaintY(dl, pieces, cy - r - th, cy - r + th, MulAlpha(band, 0.55f));
            PaintY(dl, pieces, cy + r - th, cy + r + th, MulAlpha(band, 0.55f));
            PaintX(dl, pieces, cx - r - th, cx - r + th, MulAlpha(band, 0.40f));
            PaintX(dl, pieces, cx + r - th, cx + r + th, MulAlpha(band, 0.40f));
        }
        PaintY(dl, pieces, cy - h * 0.04f, cy + h * 0.04f, MulAlpha(core, 0.5f));
        break;
    }
    case AnimKind::Glitch: {
        for (int i = 0; i < 5; ++i) {
            const float seed = std::floor(time * def.speed * 6.0f) + (float)i * 13.7f;
            if (Hash11(seed) < 0.45f) continue;
            const float yh = mn.y + h * Hash11(seed + 1.1f);
            const float hh = h * (0.03f + 0.10f * Hash11(seed + 2.2f));
            const float xshift = (Hash11(seed + 3.3f) - 0.5f) * w * 0.25f;

            PaintY(dl, pieces, yh, yh + hh, MulAlpha(band, 0.55f + 0.4f * Hash11(seed)));
            PaintX(dl, pieces, mn.x + xshift, mn.x + xshift + w * 0.35f, MulAlpha(core, 0.25f));
        }
        break;
    }
    case AnimKind::Sparkle: {
        int cells = 12;
        float cw = w / 4.0f;
        float ch = h / 5.0f;
        for (int i = 0; i < cells; ++i) {
            const float seed = (float)i * 9.17f + std::floor(time * def.speed * 5.0f);
            if (Hash11(seed) < 0.55f) continue;
            const float ux = Hash11(seed + 0.3f);
            const float uy = Hash11(seed + 0.7f);
            const float x0 = mn.x + ux * (w - cw);
            const float y0 = mn.y + uy * (h - ch);
            const float a = 0.35f + 0.65f * Hash11(seed + 1.4f);
            PaintXYCell(dl, pieces, x0, x0 + cw * 0.55f, y0, y0 + ch * 0.45f, MulAlpha(core, a));
        }

        const float breath = 0.5f + 0.5f * std::sin(time * def.speed * 3.5f);
        PaintY(dl, pieces, mn.y, mx.y, MulAlpha(band, 0.08f + 0.10f * breath));
        break;
    }
    case AnimKind::Diagonal:
    case AnimKind::RainbowFlow: {
        const float dmin = mn.x + mn.y;
        const float dmax = mx.x + mx.y;
        const float span = (std::max)(1.0f, dmax - dmin);
        const float bd = span * bf;
        const float cd = dmin + (span + bd) * phase - bd * 0.5f;
        PaintDiag(dl, pieces, cd - bd * 0.5f, cd + bd * 0.5f, band);
        PaintDiag(dl, pieces, cd - bd * 0.15f, cd + bd * 0.15f, core);
        if (def.anim == AnimKind::RainbowFlow) {

            const float p2 = std::fmod(phase + 0.4f, 1.0f);
            const float cy = mn.y + h * p2;
            PaintY(dl, pieces, cy - h * 0.04f, cy + h * 0.04f, MulAlpha(core, 0.55f));
        }
        break;
    }
    default: break;
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

const char* const* StyleNames() { return k_names; }

ImU32 OutlineColor(int style, bool aim_highlight, const float* color_override)
{
	if (color_override)
		return PaletteFromOverride(color_override).outline;

	if (aim_highlight)
		return AimPalette().outline;

	if (style < 0) style = 0;
	if (style > StyleCount - 1) style = StyleCount - 1;

	if (style == Chromatic)
		return HSVA(std::fmod((float)ImGui::GetTime() * 0.35f, 1.0f), 0.85f, 1.0f, 0.90f);

	return DefFor(style).pal.outline;
}

void Triangulate(const std::vector<ImVec2>& poly,
                 std::vector<std::vector<ImVec2>>& out_tris)
{
	out_tris.clear();
	int n0 = (int)poly.size();
	if (n0 < 3)
		return;

	if (n0 == 3)
	{
		out_tris.push_back(poly);
		return;
	}

	auto area2 = [](const ImVec2& a, const ImVec2& b, const ImVec2& c)
	{
		return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
	};
	auto point_in_tri = [&](const ImVec2& p, const ImVec2& a,
	                        const ImVec2& b, const ImVec2& c)
	{
		float a0 = area2(a, b, p);
		float a1 = area2(b, c, p);
		float a2 = area2(c, a, p);
		bool has_neg = (a0 < 0) || (a1 < 0) || (a2 < 0);
		bool has_pos = (a0 > 0) || (a1 > 0) || (a2 > 0);
		return !(has_neg && has_pos);
	};

	std::vector<int> idx((size_t)n0);
	for (int i = 0; i < n0; ++i)
		idx[i] = i;

	float signed_area = 0.0f;
	for (int i = 0; i < n0; ++i)
	{
		const ImVec2& a = poly[i];
		const ImVec2& b = poly[(i + 1) % n0];
		signed_area += a.x * b.y - b.x * a.y;
	}
	if (signed_area < 0.0f)
		std::reverse(idx.begin(), idx.end());

	out_tris.reserve((size_t)(n0 - 2));
	int guard = n0 * n0;
	while ((int)idx.size() > 3 && guard-- > 0)
	{
		int m = (int)idx.size();
		bool clipped = false;
		for (int i = 0; i < m; ++i)
		{
			int i0 = idx[(i + m - 1) % m];
			int i1 = idx[i];
			int i2 = idx[(i + 1) % m];
			const ImVec2& a = poly[i0];
			const ImVec2& b = poly[i1];
			const ImVec2& c = poly[i2];
			if (area2(a, b, c) <= 0.0f)
				continue;

			bool ear = true;
			for (int j = 0; j < m; ++j)
			{
				int ij = idx[j];
				if (ij == i0 || ij == i1 || ij == i2)
					continue;
				if (point_in_tri(poly[ij], a, b, c))
				{
					ear = false;
					break;
				}
			}
			if (!ear)
				continue;

			out_tris.push_back({ a, b, c });
			idx.erase(idx.begin() + i);
			clipped = true;
			break;
		}
		if (!clipped)
			break;
	}
	if (idx.size() == 3)
		out_tris.push_back({ poly[idx[0]], poly[idx[1]], poly[idx[2]] });
}

// филл полигонов шейдер-чамсами
void DrawFill(ImDrawList* draw_list,
              const std::vector<std::vector<ImVec2>>& pieces,
              float time,
              int style,
              bool aim_highlight,
              bool lite,
              const float* color_override)
{
	if (!draw_list || pieces.empty())
		return;

	if (style < 0) style = 0;
	if (style > StyleCount - 1) style = StyleCount - 1;

	bool have_bounds = false;
	ImVec2 body_mn{}, body_mx{};
	for (const auto& p : pieces)
	{
		if (p.size() < 3)
			continue;
		ImVec2 mn, mx;
		Bounds(p, mn, mx);
		if (!have_bounds)
		{
			body_mn = mn;
			body_mx = mx;
			have_bounds = true;
		}

		else
		{
			if (mn.x < body_mn.x) body_mn.x = mn.x;
			if (mn.y < body_mn.y) body_mn.y = mn.y;
			if (mx.x > body_mx.x) body_mx.x = mx.x;
			if (mx.y > body_mx.y) body_mx.y = mx.y;
		}
	}
	if (!have_bounds)
		return;

	StyleDef def = DefFor(style);
	(void)lite;

	Palette pal = def.pal;
	if (color_override)
		pal = PaletteFromOverride(color_override);

	else if (aim_highlight)
		pal = AimPalette();

	if (!color_override && !aim_highlight && style == Chromatic)
	{
		float h = std::fmod(time * def.speed * 0.35f, 1.0f);
		pal.top = HSVA(h, 0.85f, 1.0f, 0.48f);
		pal.bot = HSVA(h + 0.33f, 0.90f, 0.75f, 0.40f);
		pal.band = HSVA(h + 0.12f, 0.55f, 1.0f, 0.55f);
		pal.band_core = HSVA(h + 0.05f, 0.25f, 1.0f, 0.80f);
	}
	if (!color_override && !aim_highlight && style == Aurora)
	{
		float w = 0.5f + 0.5f * std::sin(time * 0.7f);
		pal.top = LerpCol(pal.top, IM_COL32(255, 120, 200, 120), w * 0.55f);
		pal.bot = LerpCol(pal.bot, IM_COL32(80, 200, 255, 105), (1.0f - w) * 0.55f);
	}
	if (!color_override && !aim_highlight && style == Gold)
	{
		float w = 0.5f + 0.5f * std::sin(time * def.speed * 2.0f);
		pal.top = LerpCol(pal.top, IM_COL32(255, 255, 200, 150), w * 0.4f);
		pal.bot = LerpCol(pal.bot, IM_COL32(180, 100, 20, 110), (1.0f - w) * 0.35f);
	}
	if (!color_override && !aim_highlight && style == Sunset)
	{
		float w = 0.5f + 0.5f * std::sin(time * 0.45f);
		pal.top = LerpCol(pal.top, IM_COL32(255, 80, 100, 125), w * 0.5f);
		pal.bot = LerpCol(pal.bot, IM_COL32(80, 20, 120, 105), (1.0f - w) * 0.45f);
	}

	float pulse_a = 1.0f;
	if (def.anim == AnimKind::Pulse || def.anim == AnimKind::Sparkle)
		pulse_a = 0.55f + 0.45f * (0.5f + 0.5f * std::sin(time * def.speed * 6.2831853f));

	else if (def.anim == AnimKind::Flicker)
		pulse_a = 0.40f + 0.60f * Hash11(std::floor(time * 16.0f));

	float fresnel = def.fresnel;
	if (fresnel < 0.f) fresnel = 0.f;
	if (fresnel > 1.f) fresnel = 1.f;

	ImU32 top = MulAlpha(pal.top, pulse_a * (0.55f + 0.45f * fresnel));
	ImU32 bot = MulAlpha(pal.bot, pulse_a);

    ImDrawListFlags backup = draw_list->Flags;
    draw_list->Flags &= ~ImDrawListFlags_AntiAliasedFill;

    const bool horiz = (def.anim == AnimKind::ScanX || def.anim == AnimKind::Wave);
    FillAll(draw_list, pieces, top, bot, horiz);
    RunAnim(draw_list, pieces, body_mn, body_mx, time, def, pal, pulse_a);

    draw_list->Flags = backup;
}

}
}
}
