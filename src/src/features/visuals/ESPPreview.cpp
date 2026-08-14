#include "pch.h"
#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ESPPreview.h"
#include "EspLayout.h"
#include "PreviewRenderer.h"
#include "ShaderChams.h"
#include "features/visuals/boxfill/BoxFill.h"
#include "preview_model/preview_model_obj.h"
#include "preview_model/preview_model_texture.h"
#include "app/Graphics.h"
#include "app/Settings.h"
#include "gui/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include "gui/widgets/widgets.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <utility>
#include <vector>

namespace {
Cheat::Core::PreviewRenderer g_Renderer;
float g_LastTime = 0.0f;
bool g_InitDone = false;

enum PreviewElem : int {
    ElemNone = -1,
    ElemName = 0,
    ElemDistance,
    ElemTool,
    ElemFlags,
    ElemHealthText,
    ElemHealthBar,
    ElemCount
};

struct ElemHit {
    int id{ ElemNone };
    ImRect rect{};
};

std::vector<ElemHit> g_PrevHits;
EspLayout::Box g_PrevBox{};
bool g_HasPrevBox = false;
int g_DragElem = ElemNone;
bool g_LayoutDrag = false;
float g_DragTextH = 13.f;
ImVec2 g_DragGrab{};
ImVec2 g_DragVisualPos{};
bool g_DragVisualInit = false;

int g_DragPart = -1;
ImVec2 g_PartVisual{};
bool g_PartVisualInit = false;
bool g_PartMoved = false;

int* SidePtrFor(int id, Cheat::Settings& s)
{
    switch (id) {
    case ElemName:       return &s.esp.name_side;
    case ElemDistance:   return &s.esp.distance_side;
    case ElemTool:       return &s.esp.tool_side;
    case ElemFlags:      return &s.esp.flags_side;
    case ElemHealthText: return &s.esp.health_text_side;
    case ElemHealthBar:  return &s.esp.healthbar_side;
    default:             return nullptr;
    }
}

float* OffPtrFor(int id, Cheat::Settings& s)
{
    switch (id) {
    case ElemName:       return &s.esp.name_off;
    case ElemDistance:   return &s.esp.distance_off;
    case ElemTool:       return &s.esp.tool_off;
    case ElemFlags:      return &s.esp.flags_off;
    case ElemHealthText: return &s.esp.health_text_off;
    default:             return nullptr;
    }
}

ImRect HitRectFor(int id)
{
    for (const auto& h : g_PrevHits) {
        if (h.id == id) return h.rect;
    }
    return {};
}

const char* ElemLabel(int id)
{
    switch (id) {
    case ElemName:       return "name";
    case ElemDistance:   return "distance";
    case ElemTool:       return "tool";
    case ElemFlags:      return "flags";
    case ElemHealthText: return "health text";
    case ElemHealthBar:  return "health bar";
    default:             return "";
    }
}

void DrawSideHighlight(ImDrawList* dl, const EspLayout::Box& b, int side, ImU32 col)
{
    float t = 2.f;
    switch (side)
    {
    case EspLayout::Top:
        dl->AddRectFilled(ImVec2(b.x1, b.y1 - t), ImVec2(b.x2, b.y1 + 1.f), col);
        break;
    case EspLayout::Bottom:
        dl->AddRectFilled(ImVec2(b.x1, b.y2 - 1.f), ImVec2(b.x2, b.y2 + t), col);
        break;
    case EspLayout::Left:
        dl->AddRectFilled(ImVec2(b.x1 - t, b.y1), ImVec2(b.x1 + 1.f, b.y2), col);
        break;
    case EspLayout::Right:
    default:
        dl->AddRectFilled(ImVec2(b.x2 - 1.f, b.y1), ImVec2(b.x2 + t, b.y2), col);
        break;
    }
}

int HitTestElems(const ImVec2& mouse)
{
    for (int i = (int)g_PrevHits.size() - 1; i >= 0; --i) {
        if (g_PrevHits[i].rect.Contains(mouse))
            return g_PrevHits[i].id;
    }
    return ElemNone;
}

ImU32 Col(const float c[4])
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
}

inline float Floor(float v) { return std::floor(v); }

