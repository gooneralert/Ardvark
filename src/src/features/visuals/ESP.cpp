#include "pch.h"
#include "ESP.h"
#include "EspLayout.h"
#include "HavocWorldEsp.h"
#include "features/games/PhantomForces.h"
#include "ShaderChams.h"
#include "MeshChams.h"
#include "MeshDxShader.h"
#include "features/visuals/boxfill/BoxFill.h"
#include "features/aim/Aim.h"
#include "features/visuals/KillEffects.h"
#include "features/misc/HitboxExpander.h"
#include "core/globals/Globals.h"
#include "core/player/PlayerHandler.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/math/Math.h"
#include "imgui.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "app/Settings.h"
#include "renderer/Renderer.h"
#include "gui/resources/fonts/fonts.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cfloat>
#include <cstddef>

// havoc position attr (без lua-потока — иначе есп мигает)
static bool HavocReadPosAttr(std::uint64_t inst, Vector3& out)
{
#if 0 // HavocReadPosAttr disabled — Offsets::Instance::ComponentMap / Offsets::Attribute were removed
	if (!g_Memory.IsValid(inst))
		return false;

	const std::uint64_t cmap = g_Memory.Read<std::uint64_t>(
		inst + Offsets::Instance::ComponentMap);
	if (!g_Memory.IsValid(cmap))
		return false;

	const uintptr_t base = g_Memory.GetModuleBase();
	if (!base)
		return false;

	const std::uint16_t want = g_Memory.Read<std::uint16_t>(
		base + Offsets::Attribute::TypeIdRva);
	if (!want)
		return false;

	std::uint64_t amap = 0;
	const std::uint64_t b = g_Memory.Read<std::uint64_t>(cmap);
	const std::uint64_t e = g_Memory.Read<std::uint64_t>(cmap + 8);
	if (g_Memory.IsValid(b) && g_Memory.IsValid(e) && e >= b && (e - b) < 0x4000)
	{
		for (std::uint64_t slot = b; slot < e; slot += 16)
		{
			const std::uint16_t t = g_Memory.Read<std::uint16_t>(slot + 8);
			if (t != want)
				continue;
			const std::uint64_t ptr = g_Memory.Read<std::uint64_t>(slot);
			if (g_Memory.IsValid(ptr))
			{
				amap = ptr;
				break;
			}
		}
	}

	if (!amap)
	{
		const std::uint64_t page = g_Memory.Read<std::uint64_t>(cmap + 24);
		if (!g_Memory.IsValid(page))
			return false;
		const std::uint32_t n = g_Memory.Read<std::uint32_t>(page + 24);
		const std::uint64_t arr = g_Memory.Read<std::uint64_t>(page);
		if (!g_Memory.IsValid(arr) || n > 256)
			return false;
		for (std::uint32_t i = 0; i < n; ++i)
		{
			const std::uint64_t blk = g_Memory.Read<std::uint64_t>(arr + 8 * (i >> 2));
			if (!g_Memory.IsValid(blk))
				continue;
			const std::uint64_t ent = blk + 16ull * (i & 3);
			const std::uint16_t t = g_Memory.Read<std::uint16_t>(ent + 8);
			if (t != want)
				continue;
			const std::uint64_t ptr = g_Memory.Read<std::uint64_t>(ent);
			if (g_Memory.IsValid(ptr))
			{
				amap = ptr;
				break;
			}
		}
	}

	if (!amap)
		return false;

	const std::uint32_t cnt = g_Memory.Read<std::uint32_t>(
		amap + Offsets::AttributesMap::Length);
	const std::uint64_t ents = g_Memory.Read<std::uint64_t>(
		amap + Offsets::AttributesMap::Attributes);
	if (!g_Memory.IsValid(ents) || cnt == 0 || cnt > 256)
		return false;

	for (std::uint32_t i = 0; i < cnt; ++i)
	{
		const std::uint64_t ent = ents + Offsets::Attribute::Size * i;
		const std::uint64_t k = g_Memory.Read<std::uint64_t>(ent + Offsets::Attribute::Key);
		if (!g_Memory.IsValid(k))
			continue;
		if (g_Memory.ReadString(k) != "position")
			continue;

		const std::uint64_t va = ent + Offsets::Attribute::Value;
		const std::uint64_t tag = g_Memory.Read<std::uint64_t>(va);
		const Vector3 pos = g_Memory.Read<Vector3>(va + 16);
		if (pos.x != pos.x || pos.y != pos.y || pos.z != pos.z)
			return false;
		if (std::fabs(pos.x) > 1e6f || std::fabs(pos.y) > 1e6f || std::fabs(pos.z) > 1e6f)
			return false;
		if (std::fabs(pos.x) < 0.01f && std::fabs(pos.y) < 0.01f && std::fabs(pos.z) < 0.01f)
			return false;

		// 7/20 = cframe, 4/17 = vec3; иначе тоже берём если tag мелкий
		if (tag > 0 && tag < 64)
		{
			out = pos;
			return true;
		}

		return false;
	}

	return false;
#else
	(void)inst;
	(void)out;
	return false;
#endif
}

static void DrawTextWithOutline(ImDrawList* draw_list, ImFont* font, float font_size,
                                ImVec2 pos, ImU32 color, const char* text)
{
    ImU32 shadow = IM_COL32(0, 0, 0, 255);
    font_size = fonts::snap_px(font_size);
    float x = std::floor(pos.x);
    float y = std::floor(pos.y);

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 && j == 0)
                continue;
            draw_list->AddText(font, font_size, ImVec2(x + i, y + j), shadow, text);
        }
    }

    draw_list->AddText(font, font_size, ImVec2(x, y), color, text);
}

static void SnapEspBox(float min_x, float min_y, float max_x, float max_y,
                       float& x1, float& y1, float& x2, float& y2)
{
    x1 = std::floor(min_x);
    y1 = std::floor(min_y);
    x2 = std::ceil(max_x);
    y2 = std::ceil(max_y);
    if (x2 <= x1) x2 = x1 + 1.0f;
    if (y2 <= y1) y2 = y1 + 1.0f;
}

// обычный бокс, опционально с чёрной обводкой
static void DrawBox(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right,
                    ImU32 color, float thick, bool outline)
{
    float x1, y1, x2, y2;
    SnapEspBox(top_left.x, top_left.y, bottom_right.x, bottom_right.y, x1, y1, x2, y2);
    if (thick < 0.5f) thick = 0.5f;

    if (outline) {
        if (thick <= 1.01f) {
            draw_list->AddRect(ImVec2(x1 - 1, y1 - 1), ImVec2(x2 + 1, y2 + 1),
                               IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);
            draw_list->AddRect(ImVec2(x1 + 1, y1 + 1), ImVec2(x2 - 1, y2 - 1),
                               IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);
        } else {
            draw_list->AddRect(ImVec2(x1, y1), ImVec2(x2, y2),
                               IM_COL32(0, 0, 0, 255), 0.0f, 0, thick + 2.0f);
        }
    }
    draw_list->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), color, 0.0f, 0, thick);
}

static void DrawCornerBox(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right,
                          ImU32 color, float thick, bool outline)
{
    float x1, y1, x2, y2;
    SnapEspBox(top_left.x, top_left.y, bottom_right.x, bottom_right.y, x1, y1, x2, y2);
    if (thick < 0.5f) thick = 0.5f;
    float lw = std::floor((x2 - x1) * 0.25f);
    float lh = std::floor((y2 - y1) * 0.25f);
    if (lw < 2.f) lw = 2.f;
    if (lh < 2.f) lh = 2.f;

    struct Seg { ImVec2 a, b; };
    Seg segs[8] = {
        { {x1, y1}, {x1 + lw, y1} }, { {x1, y1}, {x1, y1 + lh} },
        { {x2 - lw, y1}, {x2, y1} }, { {x2, y1}, {x2, y1 + lh} },
        { {x1, y2 - lh}, {x1, y2} }, { {x1, y2}, {x1 + lw, y2} },
        { {x2, y2 - lh}, {x2, y2} }, { {x2 - lw, y2}, {x2, y2} },
    };
    ImU32 black = IM_COL32(0, 0, 0, 255);
    if (outline) {
        const float ot = thick + 2.0f;
        for (const auto& s : segs) draw_list->AddLine(s.a, s.b, black, ot);
    }
    for (const auto& s : segs) draw_list->AddLine(s.a, s.b, color, thick);
}

static void DrawSkeletonLine(ImDrawList* draw_list, ImVec2 a, ImVec2 b,
                             ImU32 color, float thick, bool outline)
{
    if (thick < 1.0f) thick = 1.0f;
    if (outline)
        draw_list->AddLine(a, b, IM_COL32(0, 0, 0, 255), thick + 2.0f);
    draw_list->AddLine(a, b, color, thick);
}

static std::vector<ImVec2> ConvexHull(std::vector<ImVec2> pts)
{
    if (pts.size() < 3)
        return pts;

    std::sort(pts.begin(), pts.end(), [](const ImVec2& a, const ImVec2& b) {
        if (a.x < b.x)
            return true;
        if (a.x > b.x)
            return false;
        return a.y < b.y;
    });

    auto cross = [](const ImVec2& o, const ImVec2& a, const ImVec2& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };

    std::vector<ImVec2> hull(pts.size() * 2);
    int k = 0;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f)
            k--;
        hull[k++] = pts[i];
    }

    for (int i = (int)pts.size() - 2, t = k + 1; i >= 0; --i)
    {
        while (k >= t && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f)
            k--;
        hull[k++] = pts[i];
    }

    if (k > 0)
        hull.resize(k - 1);

    else
        hull.resize(0);

    return hull;
}