constexpr int k_box_edges[12][2] = {
    {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},
    {2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
};

std::vector<ImVec2> ConvexHull(std::vector<ImVec2> pts)
{
    if (pts.size() < 3) return pts;
    std::sort(pts.begin(), pts.end(), [](const ImVec2& a, const ImVec2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    auto cross = [](const ImVec2& o, const ImVec2& a, const ImVec2& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    std::vector<ImVec2> hull(pts.size() * 2);
    int k = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) k--;
        hull[k++] = pts[i];
    }
    for (int i = (int)pts.size() - 2, t = k + 1; i >= 0; --i) {
        while (k >= t && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) k--;
        hull[k++] = pts[i];
    }
    hull.resize(k > 0 ? k - 1 : 0);
    return hull;
}

bool SegInsidePoly(const ImVec2& a, const ImVec2& b,
                   const std::vector<ImVec2>& poly, float& out_t0, float& out_t1)
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
        float f0 = s * side(a), f1 = s * side(b), df = f1 - f0;
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

std::vector<ImVec2> ClipHalfPlane(const std::vector<ImVec2>& poly,
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

void SubtractPoly(std::vector<ImVec2> piece, const std::vector<ImVec2>& B,
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

void DrawSegmentOutsideUnion(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                             const std::vector<std::vector<ImVec2>>& polys,
                             int skip, ImU32 color)
{
    std::vector<std::pair<float, float>> covered;
    for (int i = 0; i < (int)polys.size(); ++i) {
        if (i == skip) continue;
        float t0, t1;
        if (SegInsidePoly(a, b, polys[i], t0, t1))
            covered.emplace_back(t0, t1);
    }
    auto lerp_pt = [&](float t) {
        return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    };
    if (covered.empty()) {
        dl->AddLine(a, b, color, 1.0f);
        return;
    }
    std::sort(covered.begin(), covered.end());
    std::vector<std::pair<float, float>> merged;
    for (auto& seg : covered) {
        if (merged.empty() || seg.first > merged.back().second)
            merged.push_back(seg);
        else
            merged.back().second = (std::max)(merged.back().second, seg.second);
    }
    float cur = 0.0f;
    for (auto& m : merged) {
        if (m.first > cur)
            dl->AddLine(lerp_pt(cur), lerp_pt(m.first), color, 1.0f);
        cur = (std::max)(cur, m.second);
    }
    if (cur < 1.0f)
        dl->AddLine(lerp_pt(cur), b, color, 1.0f);
}

void DrawPreviewChams(ImDrawList* dl,
                      const std::vector<std::array<ImVec2, 8>>& parts,
                      int mode, int shader,
                      ImU32 outline_col, ImU32 fill_col)
{
    if (parts.empty())
        return;

    if (mode == 0)
    {
        for (const auto& pc : parts)
            for (const auto& e : k_box_edges)
                dl->AddLine(pc[e[0]], pc[e[1]], outline_col, 1.0f);
        return;
    }

    if (mode == 1)
    {
        for (const auto& pc : parts)
        {
            std::vector<ImVec2> pts(pc.begin(), pc.end());
            auto hull = ConvexHull(std::move(pts));
            if (hull.size() >= 3)
                dl->AddConvexPolyFilled(hull.data(), (int)hull.size(), fill_col);
        }
        for (const auto& pc : parts)
            for (const auto& e : k_box_edges)
                dl->AddLine(pc[e[0]], pc[e[1]], outline_col, 1.0f);
        return;
    }

    std::vector<std::vector<ImVec2>> hulls;
    hulls.reserve(parts.size());
    for (const auto& pc : parts)
    {
        std::vector<ImVec2> pts(pc.begin(), pc.end());
        auto h = ConvexHull(std::move(pts));
        if (h.size() >= 3)
            hulls.push_back(std::move(h));
    }

    std::vector<std::vector<ImVec2>> clipped;
    clipped.reserve(hulls.size() * 2);
    for (int i = 0; i < (int)hulls.size(); ++i)
    {
        std::vector<std::vector<ImVec2>> pieces{ hulls[i] };
        for (int j = 0; j < i && !pieces.empty(); ++j)
        {
            std::vector<std::vector<ImVec2>> next;
            for (auto& piece : pieces)
                SubtractPoly(std::move(piece), hulls[j], next);
            pieces = std::move(next);
        }
        for (auto& piece : pieces)
        {
            if (piece.size() >= 3)
                clipped.push_back(std::move(piece));
        }
    }

    if (mode == 3)
    {
        Cheat::Visuals::ShaderChams::DrawFill(dl, clipped, (float)ImGui::GetTime(),
                                              shader, false, false);
        ImU32 shader_outline = Cheat::Visuals::ShaderChams::OutlineColor(shader, false);
        for (int i = 0; i < (int)hulls.size(); ++i)
        {
            const auto& hull = hulls[i];
            int n = (int)hull.size();
            for (int e = 0; e < n; ++e)
                DrawSegmentOutsideUnion(dl, hull[e], hull[(e + 1) % n], hulls, i, shader_outline);
        }
    }

    else
    {
        ImDrawListFlags backup = dl->Flags;
        dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        for (const auto& piece : clipped)
            dl->AddConvexPolyFilled(piece.data(), (int)piece.size(), fill_col);
        dl->Flags = backup;
        for (int i = 0; i < (int)hulls.size(); ++i)
        {
            const auto& hull = hulls[i];
            int n = (int)hull.size();
            for (int e = 0; e < n; ++e)
                DrawSegmentOutsideUnion(dl, hull[e], hull[(e + 1) % n], hulls, i, outline_col);
        }
    }
}

void SnapEspBox(float min_x, float min_y, float max_x, float max_y,
                float& x1, float& y1, float& x2, float& y2)
{
    x1 = Floor(min_x);
    y1 = Floor(min_y);
    x2 = std::ceil(max_x);
    y2 = std::ceil(max_y);
    if (x2 <= x1) x2 = x1 + 1.0f;
    if (y2 <= y1) y2 = y1 + 1.0f;
}

void DrawPlainBox(ImDrawList* dl, ImVec2 tl, ImVec2 br, ImU32 color, float thick, bool outline)
{
    float x1, y1, x2, y2;
    SnapEspBox(tl.x, tl.y, br.x, br.y, x1, y1, x2, y2);
    if (thick < 0.5f) thick = 0.5f;
    if (outline) {
        if (thick <= 1.01f) {
            dl->AddRect(ImVec2(x1 - 1, y1 - 1), ImVec2(x2 + 1, y2 + 1), IM_COL32(0, 0, 0, 255));
            dl->AddRect(ImVec2(x1 + 1, y1 + 1), ImVec2(x2 - 1, y2 - 1), IM_COL32(0, 0, 0, 255));
        } else {
            dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(0, 0, 0, 255), 0.0f, 0, thick + 2.0f);
        }
    }
    dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), color, 0.0f, 0, thick);
}

void DrawCornerBox(ImDrawList* dl, ImVec2 tl, ImVec2 br, ImU32 color, float thick, bool outline)
{
    float x1, y1, x2, y2;
    SnapEspBox(tl.x, tl.y, br.x, br.y, x1, y1, x2, y2);
    if (thick < 0.5f) thick = 0.5f;
    float lw = Floor((x2 - x1) * 0.25f);
    float lh = Floor((y2 - y1) * 0.25f);
    if (lw < 2.f) lw = 2.f;
    if (lh < 2.f) lh = 2.f;
    ImU32 black = IM_COL32(0, 0, 0, 255);
    auto seg = [&](float ax, float ay, float bx, float by) {
        if (outline)
            dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), black, thick + 2.0f);
        dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), color, thick);
    };
    seg(x1, y1, x1 + lw, y1); seg(x1, y1, x1, y1 + lh);
    seg(x2 - lw, y1, x2, y1); seg(x2, y1, x2, y1 + lh);
    seg(x1, y2 - lh, x1, y2); seg(x1, y2, x1 + lw, y2);
    seg(x2 - lw, y2, x2, y2); seg(x2, y2 - lh, x2, y2);
}

void DrawSkeletonLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 color, float thick, bool outline)
{
    if (thick < 1.0f) thick = 1.0f;
    if (outline)
        dl->AddLine(a, b, IM_COL32(0, 0, 0, 255), thick + 2.0f);
    dl->AddLine(a, b, color, thick);
}

ImU32 TierDotColor(int tier, int alpha)
{
    if (tier == Cheat::Settings::PART_PRIMARY)
        return IM_COL32(90, 160, 255, alpha);
    if (tier == Cheat::Settings::PART_SECONDARY)
        return IM_COL32(100, 220, 130, alpha);
    if (tier == Cheat::Settings::PART_TERTIARY)
        return IM_COL32(245, 210, 80, alpha);
    return IM_COL32(0, 0, 0, 0);
}

void DrawFadeDot(ImDrawList* dl, const ImVec2& c, int tier, bool hover)
{
    if (tier == Cheat::Settings::PART_OFF) return;

    const float glow = hover ? 11.0f : 9.0f;
    const float core = hover ? 2.4f : 2.0f;
    const int steps = 14;
    for (int i = steps; i >= 1; --i) {
        const float t = (float)i / (float)steps;
        const float r = core + (glow - core) * t;
        const float falloff = (1.0f - t) * (1.0f - t);
        const int a = (int)((hover ? 42.0f : 28.0f) * falloff);
        if (a <= 0) continue;
        dl->AddCircleFilled(c, r, TierDotColor(tier, a), 32);
    }
    dl->AddCircleFilled(c, core, TierDotColor(tier, hover ? 160 : 110), 24);
    dl->AddCircleFilled(c, core * 0.45f, TierDotColor(tier, hover ? 210 : 150), 16);
}
}