static constexpr int k_box_edges[12][2] = {
    {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},
    {2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
};

static bool SegInsidePoly(const ImVec2& a, const ImVec2& b,
                          const std::vector<ImVec2>& poly,
                          float& out_t0, float& out_t1)
{
    int n = (int)poly.size();
    if (n < 3)
        return false;

    ImVec2 c(0, 0);
    for (const auto& p : poly)
    {
        c.x += p.x;
        c.y += p.y;
    }
    c.x /= n;
    c.y /= n;

    float t0 = 0.0f, t1 = 1.0f;
    float eps = 0.25f;

    for (int i = 0; i < n; ++i)
    {
        const ImVec2& p = poly[i];
        const ImVec2& q = poly[(i + 1) % n];
        float ex = q.x - p.x, ey = q.y - p.y;

        auto side = [&](const ImVec2& v) { return ex * (v.y - p.y) - ey * (v.x - p.x); };
        float s = -1.0f;
        if (side(c) >= 0.0f)
            s = 1.0f;

        float f0 = s * side(a);
        float f1 = s * side(b);
        float df = f1 - f0;

        if (fabsf(df) < 1e-6f)
        {
            if (f0 < -eps)
                return false;

            continue;
        }

        float tc = (-eps - f0) / df;
        if (df > 0.0f)
        {
            if (tc > t0)
                t0 = tc;
        }

        else
        {
            if (tc < t1)
                t1 = tc;
        }

        if (t0 >= t1)
            return false;
    }

    if (t0 < 0.0f) t0 = 0.0f;
    if (t1 > 1.0f) t1 = 1.0f;
    out_t0 = t0;
    out_t1 = t1;
    return out_t1 > out_t0;
}

static std::vector<ImVec2> ClipHalfPlane(const std::vector<ImVec2>& poly,
                                         const ImVec2& p, const ImVec2& q, float s)
{
    std::vector<ImVec2> out;
    int n = (int)poly.size();
    if (n < 3)
        return out;
    out.reserve(n + 2);

    auto f = [&](const ImVec2& v) {
        return s * ((q.x - p.x) * (v.y - p.y) - (q.y - p.y) * (v.x - p.x));
    };

    for (int i = 0; i < n; ++i)
    {
        const ImVec2& a = poly[i];
        const ImVec2& b = poly[(i + 1) % n];
        float fa = f(a), fb = f(b);
        if (fa >= 0.0f)
            out.push_back(a);

        if ((fa < 0.0f) != (fb < 0.0f))
        {
            float t = fa / (fa - fb);
            out.push_back(ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t));
        }
    }

    if (out.size() < 3)
        out.clear();

    return out;
}

static void SubtractPoly(std::vector<ImVec2> piece, const std::vector<ImVec2>& B,
                         std::vector<std::vector<ImVec2>>& out)
{
    int n = (int)B.size();
    if (n < 3)
    {
        if (piece.size() >= 3)
            out.push_back(std::move(piece));
        return;
    }

    ImVec2 c(0, 0);
    for (const auto& v : B)
    {
        c.x += v.x;
        c.y += v.y;
    }
    c.x /= n;
    c.y /= n;

    for (int i = 0; i < n && piece.size() >= 3; ++i)
    {
        const ImVec2& p = B[i];
        const ImVec2& q = B[(i + 1) % n];
        float cs = (q.x - p.x) * (c.y - p.y) - (q.y - p.y) * (c.x - p.x);
        float s = -1.0f;
        if (cs >= 0.0f)
            s = 1.0f;

        auto outside = ClipHalfPlane(piece, p, q, -s);
        if (!outside.empty())
            out.push_back(std::move(outside));
        piece = ClipHalfPlane(piece, p, q, s);
    }
}

// куски линии снаружи полигонов (cham outline без пересечений)
static void DrawSegmentOutsideUnion(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                                    const std::vector<std::vector<ImVec2>>& polys,
                                    int skip, ImU32 color)
{
    std::vector<std::pair<float, float>> covered;
    for (int i = 0; i < (int)polys.size(); ++i)
    {
        if (i == skip)
            continue;
        float t0, t1;
        if (SegInsidePoly(a, b, polys[i], t0, t1))
            covered.emplace_back(t0, t1);
    }

    auto lerp_pt = [&](float t) { return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); };

    if (covered.empty())
    {
        dl->AddLine(a, b, color, 1.0f);
        return;
    }

    std::sort(covered.begin(), covered.end());

    float min_piece = 0.002f;
    float cursor = 0.0f;
    for (const auto& iv : covered)
    {
        if (iv.first > cursor + min_piece)
            dl->AddLine(lerp_pt(cursor), lerp_pt(iv.first), color, 1.0f);
        if (iv.second > cursor)
            cursor = iv.second;
        if (cursor >= 1.0f)
            break;
    }

    if (cursor < 1.0f - min_piece)
        dl->AddLine(lerp_pt(cursor), lerp_pt(1.0f), color, 1.0f);
}

static float g_esp_scale_x = 1.0f;
static float g_esp_scale_y = 1.0f;

// мир в экран через вьюматрицу
bool WorldToScreen(const Matrix4x4& matrix, const Vector2& dimensions, const Vector3& position, Vector2& screen)
{
    float w = position.x * matrix.m[3][0] + position.y * matrix.m[3][1] + position.z * matrix.m[3][2] + matrix.m[3][3];
    if (w < 0.01f)
        return false;

    float x = position.x * matrix.m[0][0] + position.y * matrix.m[0][1] + position.z * matrix.m[0][2] + matrix.m[0][3];
    float y = position.x * matrix.m[1][0] + position.y * matrix.m[1][1] + position.z * matrix.m[1][2] + matrix.m[1][3];

    float invw = 1.0f / w;
    x *= invw;
    y *= invw;

    screen.x = ((dimensions.x / 2) + (x * dimensions.x / 2)) * g_esp_scale_x;
    screen.y = ((dimensions.y / 2) - (y * dimensions.y / 2)) * g_esp_scale_y;
    return true;
}

// clip-space w точки (для near-plane клипа рёбер)
static float ClipW(const Matrix4x4& m, const Vector3& p)
{
    return p.x * m.m[3][0] + p.y * m.m[3][1] + p.z * m.m[3][2] + m.m[3][3];
}