namespace Cheat::Visuals {

// грузим модельку для превью
void ESPPreview::Initialize()
{
    if (g_InitDone)
        return;
    if (!Cheat::Core::g_Device || !Cheat::Core::g_DeviceContext)
        return;

    if (!g_Renderer.Initialize(Cheat::Core::g_Device, Cheat::Core::g_DeviceContext, 600, 900))
        return;

    if (g_PreviewModelOBJSize > 0)
        g_Renderer.LoadModelFromMemory(g_PreviewModelOBJData, g_PreviewModelOBJSize);
    if (g_PreviewModelTextureSize > 0)
        g_Renderer.LoadTextureFromMemory(g_PreviewModelTexture, g_PreviewModelTextureSize);

    g_InitDone = true;
}

void ESPPreview::Shutdown()
{
    g_Renderer.Shutdown();
    g_InitDone = false;
}

// превью есп в меню, драги тут
void ESPPreview::Render()
{
    if (!g_InitDone) Initialize();

    auto& s = Cheat::g_Settings;
    float now = (float)ImGui::GetTime();
    float dt = now - g_LastTime;
    if (dt > 0.1f || dt < 0.0f) dt = 0.016f;
    g_LastTime = now;

    ImFont* font = fonts::ui();
    const float fs = fonts::ui_size(font);
    ImFont* esp_font = fonts::selected();
    if (!esp_font) esp_font = ImGui::GetFont();
    const float esp_fs = fonts::snap_px(s.esp.font_size);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();

    float hintH = fs * 2.0f + 8.0f;
    float modelH = avail.y - hintH;
    if (modelH < 1.f)
        modelH = 1.f;

    ImGui::InvisibleButton("##preview_drag", ImVec2(avail.x, modelH));
    bool hovered = ImGui::IsItemHovered();
    static bool s_dragged = false;
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
        g_Renderer.SetAutoSpin(!g_Renderer.IsAutoSpinning());

    auto find_aim_part_at = [&](const ImVec2& mouse, float hit_r) -> int {
        std::vector<std::pair<int, std::pair<float, float>>> centers;
        if (!g_Renderer.GetProjectedAimPartCenters(centers))
            return -1;
        float imgW = avail.x;
        float imgH = modelH;
        auto UV = [&](float u, float v) {
            return ImVec2(origin.x + u * imgW, origin.y + v * imgH);
        };
        int best = -1;
        float best_d2 = hit_r * hit_r;
        for (const auto& entry : centers)
        {
            int part = entry.first;
            if (part < 0 || part >= Cheat::Settings::AIM_PART_COUNT)
                continue;
            ImVec2 c = UV(entry.second.first, entry.second.second);
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
    };

    if (ImGui::IsItemActivated())
    {
        s_dragged = false;
        g_LayoutDrag = false;
        g_DragVisualInit = false;
        g_DragPart = -1;
        g_PartVisualInit = false;
        g_PartMoved = false;
        g_DragElem = HitTestElems(io.MousePos);
        if (g_DragElem != ElemNone)
        {
            g_LayoutDrag = true;
            ImRect hr = HitRectFor(g_DragElem);
            g_DragGrab = ImVec2(io.MousePos.x - hr.Min.x, io.MousePos.y - hr.Min.y);
            g_DragTextH = hr.GetHeight() - 4.f;
            if (g_DragTextH < 8.f)
                g_DragTextH = 8.f;
            g_DragVisualPos = hr.Min;
            g_DragVisualInit = true;
        }

        else
        {
            g_DragPart = find_aim_part_at(io.MousePos, 18.f);
            if (g_DragPart >= 0)
            {
                g_PartVisual = io.MousePos;
                g_PartVisualInit = true;
            }
        }
    }

    if (g_LayoutDrag && g_DragElem != ElemNone)
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f))
            s_dragged = true;