// экранный AABB по 8 углам OBB; если угол за камерой — клипаем рёбра (иначе бокс плывёт при повороте)
static void ExpandScreenFromObbCorners(
    const Matrix4x4& vm, const Vector2& viewport,
    const Vector3 world[8],
    float& min_x, float& max_x, float& min_y, float& max_y,
    bool& any_visible)
{
    float cw[8];
    for (int i = 0; i < 8; ++i)
    {
        cw[i] = ClipW(vm, world[i]);
        Vector2 sp;
        if (cw[i] >= 0.01f && WorldToScreen(vm, viewport, world[i], sp))
        {
            min_x = (std::min)(min_x, sp.x);
            max_x = (std::max)(max_x, sp.x);
            min_y = (std::min)(min_y, sp.y);
            max_y = (std::max)(max_y, sp.y);
            any_visible = true;
        }
    }

    static const int k_edges[12][2] = {
        { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
        { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    };
    constexpr float k_near = 0.01f;
    for (const auto& e : k_edges)
    {
        const int a = e[0], b = e[1];
        const bool a_in = cw[a] >= k_near;
        const bool b_in = cw[b] >= k_near;
        if (a_in == b_in)
            continue;
        const float t = (k_near - cw[a]) / (cw[b] - cw[a]);
        if (t < 0.f || t > 1.f)
            continue;
        const Vector3 p{
            world[a].x + (world[b].x - world[a].x) * t,
            world[a].y + (world[b].y - world[a].y) * t,
            world[a].z + (world[b].z - world[a].z) * t,
        };
        Vector2 sp;
        if (WorldToScreen(vm, viewport, p, sp))
        {
            min_x = (std::min)(min_x, sp.x);
            max_x = (std::max)(max_x, sp.x);
            min_y = (std::min)(min_y, sp.y);
            max_y = (std::max)(max_y, sp.y);
            any_visible = true;
        }
    }
}

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

bool PointOnOverlay(const Vector2& screen, float overlay_w, float overlay_h, float pad)
{
    if (screen.x < -pad || screen.x > overlay_w + pad)
        return false;
    if (screen.y < -pad || screen.y > overlay_h + pad)
        return false;
    return true;
}

// 2 pi / 2
static float g_pi = 3.14159265358979323846f;

float CameraFovDegrees(float raw)
{
    // raw иногда в радианах
    if (raw > 0.f && raw < 3.2f)
        return raw * (180.f / g_pi);

    return raw;
}

bool IsInsideCameraFov(const Vector3& origin, const Matrix4x4& cam_rot,
                       const Vector3& target, float fov_deg, float aspect)
{
    Vector3 rel = target - origin;
    float sx = rel.x * cam_rot.m[0][0] + rel.y * cam_rot.m[1][0] + rel.z * cam_rot.m[2][0];
    float sy = rel.x * cam_rot.m[0][1] + rel.y * cam_rot.m[1][1] + rel.z * cam_rot.m[2][1];
    float forward =
        rel.x * (-cam_rot.m[0][2]) + rel.y * (-cam_rot.m[1][2]) + rel.z * (-cam_rot.m[2][2]);
    if (forward <= 0.f)
        return false;

    float fov = fov_deg;
    if (fov < 1.f) fov = 1.f;
    if (fov > 120.f) fov = 120.f;

    float a = aspect;
    if (a < 0.01f) a = 0.01f;

    float half_v = std::tan(fov * 0.5f * (g_pi / 180.f)) * forward;
    float half_h = half_v * a;
    return std::fabs(sx) <= half_h * 1.02f && std::fabs(sy) <= half_v * 1.02f;
}

// стрелка по yaw, сзади вниз, +x вправо от камеры
void ArrowDirYaw(const Vector3& origin, const Matrix4x4& cam_rot,
                 const Vector3& target, float& out_dx, float& out_dy)
{
    float lx = -cam_rot.m[0][2];
    float lz = -cam_rot.m[2][2];
    float llen = std::sqrt(lx * lx + lz * lz);
    if (llen < 1e-4f)
    {
        lx = 0.f;
        lz = -1.f;
        llen = 1.f;
    }

    lx /= llen;
    lz /= llen;

    float rx = -lz;
    float rz = lx;
    float dxw = target.x - origin.x;
    float dzw = target.z - origin.z;
    float dx = dxw * rx + dzw * rz;
    float dy = -(dxw * lx + dzw * lz);

    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f)
    {
        out_dx = 0.f;
        out_dy = 1.f;
        return;
    }

    out_dx = dx / len;
    out_dy = dy / len;
}

// стрелки когда чел за экраном
void DrawOffscreenArrow(ImDrawList* dl, float overlay_w, float overlay_h,
                        float dx, float dy, ImU32 color,
                        float size, float radius,
                        ImFont* font, float font_size, const char* info_text)
{
    ImVec2 center(overlay_w * 0.5f, overlay_h * 0.5f);
    float max_rx = overlay_w * 0.5f - 18.f;
    float max_ry = overlay_h * 0.5f - 18.f;
    if (max_rx < 40.f) max_rx = 40.f;
    if (max_ry < 40.f) max_ry = 40.f;

    float r = radius;
    if (r < 40.f) r = 40.f;
    float rmax = max_rx;
    if (max_ry > rmax) rmax = max_ry;
    if (r > rmax) r = rmax;

    float tip_x = center.x + dx * r;
    float tip_y = center.y + dy * r;
    float nx = (tip_x - center.x) / max_rx;
    float ny = (tip_y - center.y) / max_ry;
    float el = nx * nx + ny * ny;
    if (el > 1.f)
    {
        float sc = 1.f / std::sqrt(el);
        tip_x = center.x + (tip_x - center.x) * sc;
        tip_y = center.y + (tip_y - center.y) * sc;
    }

    ImVec2 tip(tip_x, tip_y);
    float s = size;
    if (s < 6.f) s = 6.f;
    float px = -dy, py = dx;
    ImVec2 a(tip.x - dx * s + px * s * 0.55f, tip.y - dy * s + py * s * 0.55f);
    ImVec2 b(tip.x - dx * s - px * s * 0.55f, tip.y - dy * s - py * s * 0.55f);
    dl->AddTriangleFilled(tip, a, b, color);
    dl->AddTriangle(tip, a, b, IM_COL32(0, 0, 0, 200), 1.0f);

    if (info_text && info_text[0] && font)
    {
        ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, info_text);
        ImVec2 tp(tip.x + dx * (s * 0.35f + 4.f) - ts.x * 0.5f,
                  tip.y + dy * (s * 0.35f + 4.f) - ts.y * 0.5f);

        float xmin = 2.f;
        float xmax = overlay_w - ts.x - 2.f;
        float ymin = 2.f;
        float ymax = overlay_h - ts.y - 2.f;
        if (tp.x < xmin) tp.x = xmin;
        if (tp.x > xmax) tp.x = xmax;
        if (tp.y < ymin) tp.y = ymin;
        if (tp.y > ymax) tp.y = ymax;
        tp.x = std::floor(tp.x);
        tp.y = std::floor(tp.y);
        DrawTextWithOutline(dl, font, font_size, tp, color, info_text);
    }
}

void BuildArrowInfo(const Cheat::PlayerCache& cache, float dist_studs, char* buf, size_t buf_n)
{
    buf[0] = '\0';
    const auto& ai = Cheat::g_Settings.esp.arrow_info;
    auto append = [&](const char* part) {
        if (!part || !part[0])
            return;
        size_t used = std::strlen(buf);
        if (used == 0)
            std::snprintf(buf, buf_n, "%s", part);

        else
            std::snprintf(buf + used, buf_n - used, " | %s", part);
    };

    if (ai[Cheat::Settings::ARROW_NAME])
    {
        const std::string* shown = &cache.name;
        if (Cheat::g_Settings.esp.name_mode == 0 && !cache.displayName.empty())
            shown = &cache.displayName;
        if (!shown->empty())
            append(shown->c_str());
    }

    if (ai[Cheat::Settings::ARROW_DISTANCE])
    {
        char dbuf[32];
        // havoc — всегда метры
        if (Cheat::Visuals::HavocWorldEsp::IsActivePlace() ||
            Cheat::g_Settings.esp.distance_unit == 1)
            std::snprintf(dbuf, sizeof(dbuf), "%.0fm",
                          Cheat::Visuals::HavocWorldEsp::StudsToMeters(dist_studs));

        else
            std::snprintf(dbuf, sizeof(dbuf), "%.0f", dist_studs);
        append(dbuf);
    }

    if (ai[Cheat::Settings::ARROW_HEALTH] && cache.humanoid)
    {
        char hbuf[32];
        std::snprintf(hbuf, sizeof(hbuf), "%.0f",
                      Cheat::Humanoid(cache.humanoid->address).GetHealth());
        append(hbuf);
    }

    if (ai[Cheat::Settings::ARROW_TOOL] && !cache.toolName.empty())
        append(cache.toolName.c_str());
}

ImU32 Col4(const float c[4])
{
    return IM_COL32((int)(c[0]*255),(int)(c[1]*255),(int)(c[2]*255),(int)(c[3]*255));
}

// конус над башкой
static void DrawChinaHat(
    ImDrawList* dl,
    const Matrix4x4& vm,
    const Vector2& viewport,
    float overlay_w,
    float overlay_h,
    const Vector3& head_pos,
    const float* col4)
{
    if (!dl || !col4)
    {
        return;
    }

    float hat_h = Cheat::g_Settings.esp.china_hat_height;
    float hat_r = Cheat::g_Settings.esp.china_hat_radius;
    if (hat_h < 0.1f)
    {
        hat_h = 0.1f;
    }

    if (hat_r < 0.1f)
    {
        hat_r = 0.1f;
    }

    const int segs = 48;
    Vector3 apex(
        head_pos.x,
        head_pos.y + hat_h + 0.15f,
        head_pos.z);

    Vector2 apex_s{};
    if (!WorldToScreen(vm, viewport, apex, apex_s))
    {
        return;
    }

    ImVec2 base_s[48];
    bool any = PointOnOverlay(apex_s, overlay_w, overlay_h, 2.f);
    bool all_ok = true;

    for (int i = 0; i < segs; ++i)
    {
        float ang = (2.f * 3.14159f * (float)i) / (float)segs;
        Vector3 p(
            head_pos.x + hat_r * cosf(ang),
            head_pos.y + 0.2f,
            head_pos.z + hat_r * sinf(ang));

        Vector2 sp{};
        if (!WorldToScreen(vm, viewport, p, sp))
        {
            all_ok = false;
            base_s[i] = ImVec2(-9999.f, -9999.f);
            continue;
        }

        base_s[i] = ImVec2(sp.x, sp.y);
        any = any || PointOnOverlay(sp, overlay_w, overlay_h, 2.f);
    }

    if (!any)
    {
        return;
    }

    ImU32 fill = IM_COL32(
        (int)(col4[0] * 255), (int)(col4[1] * 255),
        (int)(col4[2] * 255), (int)(col4[3] * 255));
    ImU32 base_col = IM_COL32(
        (int)(col4[0] * 255), (int)(col4[1] * 255),
        (int)(col4[2] * 255), (int)(col4[3] * 0.6f * 255));
    ImU32 outline = IM_COL32(0, 0, 0, 100);

    ImDrawListFlags fl = dl->Flags;
    dl->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;

    ImVec2 apex_im(apex_s.x, apex_s.y);
    const float soft = 2.f;

    for (int i = 0; i < segs; ++i)
    {
        int next = (i + 1) % segs;
        if (base_s[i].x < -9000.f || base_s[next].x < -9000.f)
        {
            continue;
        }

        float ang = (2.f * 3.14159f * (float)i) / (float)segs;
        ImVec2 apex_off(
            apex_im.x + cosf(ang) * soft,
            apex_im.y + sinf(ang) * soft);
        dl->AddTriangleFilled(apex_off, base_s[i], base_s[next], fill);
    }

    if (all_ok)
    {
        dl->AddConvexPolyFilled(base_s, segs, base_col);
    }

    for (int i = 0; i < segs; ++i)
    {
        int next = (i + 1) % segs;
        if (base_s[i].x < -9000.f || base_s[next].x < -9000.f)
        {
            continue;
        }

        dl->AddLine(base_s[i], base_s[next], outline, 1.2f);
    }

    dl->Flags = fl;
}