        if (s_dragged)
        {
            ImVec2 target(io.MousePos.x - g_DragGrab.x, io.MousePos.y - g_DragGrab.y);
            if (!g_DragVisualInit)
            {
                g_DragVisualPos = target;
                g_DragVisualInit = true;
            }

            else
            {
                float k = 1.f - std::exp(-18.f * io.DeltaTime);
                g_DragVisualPos.x += (target.x - g_DragVisualPos.x) * k;
                g_DragVisualPos.y += (target.y - g_DragVisualPos.y) * k;
            }

            if (g_HasPrevBox && g_DragElem != ElemHealthBar)
            {
                int side = EspLayout::NearestSide(
                    g_DragVisualPos.x + 8.f, g_DragVisualPos.y + g_DragTextH * 0.5f, g_PrevBox);
                if (int* sp = SidePtrFor(g_DragElem, s))
                    *sp = side;
                if (float* op = OffPtrFor(g_DragElem, s))
                {
                    *op = EspLayout::OffsetFromPos(side, g_DragVisualPos.x, g_DragVisualPos.y,
                                                   g_DragTextH, g_PrevBox, 2.f);
                }
                EspLayout::ResolveAllSides(s, esp_fs, 1, g_DragElem);
            }

            else if (g_HasPrevBox && g_DragElem == ElemHealthBar)
            {
                int side = EspLayout::ClampHealthBarSide(
                    EspLayout::NearestSide(io.MousePos.x, io.MousePos.y, g_PrevBox));
                if (int* sp = SidePtrFor(g_DragElem, s))
                    *sp = side;
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if (g_DragElem != ElemHealthBar)
                EspLayout::ResolveAllSides(s, esp_fs, 1, -1);
            g_DragElem = ElemNone;
            g_LayoutDrag = false;
            g_DragVisualInit = false;
        }
    }

    else if (g_DragPart >= 0)
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
        {
            s_dragged = true;
            g_PartMoved = true;
        }

        if (g_PartMoved)
        {
            ImVec2 target = io.MousePos;
            if (!g_PartVisualInit)
            {
                g_PartVisual = target;
                g_PartVisualInit = true;
            }

            else
            {
                float k = 1.f - std::exp(-20.f * io.DeltaTime);
                g_PartVisual.x += (target.x - g_PartVisual.x) * k;
                g_PartVisual.y += (target.y - g_PartVisual.y) * k;
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            auto& acfg = Cheat::g_Settings.aim.active();
            if (g_PartMoved)
            {
                int drop = find_aim_part_at(g_PartVisual, 22.f);
                if (drop >= 0 && drop != g_DragPart)
                {
                    std::swap(acfg.part_tier[g_DragPart], acfg.part_tier[drop]);
                    acfg.SyncPartsFromTiers();
                }
            }

            else if (hovered && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                int& tier = acfg.part_tier[g_DragPart];
                tier = (tier + 1) % 4;
                acfg.SyncPartsFromTiers();
            }

            g_DragPart = -1;
            g_PartMoved = false;
            g_PartVisualInit = false;
        }
    }

    else if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f))
    {
        s_dragged = true;
        g_Renderer.SetAutoSpin(false);
        g_Renderer.AddRotationDelta(io.MouseDelta.x * 0.007f, io.MouseDelta.y * 0.007f);
        g_Renderer.NotifyManualInput();
    }

    if (hovered && io.MouseWheel != 0.0f)
        g_Renderer.AddZoom(io.MouseWheel * 0.045f);

    const ImVec2 imgMin = origin;
    const ImVec2 imgMax(origin.x + avail.x, origin.y + modelH);

    if (!g_Renderer.IsReady()) {
        const char* msg = "preview model missing";
        const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, msg);
        dl->AddText(font, fs,
            ImVec2(imgMin.x + (avail.x - tsz.x) * 0.5f, imgMin.y + modelH * 0.45f),
            colors::text_inactive_u32(), msg);
    }

    else
    {
        g_Renderer.Update(dt);
        dl->AddImage((ImTextureID)g_Renderer.GetTextureID(), imgMin, imgMax);

        float u0, v0, u1, v1;
        if (g_Renderer.GetProjectedUVBounds(u0, v0, u1, v1)) {
            const float imgW = imgMax.x - imgMin.x;
            const float imgH = imgMax.y - imgMin.y;
            auto UV = [&](float u, float v) {
                return ImVec2(imgMin.x + u * imgW, imgMin.y + v * imgH);
            };
            const ImVec2 boxMin = UV(u0, v0);
            const ImVec2 boxMax = UV(u1, v1);
            float bx1, by1, bx2, by2;
            SnapEspBox(boxMin.x, boxMin.y, boxMax.x, boxMax.y, bx1, by1, bx2, by2);

            float hp = 72.f;
            float max_hp = 100.f;
            float dist = 26.f;
            float hp_frac = hp / max_hp;

            const EspLayout::Box ebox{ bx1, by1, bx2, by2 };
            g_PrevBox = ebox;
            g_HasPrevBox = true;
            std::vector<ElemHit> hits;
            hits.reserve(8);

            if (g_LayoutDrag && g_DragElem != ElemNone && g_HasPrevBox) {
                int preview_side = EspLayout::NearestSide(io.MousePos.x, io.MousePos.y, ebox);
                if (g_DragElem == ElemHealthBar)
                    preview_side = EspLayout::ClampHealthBarSide(preview_side);
                DrawSideHighlight(dl, ebox, preview_side, IM_COL32(90, 180, 255, 200));
            }

            if (s.esp.chams) {
                std::vector<std::array<std::pair<float, float>, 8>> uv_boxes;
                if (g_Renderer.GetProjectedPartBoxes(uv_boxes)) {
                    std::vector<std::array<ImVec2, 8>> parts;
                    parts.reserve(uv_boxes.size());
                    for (const auto& box : uv_boxes) {
                        std::array<ImVec2, 8> pc{};
                        for (int i = 0; i < 8; ++i)
                            pc[i] = UV(box[i].first, box[i].second);
                        parts.push_back(pc);
                    }
                    // mesh preview ≈ filled; engine отдельно в мире
                    const int preview_mode = (s.esp.chams_mode == 4) ? 1
                        : s.esp.chams_mode;
                    const int preview_shader = s.esp.chams_shader;
                    DrawPreviewChams(dl, parts, preview_mode, preview_shader,
                        Col(s.esp.chams_outline_color), Col(s.esp.chams_fill_color));
                }
            }

            // skel/chams сначала, fill потом сверху
            if (s.esp.skeleton) {
                const int skel_type = s.esp.skeleton_type;
                if (skel_type >= 1 && skel_type <= 3) {
                    int img = Cheat::Visuals::BoxFill::SK;
                    if (skel_type == 2)
                        img = Cheat::Visuals::BoxFill::US;
                    else if (skel_type == 3)
                        img = Cheat::Visuals::BoxFill::SE;
                    Cheat::Visuals::BoxFill::Draw(dl, img,
                        ImVec2(bx1, by1), ImVec2(bx2, by2),
                        s.esp.skeleton_color[3], true);
                } else {
                    std::vector<float> segs;
                    if (g_Renderer.GetProjectedR6Skeleton(segs)) {
                        const ImU32 bone = Col(s.esp.skeleton_color);
                        const float skel_t = s.esp.skeleton_thickness;
                        const bool skel_ol = s.esp.esp_outline[
                            Cheat::Settings::OUTLINE_SKELETON];
                        for (size_t i = 0; i + 3 < segs.size(); i += 4) {
                            DrawSkeletonLine(dl,
                                UV(segs[i], segs[i + 1]),
                                UV(segs[i + 2], segs[i + 3]),
                                bone, skel_t, skel_ol);
                        }
                    }
                }
            }

            if (s.esp.box_fill && s.esp.box) {
                if (s.esp.box_fill_mode == 1) {
                    int img = s.esp.box_fill_image;
                    if (img < 0) img = 0;
                    if (img >= Cheat::Visuals::BoxFill::k_fill_image_count)
                        img = Cheat::Visuals::BoxFill::k_fill_image_count - 1;
                    Cheat::Visuals::BoxFill::Draw(dl, img, ImVec2(bx1, by1), ImVec2(bx2, by2),
                        s.esp.box_fill_image_alpha, false);
                } else {
                    dl->AddRectFilled(ImVec2(bx1, by1), ImVec2(bx2, by2), Col(s.esp.box_fill_color));
                }
            }

            if (s.esp.box) {
                const float box_t = s.esp.box_thickness;
                const bool box_ol = s.esp.esp_outline[Cheat::Settings::OUTLINE_BOX];
                if (s.esp.box_mode == 1)
                    DrawCornerBox(dl, ImVec2(bx1, by1), ImVec2(bx2, by2),
                                  Col(s.esp.box_color), box_t, box_ol);
                else
                    DrawPlainBox(dl, ImVec2(bx1, by1), ImVec2(bx2, by2),
                                 Col(s.esp.box_color), box_t, box_ol);
            }

            // если не драгаем компактим слоты (дыры забиваем)
            if (!(g_LayoutDrag && s_dragged && g_DragElem != ElemHealthBar))
                EspLayout::ResolveAllSides(s, esp_fs, 1, -1);

            float bar_w = 2.f;
            float bar_gap = 3.f;
            float pad = 2.f;
            float pad_l = pad + 2.f;
            float pad_r = pad + 2.f;

            if (s.esp.healthbar)
            {
                int hb_side = EspLayout::ClampHealthBarSide(s.esp.healthbar_side);
                float bar_x1, bar_y1, bar_x2, bar_y2;
                EspLayout::PlaceHealthBar(hb_side, ebox, bar_w, bar_gap,
                                          bar_x1, bar_y1, bar_x2, bar_y2);
                if (hb_side == EspLayout::Right)
                    pad_r += bar_w + bar_gap + 1.0f;

                else
                    pad_l += bar_w + bar_gap + 1.0f;

                const float bar_h = bar_y2 - bar_y1;
                const float fill_h = Floor(bar_h * hp_frac + 0.5f);
                const float fill_top = bar_y2 - fill_h;

                dl->AddRectFilled(ImVec2(bar_x1 - 1.0f, bar_y1 - 1.0f),
                                  ImVec2(bar_x2 + 1.0f, bar_y2 + 1.0f),
                                  IM_COL32(0, 0, 0, 255));
                const ImU32 hp_col = IM_COL32((int)(255 * (1.0f - hp_frac)),
                                              (int)(255 * hp_frac), 0, 255);
                if (fill_h > 0.0f)
                    dl->AddRectFilled(ImVec2(bar_x1, fill_top), ImVec2(bar_x2, bar_y2), hp_col);

                const ImRect hb_hit(ImVec2(bar_x1 - 4.0f, bar_y1 - 2.0f),
                                    ImVec2(bar_x2 + 4.0f, bar_y2 + 2.0f));
                const bool hb_active = (g_DragElem == ElemHealthBar);
                if (hb_active || (hovered && HitTestElems(io.MousePos) == ElemHealthBar && g_DragElem == ElemNone))
                    dl->AddRect(hb_hit.Min, hb_hit.Max, IM_COL32(90, 180, 255, hb_active ? 220 : 120));
                hits.push_back({ ElemHealthBar, hb_hit });
            }

            auto place_draw = [&](int id, int side, float offset, const char* text, ImU32 col) {
                const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, text);
                const int sidx = (side < 0 || side > 3) ? 0 : side;
                float tx, ty;
                EspLayout::PlaceText(sidx, ebox, offset, tsz.x, tsz.y,
                                     pad_l, pad_r, pad, tx, ty);
                const bool active = (g_DragElem == id && s_dragged);
                ImVec2 pos = active ? g_DragVisualPos : ImVec2(Floor(tx), Floor(ty));
                if (active)
                    g_DragTextH = tsz.y;
                widgets::draw_outlined_text(dl, esp_font, esp_fs, pos, col, text);
                if (active || (hovered && HitTestElems(io.MousePos) == id && g_DragElem == ElemNone)) {
                    dl->AddRect(ImVec2(pos.x - 2.0f, pos.y - 1.0f),
                                ImVec2(pos.x + tsz.x + 2.0f, pos.y + tsz.y + 1.0f),
                                IM_COL32(90, 180, 255, active ? 220 : 120));
                }
                hits.push_back({ id, ImRect(ImVec2(pos.x - 3.0f, pos.y - 2.0f),
                                            ImVec2(pos.x + tsz.x + 3.0f, pos.y + tsz.y + 2.0f)) });
            };

            if (s.esp.name) {
                const char* name = s.esp.name_mode == 0 ? "Big Yahu" : "big_yahu228";
                place_draw(ElemName, s.esp.name_side, s.esp.name_off, name, Col(s.esp.name_color));
            }

            if (s.esp.distance) {
                char dist_buf[32];
                if (s.esp.distance_unit == 1)
                    std::snprintf(dist_buf, sizeof(dist_buf), "%.0fm", dist * 0.28f);
                else
                    std::snprintf(dist_buf, sizeof(dist_buf), "%.0f studs", dist);
                place_draw(ElemDistance, s.esp.distance_side, s.esp.distance_off, dist_buf,
                           Col(s.esp.distance_color));
            }

            if (s.esp.tool) {
                place_draw(ElemTool, s.esp.tool_side, s.esp.tool_off, "[ClassicSword]",
                           Col(s.esp.tool_color));
            }

            if (s.esp.health_text) {
                const bool with_bar = s.esp.healthbar;
                char hp_buf[16];
                if (with_bar)
                    std::snprintf(hp_buf, sizeof(hp_buf), "%.0f", hp);
                else
                    std::snprintf(hp_buf, sizeof(hp_buf), "%.0f hp", hp);
                place_draw(ElemHealthText, s.esp.health_text_side, s.esp.health_text_off,
                           hp_buf, IM_COL32(255, 255, 255, 255));
            }

            g_PrevHits = std::move(hits);

            {
                auto& acfg = Cheat::g_Settings.aim.active();
                std::vector<std::pair<int, std::pair<float, float>>> centers;
                if (g_Renderer.GetProjectedAimPartCenters(centers)) {
                    const ImVec2 mouse = io.MousePos;
                    float hit_r = 18.f;
                    int best_hover = -1;
                    float best_d2 = hit_r * hit_r;

                    for (const auto& entry : centers) {
                        const int part = entry.first;
                        if (part < 0 || part >= Cheat::Settings::AIM_PART_COUNT) continue;
                        const ImVec2 c = UV(entry.second.first, entry.second.second);
                        const float dx = mouse.x - c.x;
                        const float dy = mouse.y - c.y;
                        const float d2 = dx * dx + dy * dy;
                        if (hovered && d2 < best_d2) {
                            best_d2 = d2;
                            best_hover = part;
                        }
                    }

                    const int drop_target = (g_DragPart >= 0 && g_PartMoved)
                        ? find_aim_part_at(g_PartVisual, 22.f) : -1;

                    for (const auto& entry : centers) {
                        const int part = entry.first;
                        if (part < 0 || part >= Cheat::Settings::AIM_PART_COUNT) continue;
                        const int tier = acfg.part_tier[part];
                        if (tier == Cheat::Settings::PART_OFF && part != g_DragPart) continue;

                        ImVec2 c = UV(entry.second.first, entry.second.second);
                        if (part == g_DragPart && g_PartMoved)
                            c = g_PartVisual;

                        const bool hover = (part == best_hover) || (part == drop_target && part != g_DragPart);
                        if (tier != Cheat::Settings::PART_OFF || part == g_DragPart)
                            DrawFadeDot(dl, c, tier == Cheat::Settings::PART_OFF ? Cheat::Settings::PART_TERTIARY : tier, hover);
                    }
                }
            }
        }
    }

    const float hy = origin.y + modelH + 2.0f;
    auto hint = [&](const char* t, float y) {
        const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, t);
        dl->AddText(font, fs, ImVec2(origin.x + (avail.x - tsz.x) * 0.5f, y),
                    colors::text_inactive_u32(), t);
    };
    hint("drag · click · scroll", hy);
    }

}