// есп каждый кадр оверлея
void Cheat::Visuals::ESP::Render()
{
    if (!Cheat::g_Settings.esp.enabled) return;
    // flags вырезаны — старый конфиг не вернёт
    Cheat::g_Settings.esp.flags = false;
    Cheat::g_Settings.esp.bot_esp[Cheat::Settings::BOT_FLAGS] = false;
    if (!Cheat::Globals::Workspace || !Cheat::Globals::InstanceDataModel.address) return;

    auto camera_ptr = Cheat::Globals::Workspace->GetCurrentCamera();
    if (!camera_ptr) return;

    Camera camera(camera_ptr->address);
    Vector2 viewport = camera.GetViewportSize();
    const Vector3 cam_pos = camera.GetPosition();
    const Matrix4x4 cam_rot = camera.GetRotation();
    const bool want_arrows = Cheat::g_Settings.esp.offscreen_arrows;
    const float cam_fov = want_arrows ? CameraFovDegrees(camera.GetFieldOfView()) : 70.f;
    const float cam_aspect = (viewport.y > 1.f) ? (viewport.x / viewport.y) : (16.f / 9.f);
    auto draw_list = ImGui::GetBackgroundDrawList();

    g_esp_scale_x = 1.0f;
    g_esp_scale_y = 1.0f;
    float overlay_w = viewport.x;
    float overlay_h = viewport.y;
    HWND oh = Renderer::GetHwnd();
    if (oh)
    {
        RECT ocr{};
        if (GetClientRect(oh, &ocr) && viewport.x > 1.0f && viewport.y > 1.0f)
        {
            overlay_w = (float)(ocr.right - ocr.left);
            overlay_h = (float)(ocr.bottom - ocr.top);
            if (overlay_w < 1.f) overlay_w = 1.f;
            if (overlay_h < 1.f) overlay_h = 1.f;
            g_esp_scale_x = overlay_w / viewport.x;
            g_esp_scale_y = overlay_h / viewport.y;
        }
    }

    ImFont* esp_font = fonts::selected();
    if (!esp_font) esp_font = ImGui::GetFont();

    const float esp_fs = fonts::snap_px(Cheat::g_Settings.esp.font_size);

    ImDrawListFlags backup_flags = draw_list->Flags;
    draw_list->Flags &= ~ImDrawListFlags_AntiAliasedLines;

    static const uintptr_t s_module_base = g_Memory.GetModuleBase();
    uintptr_t visual_engine = g_Memory.Read<uintptr_t>(s_module_base + ::VisualEngine::Pointer);
    Matrix4x4 vm = g_Memory.Read<Matrix4x4>(visual_engine + ::VisualEngine::ViewMatrix);

    // dx mesh: shader / occluded / gpu outline
    if (Cheat::g_Settings.esp.chams &&
        Cheat::g_Settings.esp.chams_mode == 4 &&
        (Cheat::g_Settings.esp.mesh_chams_style == 1 ||
         Cheat::g_Settings.esp.mesh_chams_occlusion ||
         Cheat::g_Settings.esp.mesh_chams_outline))
    {
        const float t = (float)GetTickCount64() * 0.001f;
        Cheat::Visuals::MeshDxShader::BeginFrame(vm, cam_pos, t);
    }

    std::uint64_t local_player_addr = 0;
    if (Cheat::Globals::Players && g_Memory.IsValid(Cheat::Globals::Players->address))
        local_player_addr = g_Memory.Read<std::uint64_t>(
            Cheat::Globals::Players->address + ::Player::LocalPlayer);

    // дистанция от своего hrp, не от камеры
    Vector3 dist_from = cam_pos;
    if (local_player_addr)
    {
        PlayerCache loc = PlayerHandler::GetCachedPlayer(local_player_addr);
        if (loc.humanoidRootPart && g_Memory.IsValid(loc.humanoidRootPart->address))
            dist_from = BasePart(loc.humanoidRootPart->address).GetPosition();
    }

    const std::uint64_t local_team_folder =
        Cheat::g_Settings.misc.teamcheck ? PlayerHandler::LocalTeamFolder() : 0;

    PlayerHandler::ForEachPlayer([&](const PlayerCache& cache)
    {
        if (!Cheat::g_Settings.esp.draw_local && cache.address == local_player_addr)
            return;

        if (Cheat::g_Settings.misc.teamcheck &&
            cache.address != local_player_addr &&
            PlayerHandler::IsTeammate(cache, local_team_folder))
            return;

        const bool is_bot = !cache.is_player && !cache.is_corpse;
        if (is_bot && !Cheat::g_Settings.esp.bots)
            return;

        const bool havoc = Visuals::HavocWorldEsp::IsActivePlace();
        bool is_dead = cache.is_corpse;
        if (!is_dead && cache.humanoid && g_Memory.IsValid(cache.humanoid->address)) {
            Humanoid hum(cache.humanoid->address);
            is_dead = hum.GetStateId() == 15 || hum.GetHealth() <= 0.0f;
        }
        if (havoc && is_dead)
            return;
        if (is_dead && Cheat::g_Settings.esp.dead_check && !Cheat::g_Settings.esp.body_corpse)
            return;

        const bool corpse_mode = !havoc && is_dead && Cheat::g_Settings.esp.body_corpse;

        // у ботов свои тогглы/цвета, не путать с игроками
        struct {
            bool box, name, skeleton, chams, healthbar, health_text, distance, tool;
            int  chams_mode, chams_shader;
            const float* box_color;
            const float* name_color;
            const float* skeleton_color;
            const float* chams_outline;
            const float* chams_fill;
            const float* distance_color;
            const float* tool_color;
            const float* arrow_color;
        } st{};

        if (is_bot) {
            const auto& be = Cheat::g_Settings.esp.bot_esp;
            st.box = be[Cheat::Settings::BOT_BOX];
            st.name = be[Cheat::Settings::BOT_NAME];
            st.skeleton = be[Cheat::Settings::BOT_SKELETON];
            st.chams = be[Cheat::Settings::BOT_CHAMS];
            st.healthbar = be[Cheat::Settings::BOT_HEALTHBAR];
            st.health_text = be[Cheat::Settings::BOT_HEALTH_TEXT];
            st.distance = be[Cheat::Settings::BOT_DISTANCE];
            st.tool = be[Cheat::Settings::BOT_TOOL];
            st.chams_mode = Cheat::g_Settings.esp.bot_chams_mode;
            st.chams_shader = Cheat::g_Settings.esp.bot_chams_shader;
            st.box_color = Cheat::g_Settings.esp.bot_box_color;
            st.name_color = Cheat::g_Settings.esp.bot_name_color;
            st.skeleton_color = Cheat::g_Settings.esp.bot_skeleton_color;
            st.chams_outline = Cheat::g_Settings.esp.bot_chams_outline_color;
            st.chams_fill = Cheat::g_Settings.esp.bot_chams_fill_color;
            st.distance_color = Cheat::g_Settings.esp.bot_distance_color;
            st.tool_color = Cheat::g_Settings.esp.bot_tool_color;
            st.arrow_color = Cheat::g_Settings.esp.bot_name_color;
        }

        else
        {
            st.box = Cheat::g_Settings.esp.box;
            st.name = Cheat::g_Settings.esp.name;
            st.skeleton = Cheat::g_Settings.esp.skeleton;
            st.chams = Cheat::g_Settings.esp.chams;
            st.healthbar = Cheat::g_Settings.esp.healthbar;
            st.health_text = Cheat::g_Settings.esp.health_text;
            st.distance = Cheat::g_Settings.esp.distance;
            st.tool = Cheat::g_Settings.esp.tool;
            st.chams_mode = Cheat::g_Settings.esp.chams_mode;
            st.chams_shader = Cheat::g_Settings.esp.chams_shader;
            st.box_color = Cheat::g_Settings.esp.box_color;
            st.name_color = Cheat::g_Settings.esp.name_color;
            st.skeleton_color = Cheat::g_Settings.esp.skeleton_color;
            st.chams_outline = Cheat::g_Settings.esp.chams_outline_color;
            st.chams_fill = Cheat::g_Settings.esp.chams_fill_color;
            st.distance_color = Cheat::g_Settings.esp.distance_color;
            st.tool_color = Cheat::g_Settings.esp.tool_color;
            st.arrow_color = Cheat::g_Settings.esp.arrow_color;
        }

        // живым hrp+голова, трупу любая парта ок
        auto pick_anchor = [&]() -> const Instance* {
            if (cache.humanoidRootPart && g_Memory.IsValid(cache.humanoidRootPart->address))
                return cache.humanoidRootPart.get();
            if (cache.head && g_Memory.IsValid(cache.head->address))
                return cache.head.get();
            if (cache.upperTorso && g_Memory.IsValid(cache.upperTorso->address))
                return cache.upperTorso.get();
            if (cache.lowerTorso && g_Memory.IsValid(cache.lowerTorso->address))
                return cache.lowerTorso.get();
            return nullptr;
        };

        const Instance* anchor = pick_anchor();
        if (!corpse_mode) {
            if (!cache.humanoidRootPart || !cache.head) return;
            anchor = cache.humanoidRootPart.get();
        }

        else if (!anchor)
        {
            return;
        }

        BasePart root(anchor->address);
        Vector3 root_pos = root.GetPosition();

        // havoc: position attr на рендер-потоке (lua heartbeat в другом треде = мигание)
        if (havoc && g_Memory.IsValid(cache.character))
        {
            Vector3 ap{};
            if (HavocReadPosAttr(cache.character, ap))
            {
                const float dx = ap.x - root_pos.x;
                const float dy = ap.y - root_pos.y;
                const float dz = ap.z - root_pos.z;
                if (dx * dx + dy * dy + dz * dz > 12.f * 12.f)
                {
                    if (cache.humanoidRootPart &&
                        g_Memory.IsValid(cache.humanoidRootPart->address))
                        BasePart(cache.humanoidRootPart->address).SetPosition(ap);
                }
                root_pos = ap;
            }
        }

        float dist_studs = dist_from.DistanceTo(root_pos);

        // havoc: люди 400m, боты — свой слайдер (метры)
        if (havoc)
        {
            float meters = Visuals::HavocWorldEsp::StudsToMeters(dist_studs);
            if (is_bot)
            {
                float cap = Cheat::g_Settings.esp.bot_max_distance;
                if (cap < 50.f) cap = 50.f;
                if (cap > 400.f) cap = 400.f;
                if (meters > cap)
                    return;
            }

            else if (Visuals::HavocWorldEsp::BeyondRange(dist_studs))
            {
                return;
            }
        }

        else if (Cheat::g_Settings.esp.distance_check)
        {
            float dist_for_check = dist_studs;
            if (Cheat::g_Settings.esp.distance_unit == 1)
                dist_for_check = dist_studs * 0.28f;

            if (dist_for_check > Cheat::g_Settings.esp.max_distance)
                return;
        }

        const bool want_tracer = Cheat::g_Settings.esp.tracer;

        Vector2 root_w2s{};
        Vector2 head_w2s{};
        const bool root_ok = WorldToScreen(vm, viewport, root_pos, root_w2s);
        bool head_ok = false;
        Vector3 head_pos{};
        if (cache.head) {
            head_pos = BasePart(cache.head->address).GetPosition();
            head_ok = WorldToScreen(vm, viewport, head_pos, head_w2s);
        }
        const bool on_screen =
            (root_ok && PointOnOverlay(root_w2s, overlay_w, overlay_h, 2.f)) ||
            (head_ok && PointOnOverlay(head_w2s, overlay_w, overlay_h, 2.f));

        if (want_arrows && !on_screen && !corpse_mode) {
            const bool in_fov =
                IsInsideCameraFov(cam_pos, cam_rot, root_pos, cam_fov, cam_aspect) ||
                (cache.head && IsInsideCameraFov(cam_pos, cam_rot, head_pos, cam_fov, cam_aspect));
            if (!in_fov) {
                float adx = 0.f, ady = 1.f;
                ArrowDirYaw(cam_pos, cam_rot, root_pos, adx, ady);

                const bool aim_target =
                    Cheat::Features::Aim::CurrentTarget() != 0 &&
                    cache.address == Cheat::Features::Aim::CurrentTarget();
                const ImU32 arrow_col = aim_target
                    ? IM_COL32(255, 45, 45, 255)
                    : Col4(st.arrow_color);

                char info_buf[160];
                BuildArrowInfo(cache, dist_studs, info_buf, sizeof(info_buf));
                DrawOffscreenArrow(draw_list, overlay_w, overlay_h, adx, ady,
                                   arrow_col, Cheat::g_Settings.esp.arrow_size,
                                   Cheat::g_Settings.esp.arrow_radius,
                                   esp_font, esp_fs,
                                   info_buf[0] ? info_buf : nullptr);
            }
        }

        if (!on_screen)
            return;

        std::vector<std::shared_ptr<Instance>> parts = {
            cache.head, cache.humanoidRootPart, cache.upperTorso, cache.lowerTorso,
            cache.leftUpperArm, cache.rightUpperArm, cache.leftLowerArm, cache.rightLowerArm,
            cache.leftUpperLeg, cache.rightUpperLeg, cache.leftLowerLeg, cache.rightLowerLeg,
            cache.leftFoot, cache.rightFoot, cache.leftHand, cache.rightHand
        };

        // свежая VP прямо перед боксом — иначе при повороте камеры плывёт
        vm = g_Memory.Read<Matrix4x4>(visual_engine + ::VisualEngine::ViewMatrix);

        float min_x = 10000.0f, max_x = -10000.0f;
        float min_y = 10000.0f, max_y = -10000.0f;
        bool any_visible = false;

        Vector3 wmin{ 1e9f, 1e9f, 1e9f }, wmax{ -1e9f, -1e9f, -1e9f };

        struct PartCorners { ImVec2 pt[8]; bool full; };
        std::vector<PartCorners> chams_parts;
        const bool want_chams = st.chams;

        struct PartData { Vector3 pos; Vector3 sz; Matrix4x4 rot; const Instance* inst; };
        std::vector<PartData> part_data;
        part_data.reserve(parts.size());

        for (auto const& part : parts)
        {
            if (!part) continue;

            BasePart bp(part->address);
            Vector3 pos = bp.GetPosition();
            // hitbox expander — ESP на оригинальном size
            Vector3 sz = Cheat::Features::HitboxExpander::SizeForEsp(part->address, bp.GetSize());
            Matrix4x4 rot = bp.GetRotation();

            // PF: у парт size часто ~0 — как в roblox-ext подставляем R6
            if (Games::PhantomForces::IsActivePlace() &&
                sz.x < 0.01f && sz.y < 0.01f && sz.z < 0.01f) {
                if (cache.head && part.get() == cache.head.get())
                    sz = { 1.f, 1.f, 1.f };
                else if ((cache.upperTorso && part.get() == cache.upperTorso.get()) ||
                         (cache.humanoidRootPart && part.get() == cache.humanoidRootPart.get()))
                    sz = { 2.f, 2.f, 1.f };
                else
                    sz = { 1.f, 2.f, 1.f };
            }

            part_data.push_back({ pos, sz, rot, part.get() });

            const bool is_hrp = (cache.humanoidRootPart &&
                                 part.get() == cache.humanoidRootPart.get());

            Vector3 half = { sz.x / 2.0f, sz.y / 2.0f, sz.z / 2.0f };

            Vector3 local[8] = {
                { -half.x, -half.y, -half.z }, { -half.x, -half.y,  half.z },
                { -half.x,  half.y, -half.z }, { -half.x,  half.y,  half.z },
                {  half.x, -half.y, -half.z }, {  half.x, -half.y,  half.z },
                {  half.x,  half.y, -half.z }, {  half.x,  half.y,  half.z }
            };

            Vector3 world[8];
            PartCorners pc{}; pc.full = true;
            for (int i = 0; i < 8; ++i)
            {
                world[i] = {
                    pos.x + (rot.m[0][0] * local[i].x + rot.m[0][1] * local[i].y + rot.m[0][2] * local[i].z),
                    pos.y + (rot.m[1][0] * local[i].x + rot.m[1][1] * local[i].y + rot.m[1][2] * local[i].z),
                    pos.z + (rot.m[2][0] * local[i].x + rot.m[2][1] * local[i].y + rot.m[2][2] * local[i].z)
                };
                wmin.x = (std::min)(wmin.x, world[i].x); wmax.x = (std::max)(wmax.x, world[i].x);
                wmin.y = (std::min)(wmin.y, world[i].y); wmax.y = (std::max)(wmax.y, world[i].y);
                wmin.z = (std::min)(wmin.z, world[i].z); wmax.z = (std::max)(wmax.z, world[i].z);

                Vector2 screen_pos;
                if (WorldToScreen(vm, viewport, world[i], screen_pos))
                    pc.pt[i] = ImVec2(screen_pos.x, screen_pos.y);
                else
                    pc.full = false;
            }

            // hrp в бокс не — на дистанции раздувает и хп уезжает вниз
            if (!is_hrp)
            {
                ExpandScreenFromObbCorners(vm, viewport, world,
                                           min_x, max_x, min_y, max_y, any_visible);
            }

            // overlay AABB chams; mesh=4 резолвит MeshData отдельно
            if (want_chams && pc.full && !is_hrp &&
                st.chams_mode != 4)
                chams_parts.push_back(pc);
        }

        // bounding type=mesh: полный обход партов+мешей (не кэш конечностей — он часто неполный)
        if (Cheat::g_Settings.esp.bounding_type == 1 &&
            g_Memory.IsValid(cache.character))
        {
            vm = g_Memory.Read<Matrix4x4>(visual_engine + ::VisualEngine::ViewMatrix);
            min_x = 10000.0f; max_x = -10000.0f;
            min_y = 10000.0f; max_y = -10000.0f;
            any_visible = false;
            wmin = { 1e9f, 1e9f, 1e9f };
            wmax = { -1e9f, -1e9f, -1e9f };
            if (Cheat::Visuals::MeshChams::ExpandBounds(
                    cache.character, vm, viewport,
                    g_esp_scale_x, g_esp_scale_y,
                    min_x, max_x, min_y, max_y, wmin, wmax))
                any_visible = true;
        }

        // stab: тока подрезаем раздутый OBB к head/feet, не схлопываем
        else if (!corpse_mode && any_visible)
        {
            vm = g_Memory.Read<Matrix4x4>(visual_engine + ::VisualEngine::ViewMatrix);

            float save_miny = min_y;
            float save_maxy = max_y;
            float top_y = min_y;
            float bot_y = max_y;
            bool got_top = false;
            bool got_bot = false;

            if (cache.head && g_Memory.IsValid(cache.head->address))
            {
                BasePart hp(cache.head->address);
                Vector3 hs = hp.GetSize();
                float hy = hs.y * 0.5f;
                if (hy < 0.2f) hy = 0.5f;
                Vector3 apex = head_pos;
                apex.y += hy;
                Vector2 sp{};
                if (WorldToScreen(vm, viewport, apex, sp))
                {
                    top_y = sp.y;
                    got_top = true;
                }
            }

            float feet_y = root_pos.y - 3.f;
            float feet_x = root_pos.x;
            float feet_z = root_pos.z;

            if (cache.leftFoot && g_Memory.IsValid(cache.leftFoot->address))
            {
                BasePart fp(cache.leftFoot->address);
                Vector3 p = fp.GetPosition();
                Vector3 s = fp.GetSize();
                feet_y = p.y - s.y * 0.5f;
                feet_x = p.x;
                feet_z = p.z;
            }

            if (cache.rightFoot && g_Memory.IsValid(cache.rightFoot->address))
            {
                BasePart fp(cache.rightFoot->address);
                Vector3 p = fp.GetPosition();
                Vector3 s = fp.GetSize();
                float bottom = p.y - s.y * 0.5f;
                if (bottom < feet_y)
                {
                    feet_y = bottom;
                    feet_x = p.x;
                    feet_z = p.z;
                }
            }

            Vector3 feet{ feet_x, feet_y, feet_z };
            Vector2 fsp{};
            if (WorldToScreen(vm, viewport, feet, fsp))
            {
                bot_y = fsp.y;
                got_bot = true;
            }

            if (got_top && got_bot && bot_y > top_y + 8.f)
            {
                // низ уехал под ноги / верх над башкой — подрезать
                if (max_y > bot_y + 2.f)
                    max_y = bot_y;
                if (min_y < top_y - 2.f)
                    min_y = top_y;

                // схлопнулось — откат
                if (max_y - min_y < 14.f)
                {
                    min_y = save_miny;
                    max_y = save_maxy;
                }
            }
        }

        if (!any_visible || min_x > 5000.0f) return;

        bool aim_target =
            Cheat::Features::Aim::CurrentTarget() != 0 &&
            cache.address == Cheat::Features::Aim::CurrentTarget();
        ImU32 aim_red = IM_COL32(255, 45, 45, 255);
        ImU32 aim_red_fill = IM_COL32(255, 45, 45, 110);

        if (!corpse_mode && want_tracer)
        {
            ImVec2 from = TracerOrigin(overlay_w, overlay_h,
                                       Cheat::g_Settings.esp.tracer_origin);
            ImVec2 to((min_x + max_x) * 0.5f, max_y);
            ImU32 tc = Col4(is_bot ? st.box_color : Cheat::g_Settings.esp.tracer_color);
            if (aim_target)
                tc = aim_red;
            draw_list->AddLine(from, to, IM_COL32(0, 0, 0, 200), 2.5f);
            draw_list->AddLine(from, to, tc, 1.0f);
        }

        float hit_fade = 0.f;
        if (!corpse_mode && Cheat::g_Settings.esp.hit_chams)
        {
            int cm = st.chams_mode;
            if (cm == 2 || cm == 3 || cm == 4)
            {
                hit_fade = Cheat::Visuals::KillEffects::HitChamsFade(cache.address);
            }
        }

        // mesh chams: вершины/фейсы из MeshContentProvider
        if (want_chams && st.chams_mode == 4 && g_Memory.IsValid(cache.character))
        {
            const float* fc_ptr = corpse_mode ? Cheat::g_Settings.esp.corpse_color : st.chams_fill;
            ImU32 fill_col = IM_COL32(
                (int)(fc_ptr[0] * 255), (int)(fc_ptr[1] * 255), (int)(fc_ptr[2] * 255),
                corpse_mode ? (int)(fc_ptr[3] * 0.55f * 255) : (int)(fc_ptr[3] * 255));
            if (!corpse_mode && aim_target)
                fill_col = aim_red_fill;

            if (hit_fade > 0.001f)
            {
                const float* hc = Cheat::g_Settings.esp.hit_chams_color;
                float inv = 1.f - hit_fade;
                float rr = fc_ptr[0] * inv + hc[0] * hit_fade;
                float gg = fc_ptr[1] * inv + hc[1] * hit_fade;
                float bb = fc_ptr[2] * inv + hc[2] * hit_fade;
                float aa = fc_ptr[3] * inv + hc[3] * hit_fade;
                fill_col = IM_COL32(
                    (int)(rr * 255), (int)(gg * 255),
                    (int)(bb * 255), (int)(aa * 255));
            }

            // свежая матрица на каждого персонажа (mesh долгий → старый vm = отставание)
            vm = g_Memory.Read<Matrix4x4>(visual_engine + ::VisualEngine::ViewMatrix);
            Cheat::Visuals::MeshChams::Draw(
                draw_list, cache.character, vm, viewport,
                g_esp_scale_x, g_esp_scale_y, fill_col);
        }

        // overlay chams (не mesh)
        if (want_chams && !chams_parts.empty() &&
            st.chams_mode != 4)
        {
            const float* oc_ptr = corpse_mode ? Cheat::g_Settings.esp.corpse_color : st.chams_outline;
            const float* fc_ptr = corpse_mode ? Cheat::g_Settings.esp.corpse_color : st.chams_fill;
            const float* corpse_override = corpse_mode ? Cheat::g_Settings.esp.corpse_color : nullptr;

            ImU32 outline_col = IM_COL32(
                (int)(oc_ptr[0]*255),(int)(oc_ptr[1]*255),(int)(oc_ptr[2]*255),(int)(oc_ptr[3]*255));
            ImU32 fill_col = IM_COL32(
                (int)(fc_ptr[0]*255),(int)(fc_ptr[1]*255),(int)(fc_ptr[2]*255),
                corpse_mode ? (int)(fc_ptr[3] * 0.55f * 255) : (int)(fc_ptr[3]*255));
            if (!corpse_mode && aim_target)
            {
                outline_col = aim_red;
                fill_col = aim_red_fill;
            }

            int mode = st.chams_mode;

            if (mode == 0)
            {
                for (const auto& pc : chams_parts)
                    for (const auto& e : k_box_edges)
                        draw_list->AddLine(pc.pt[e[0]], pc.pt[e[1]], outline_col, 1.0f);
            }

            else if (mode == 1)
            {
                for (const auto& pc : chams_parts)
                {
                    std::vector<ImVec2> pts(pc.pt, pc.pt + 8);
                    auto hull = ConvexHull(pts);
                    if (hull.size() >= 3)
                        draw_list->AddConvexPolyFilled(hull.data(), (int)hull.size(), fill_col);
                }
                for (const auto& pc : chams_parts)
                    for (const auto& e : k_box_edges)
                        draw_list->AddLine(pc.pt[e[0]], pc.pt[e[1]], outline_col, 1.0f);
            }

            else
            {
                std::vector<std::vector<ImVec2>> hulls;
                hulls.reserve(chams_parts.size());
                for (const auto& pc : chams_parts) {
                    std::vector<ImVec2> pts(pc.pt, pc.pt + 8);
                    hulls.push_back(ConvexHull(pts));
                }

                std::vector<std::vector<ImVec2>> clipped;
                clipped.reserve(hulls.size() * 2);
                for (int i = 0; i < (int)hulls.size(); ++i) {
                    if (hulls[i].size() < 3) continue;

                    std::vector<std::vector<ImVec2>> pieces{ hulls[i] };
                    for (int j = 0; j < i && !pieces.empty(); ++j) {
                        if (hulls[j].size() < 3) continue;
                        std::vector<std::vector<ImVec2>> next;
                        for (auto& piece : pieces)
                            SubtractPoly(std::move(piece), hulls[j], next);
                        pieces = std::move(next);
                    }
                    for (auto& piece : pieces)
                        if (piece.size() >= 3)
                            clipped.push_back(std::move(piece));
                }

                if (mode == 3) {
                    const int shader = st.chams_shader;
                    ShaderChams::DrawFill(draw_list, clipped, (float)ImGui::GetTime(),
                                          shader, aim_target && !corpse_mode, false, corpse_override);

                    if (hit_fade > 0.001f)
                    {
                        const float* hc = Cheat::g_Settings.esp.hit_chams_color;
                        float hit_ov[4] = {
                            hc[0], hc[1], hc[2], hc[3] * hit_fade
                        };
                        ShaderChams::DrawFill(draw_list, clipped, (float)ImGui::GetTime(),
                                              shader, false, false, hit_ov);
                    }

                    const ImU32 shader_outline = corpse_override
                        ? ShaderChams::OutlineColor(shader, false, corpse_override)
                        : (aim_target ? aim_red : ShaderChams::OutlineColor(shader, false));
                    for (int i = 0; i < (int)hulls.size(); ++i) {
                        const auto& hull = hulls[i];
                        const int n = (int)hull.size();
                        if (n < 2) continue;
                        for (int e = 0; e < n; ++e)
                            DrawSegmentOutsideUnion(draw_list,
                                hull[e], hull[(e + 1) % n],
                                hulls, i, shader_outline);
                    }
                }

                else
                {
                    ImDrawListFlags fill_backup = draw_list->Flags;
                    draw_list->Flags &= ~ImDrawListFlags_AntiAliasedFill;

                    for (const auto& piece : clipped)
                        draw_list->AddConvexPolyFilled(piece.data(), (int)piece.size(), fill_col);

                    if (hit_fade > 0.001f && mode == 2)
                    {
                        const float* hc = Cheat::g_Settings.esp.hit_chams_color;
                        ImU32 hit_col = IM_COL32(
                            (int)(hc[0] * 255), (int)(hc[1] * 255),
                            (int)(hc[2] * 255), (int)(hc[3] * hit_fade * 255));
                        for (const auto& piece : clipped)
                            draw_list->AddConvexPolyFilled(
                                piece.data(), (int)piece.size(), hit_col);
                    }

                    draw_list->Flags = fill_backup;

                    for (int i = 0; i < (int)hulls.size(); ++i) {
                        const auto& hull = hulls[i];
                        const int n = (int)hull.size();
                        if (n < 2) continue;
                        for (int e = 0; e < n; ++e)
                            DrawSegmentOutsideUnion(draw_list,
                                hull[e], hull[(e + 1) % n],
                                hulls, i, outline_col);
                    }
                }
            }
        }

        if (!corpse_mode && Cheat::g_Settings.esp.china_hat && cache.head)
        {
            DrawChinaHat(
                draw_list, vm, viewport, overlay_w, overlay_h,
                head_pos, Cheat::g_Settings.esp.china_hat_color);
        }

        // труп: чамсы уже нарисовали, имя x_x и выход
        if (corpse_mode) {
            if (st.name) {
                float cx1, cy1, cx2, cy2;
                SnapEspBox(min_x, min_y, max_x, max_y, cx1, cy1, cx2, cy2);
                const char* corpse_name = "x_x";
                const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, corpse_name);
                const auto& nc = Cheat::g_Settings.esp.corpse_color;
                const ImU32 name_col = IM_COL32(
                    (int)(nc[0] * 255), (int)(nc[1] * 255),
                    (int)(nc[2] * 255), (int)(nc[3] * 255));
                const float tx = std::floor((cx1 + cx2) * 0.5f - tsz.x * 0.5f);
                const float ty = std::floor(cy1 - tsz.y - 2.0f);
                DrawTextWithOutline(draw_list, esp_font, esp_fs, ImVec2(tx, ty), name_col, corpse_name);
            }
            return;
        }

        const bool want_health = st.healthbar || st.health_text;
        float hp = 0.0f, max_hp = 100.0f;
        if (want_health && cache.humanoid) {
            Humanoid hum(cache.humanoid->address);
            hp = hum.GetHealth();
            max_hp = hum.GetMaxHealth();
            if (max_hp <= 0.0f) max_hp = 100.0f;
        }
        float hp_frac = 0.0f;
        if (hp <= 0.0f)
            hp_frac = 0.0f;

        else if (hp >= max_hp)
            hp_frac = 1.0f;

        else
            hp_frac = hp / max_hp;

        float bx1, by1, bx2, by2;
        SnapEspBox(min_x, min_y, max_x, max_y, bx1, by1, bx2, by2);

        // бокс: 2d / углы / 3d (fill рисуем после скелета — перекрывает визуалы)
        if (st.box) {
            const float* bc = st.box_color;
            ImU32 box_color = aim_target ? aim_red : IM_COL32(
                (int)(bc[0] * 255), (int)(bc[1] * 255), (int)(bc[2] * 255), (int)(bc[3] * 255));
            const int box_mode = Cheat::g_Settings.esp.box_mode;

            bool drew_3d = false;
            if (box_mode == 2) {
                ImVec2 pts[8];
                bool all_ok = true;
                for (int i = 0; i < 8 && all_ok; ++i) {
                    const Vector3 c = {
                        (i & 4) ? wmax.x : wmin.x,
                        (i & 2) ? wmax.y : wmin.y,
                        (i & 1) ? wmax.z : wmin.z
                    };
                    Vector2 sp;
                    if (WorldToScreen(vm, viewport, c, sp))
                        pts[i] = ImVec2(sp.x, sp.y);
                    else
                        all_ok = false;
                }
                if (all_ok) {
                    const float box_t = Cheat::g_Settings.esp.box_thickness;
                    const bool box_ol = Cheat::g_Settings.esp.esp_outline[
                        Cheat::Settings::OUTLINE_BOX];
                    if (box_ol) {
                        const ImU32 black = IM_COL32(0, 0, 0, 255);
                        const float ot = (box_t < 0.5f ? 0.5f : box_t) + 2.0f;
                        for (const auto& e : k_box_edges)
                            draw_list->AddLine(pts[e[0]], pts[e[1]], black, ot);
                    }
                    for (const auto& e : k_box_edges)
                        draw_list->AddLine(pts[e[0]], pts[e[1]], box_color,
                                           box_t < 0.5f ? 0.5f : box_t);
                    drew_3d = true;
                }
            }

            const float box_t = Cheat::g_Settings.esp.box_thickness;
            const bool box_ol = Cheat::g_Settings.esp.esp_outline[
                Cheat::Settings::OUTLINE_BOX];
            if (box_mode == 1)
                DrawCornerBox(draw_list, ImVec2(bx1, by1), ImVec2(bx2, by2),
                              box_color, box_t, box_ol);
            else if (!drew_3d && box_mode != 1)
                DrawBox(draw_list, ImVec2(bx1, by1), ImVec2(bx2, by2),
                        box_color, box_t, box_ol);
        }

        const EspLayout::Box ebox{ bx1, by1, bx2, by2 };

        // мелкий бокс — чуть жмём шрифт, не в кашу
        float bh = by2 - by1;
        float fs = esp_fs;
        {
            float s = 1.f;
            if (bh < 40.f && bh > 1.f)
                s = 0.75f + 0.25f * (bh / 40.f);
            if (s < 0.75f) s = 0.75f;
            if (s > 1.f) s = 1.f;
            fs = fonts::snap_px(esp_fs * s);
            if (fs < 10.f) fs = 10.f;
        }

        EspLayout::ResolveAllSides(Cheat::g_Settings, fs, 1, -1);

        float bar_w = 2.f;
        if (bh < 36.f) bar_w = 1.f;
        float bar_gap = 3.f;
        if (bh < 48.f) bar_gap = 2.f;
        float pad = 2.f + (fs / esp_fs) * 2.f;
        if (pad < 2.f) pad = 2.f;
        float pad_l = pad + 2.f;
        float pad_r = pad + 2.f;

        if (st.healthbar)
        {
            int hb_side = EspLayout::ClampHealthBarSide(Cheat::g_Settings.esp.healthbar_side);
            float bar_x1, bar_y1, bar_x2, bar_y2;
            EspLayout::PlaceHealthBar(hb_side, ebox, bar_w, bar_gap,
                                      bar_x1, bar_y1, bar_x2, bar_y2);
            if (hb_side == EspLayout::Right)
                pad_r += bar_w + bar_gap + 1.0f;

            else
                pad_l += bar_w + bar_gap + 1.0f;

            const float bar_h = bar_y2 - bar_y1;
            const float fill_h = std::floor(bar_h * hp_frac + 0.5f);
            const float fill_top = bar_y2 - fill_h;

            draw_list->AddRectFilled(ImVec2(bar_x1 - 1.0f, bar_y1 - 1.0f),
                                     ImVec2(bar_x2 + 1.0f, bar_y2 + 1.0f),
                                     IM_COL32(0, 0, 0, 255));

            const ImU32 hp_col = IM_COL32((int)(255 * (1.0f - hp_frac)), (int)(255 * hp_frac), 0, 255);
            if (fill_h > 0.0f)
                draw_list->AddRectFilled(ImVec2(bar_x1, fill_top), ImVec2(bar_x2, bar_y2), hp_col);
        }

        auto place_and_draw = [&](int side, float offset, const char* text, ImU32 col) {
            const ImVec2 tsz = esp_font->CalcTextSizeA(fs, FLT_MAX, 0.0f, text);
            const int sidx = (side < 0 || side > 3) ? 0 : side;
            float tx, ty;
            EspLayout::PlaceText(sidx, ebox, offset, tsz.x, tsz.y,
                                 pad_l, pad_r, pad, tx, ty);
            DrawTextWithOutline(draw_list, esp_font, fs,
                ImVec2(std::floor(tx), std::floor(ty)), col, text);
        };

        if (st.name) {
            char name_buf[160];
            const std::string& shown = (Cheat::g_Settings.esp.name_mode == 0 && !cache.displayName.empty())
                ? cache.displayName
                : cache.name;
            if (is_bot)
                std::snprintf(name_buf, sizeof(name_buf), "[bot] %s", shown.c_str());
            else
                std::snprintf(name_buf, sizeof(name_buf), "%s", shown.c_str());
            const float* nc = st.name_color;
            const ImU32 name_col = aim_target ? aim_red : IM_COL32(
                (int)(nc[0]*255),(int)(nc[1]*255),(int)(nc[2]*255),(int)(nc[3]*255));
            place_and_draw(Cheat::g_Settings.esp.name_side, Cheat::g_Settings.esp.name_off,
                           name_buf, name_col);
        }

        if (st.distance) {
            char dist_buf[32];
            if (havoc || Cheat::g_Settings.esp.distance_unit == 1)
                std::snprintf(dist_buf, sizeof(dist_buf), "%.0fm",
                              Visuals::HavocWorldEsp::StudsToMeters(dist_studs));

            else
                std::snprintf(dist_buf, sizeof(dist_buf), "%.0f studs", dist_studs);

            const float* dc = st.distance_color;
            place_and_draw(Cheat::g_Settings.esp.distance_side, Cheat::g_Settings.esp.distance_off,
                dist_buf,
                IM_COL32((int)(dc[0]*255),(int)(dc[1]*255),(int)(dc[2]*255),(int)(dc[3]*255)));
        }

        if (st.tool && !cache.toolName.empty()) {
            char tool_buf[128];
            std::snprintf(tool_buf, sizeof(tool_buf), "[%s]", cache.toolName.c_str());

            const float* tc = st.tool_color;
            place_and_draw(Cheat::g_Settings.esp.tool_side, Cheat::g_Settings.esp.tool_off,
                tool_buf,
                IM_COL32((int)(tc[0]*255),(int)(tc[1]*255),(int)(tc[2]*255),(int)(tc[3]*255)));
        }

        if (st.health_text) {
            const bool with_bar = st.healthbar;
            char hp_buf[16];
            if (with_bar)
                std::snprintf(hp_buf, sizeof(hp_buf), "%.0f", hp);
            else
                std::snprintf(hp_buf, sizeof(hp_buf), "%.0f hp", hp);
            place_and_draw(Cheat::g_Settings.esp.health_text_side,
                           Cheat::g_Settings.esp.health_text_off, hp_buf,
                           IM_COL32(255, 255, 255, 255));
        }

        // кости / png скелет (anton / unfunny / egor)
        if (st.skeleton) {
            const float* sc = st.skeleton_color;
            const int skel_type = Cheat::g_Settings.esp.skeleton_type;

            if (skel_type >= 1 && skel_type <= 3) {
                int img = Cheat::Visuals::BoxFill::SK;
                if (skel_type == 2)
                    img = Cheat::Visuals::BoxFill::US;
                else if (skel_type == 3)
                    img = Cheat::Visuals::BoxFill::SE;
                Cheat::Visuals::BoxFill::Draw(draw_list, img,
                    ImVec2(bx1, by1), ImVec2(bx2, by2), sc[3], true);
            } else {
            const ImU32 skel_color = aim_target ? aim_red : IM_COL32(
                (int)(sc[0]*255),(int)(sc[1]*255),(int)(sc[2]*255),(int)(sc[3]*255));

            auto find_part = [&](const std::shared_ptr<Instance>& part) -> const PartData* {
                if (!part) return nullptr;
                for (const auto& pd : part_data)
                    if (pd.inst == part.get()) return &pd;
                return nullptr;
            };

            auto part_pos = [&](const std::shared_ptr<Instance>& part, Vector3& out) -> bool {
                const PartData* pd = find_part(part);
                if (!pd) return false;
                out = pd->pos;
                return true;
            };

            auto part_axis = [&](const std::shared_ptr<Instance>& part, Vector3& top, Vector3& bottom) -> bool {
                const PartData* pd = find_part(part);
                if (!pd) return false;
                const float hy = pd->sz.y / 2.0f;
                const Vector3 up = { pd->rot.m[0][1] * hy, pd->rot.m[1][1] * hy, pd->rot.m[2][1] * hy };
                top    = { pd->pos.x + up.x, pd->pos.y + up.y, pd->pos.z + up.z };
                bottom = { pd->pos.x - up.x, pd->pos.y - up.y, pd->pos.z - up.z };
                return true;
            };

            const float skel_t = Cheat::g_Settings.esp.skeleton_thickness;
            const bool skel_ol = Cheat::g_Settings.esp.esp_outline[
                Cheat::Settings::OUTLINE_SKELETON];
            auto line_w2s = [&](const Vector3& a, const Vector3& b) {
                Vector2 sa, sb;
                if (WorldToScreen(vm, viewport, a, sa) && WorldToScreen(vm, viewport, b, sb))
                    DrawSkeletonLine(draw_list, ImVec2(sa.x, sa.y), ImVec2(sb.x, sb.y),
                                     skel_color, skel_t, skel_ol);
            };

            auto lerp3 = [](const Vector3& a, const Vector3& b, float t) -> Vector3 {
                return { a.x + (b.x - a.x) * t,
                         a.y + (b.y - a.y) * t,
                         a.z + (b.z - a.z) * t };
            };

            auto point_at_height = [&](const Vector3& top, const Vector3& bottom,
                                       const Vector3& ref) -> Vector3 {
                const float dy = top.y - bottom.y;
                if (fabsf(dy) < 0.001f) return lerp3(top, bottom, 0.5f);
                float t = (top.y - ref.y) / dy;
                if (t < 0.0f) t = 0.0f;

                else if (t > 1.0f) t = 1.0f;
                return lerp3(top, bottom, t);
            };

            if (cache.isR6)
            {
                // r6 плечи чуть ниже топа торса
                float shoulder_drop = 0.18f;

                Vector3 torso_top, torso_bot;
                bool torso_ok = part_axis(cache.upperTorso, torso_top, torso_bot);
                Vector3 shoulder_c{};
                if (torso_ok)
                {
                    shoulder_c = lerp3(torso_top, torso_bot, shoulder_drop);
                    line_w2s(shoulder_c, torso_bot);
                }

                Vector3 t, b;
                if (part_axis(cache.leftUpperArm, t, b))
                {
                    Vector3 joint = t;
                    if (torso_ok)
                        joint = point_at_height(t, b, shoulder_c);
                    if (torso_ok)
                        line_w2s(shoulder_c, joint);
                    line_w2s(joint, b);
                }

                if (part_axis(cache.rightUpperArm, t, b))
                {
                    Vector3 joint = t;
                    if (torso_ok)
                        joint = point_at_height(t, b, shoulder_c);
                    if (torso_ok)
                        line_w2s(shoulder_c, joint);
                    line_w2s(joint, b);
                }

                if (part_axis(cache.leftUpperLeg, t, b))
                {
                    if (torso_ok)
                        line_w2s(torso_bot, t);
                    line_w2s(t, b);
                }

                if (part_axis(cache.rightUpperLeg, t, b))
                {
                    if (torso_ok)
                        line_w2s(torso_bot, t);
                    line_w2s(t, b);
                }
            }

            else
            {
                auto bone = [&](const std::shared_ptr<Instance>& pa,
                                const std::shared_ptr<Instance>& pb) {
                    Vector3 a, b;
                    if (part_pos(pa, a) && part_pos(pb, b))
                        line_w2s(a, b);
                };

                float shoulder_drop15 = 0.15f;

                Vector3 ut_top, ut_bot;
                bool ut_ok = part_axis(cache.upperTorso, ut_top, ut_bot);
                Vector3 shoulder_c{};
                if (ut_ok)
                    shoulder_c = lerp3(ut_top, ut_bot, shoulder_drop15);

                auto shoulder_bone = [&](const std::shared_ptr<Instance>& arm) {
                    Vector3 a_top, a_bot, a_c;
                    if (ut_ok && part_axis(arm, a_top, a_bot) && part_pos(arm, a_c))
                    {
                        Vector3 joint = point_at_height(a_top, a_bot, shoulder_c);
                        line_w2s(shoulder_c, joint);
                        line_w2s(joint, a_c);
                    }

                    else
                    {
                        bone(cache.upperTorso, arm);
                    }
                };

                bone(cache.head,          cache.upperTorso);
                bone(cache.upperTorso,    cache.lowerTorso);

                shoulder_bone(cache.leftUpperArm);
                bone(cache.leftUpperArm,  cache.leftLowerArm);
                bone(cache.leftLowerArm,  cache.leftHand);

                shoulder_bone(cache.rightUpperArm);
                bone(cache.rightUpperArm, cache.rightLowerArm);
                bone(cache.rightLowerArm, cache.rightHand);

                bone(cache.lowerTorso,    cache.leftUpperLeg);
                bone(cache.leftUpperLeg,  cache.leftLowerLeg);
                bone(cache.leftLowerLeg,  cache.leftFoot);

                bone(cache.lowerTorso,    cache.rightUpperLeg);
                bone(cache.rightUpperLeg, cache.rightLowerLeg);
                bone(cache.rightLowerLeg, cache.rightFoot);
            }
            } // funny skeleton (линии)
        }

        // fill поверх chams/skel/hat — рамку и текст не трогаем (уже выше по z нет, рамку дорисуем)
        if (Cheat::g_Settings.esp.box_fill && st.box)
        {
            const auto& bf = Cheat::g_Settings.esp;
            if (bf.box_fill_mode == 1)
            {
                int img = bf.box_fill_image;
                if (img < 0) img = 0;
                if (img >= Cheat::Visuals::BoxFill::k_fill_image_count)
                    img = Cheat::Visuals::BoxFill::k_fill_image_count - 1;
                Cheat::Visuals::BoxFill::Draw(draw_list, img,
                    ImVec2(bx1, by1), ImVec2(bx2, by2), bf.box_fill_image_alpha, false);
            }

            else
            {
                const float* fc = bf.box_fill_color;
                const ImU32 fill_col = IM_COL32(
                    (int)(fc[0] * 255), (int)(fc[1] * 255),
                    (int)(fc[2] * 255), (int)(fc[3] * 255));
                draw_list->AddRectFilled(ImVec2(bx1, by1), ImVec2(bx2, by2), fill_col);
            }

            // рамка сверху fill (текст уже нарисован — он вне/сбоку бокса)
            const float* bc = st.box_color;
            ImU32 box_color = aim_target ? aim_red : IM_COL32(
                (int)(bc[0] * 255), (int)(bc[1] * 255),
                (int)(bc[2] * 255), (int)(bc[3] * 255));
            const float box_t = Cheat::g_Settings.esp.box_thickness;
            const bool box_ol = Cheat::g_Settings.esp.esp_outline[
                Cheat::Settings::OUTLINE_BOX];
            const int box_mode = Cheat::g_Settings.esp.box_mode;
            if (box_mode == 1)
                DrawCornerBox(draw_list, ImVec2(bx1, by1), ImVec2(bx2, by2),
                              box_color, box_t, box_ol);

            else if (box_mode != 2)
                DrawBox(draw_list, ImVec2(bx1, by1), ImVec2(bx2, by2),
                        box_color, box_t, box_ol);
        }
    });

    Visuals::HavocWorldEsp::Render(
        draw_list, esp_font, esp_fs, vm, viewport, cam_pos,
        overlay_w, overlay_h, g_esp_scale_x, g_esp_scale_y);

    draw_list->Flags = backup_flags;
}
