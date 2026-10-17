#pragma once
// -----------------------------------------------------------------------------
// menu_icons.h - the rail's line icons, drawn as geometry (MATCHA_UI_SPEC 4.2).
//
// The reference's rail icons are 16 px line icons with ~1.7 px round-capped
// strokes. They used to come from the Font Awesome face the LiquidUI kit ships,
// which only exists when the vendored data folder sits next to the exe: when it
// is missing (or the glyph is), the row rendered with no icon at all - NPC and
// Teams did exactly that. Drawing the shapes makes the rail independent of the
// font files and byte-exact with the reference contract:
// src/docs/ui-reference/generate_reference_svg.py ("sidebar icons").
//
// Geometry is written in the reference's own units - a 16 x 16 box (half-extent
// 8) centred on the row - and scaled by `box / 16`, so a caller picks the size.
// Screen y grows downwards, exactly like the SVG the numbers came from.
// -----------------------------------------------------------------------------
#include "imgui.h"
#include <cmath>

namespace menu_icons
{
    enum class Kind
    {
        Spark,   // Aimbot   - 4-point sparkle: two opposed stroked diamonds
        Person,  // Silent / Character / NPC - head circle + shoulder arc
        Clock,   // Trigger  - watch face + two hands
        Eye,     // Visuals  - lens outline + pupil
        Globe,   // World    - sphere, equator and meridian
        Folder,  // folder outline
        Chat,    // speech bubble
        Flag,    // Teams    - pennant on a staff
        Sliders  // Options  - three rails with knobs
    };

    constexpr float k_half   = 8.f;    // reference half-extent (16 px box)
    constexpr float k_stroke = 1.7f;   // reference stroke weight at that box

    inline ImVec2 at(ImVec2 c, float k, float dx, float dy)
    {
        return ImVec2(c.x + dx * k, c.y + dy * k);
    }

    inline void poly(ImDrawList* dl, ImVec2 c, float k, const float* xy, int n,
                     bool closed, ImU32 col, float th)
    {
        ImVec2 pts[64];
        const int m = (n > 64) ? 64 : n;
        for (int i = 0; i < m; ++i)
            pts[i] = at(c, k, xy[i * 2], xy[i * 2 + 1]);
        dl->AddPolyline(pts, m, col, closed ? ImDrawFlags_Closed : ImDrawFlags_None, th);
    }

    // samples a cubic bezier into out[] (needs segs + 1 slots); returns the count
    inline int bezier(ImVec2* out, ImVec2 c, float k,
                      float x0, float y0, float x1, float y1,
                      float x2, float y2, float x3, float y3, int segs)
    {
        int n = 0;
        for (int i = 0; i <= segs; ++i)
        {
            const float t = (float)i / segs, u = 1.f - t;
            const float a = u * u * u, b = 3.f * u * u * t;
            const float d = 3.f * u * t * t, e = t * t * t;
            out[n++] = at(c, k,
                          a * x0 + b * x1 + d * x2 + e * x3,
                          a * y0 + b * y1 + d * y2 + e * y3);
        }
        return n;
    }

    // circular arc sampled as a line strip (radians, screen y-down)
    inline void arc(ImDrawList* dl, ImVec2 c, float k, float cx, float cy, float r,
                    float a0, float a1, int segs, ImU32 col, float th)
    {
        ImVec2 prev = at(c, k, cx + r * cosf(a0), cy + r * sinf(a0));
        for (int i = 1; i <= segs; ++i)
        {
            const float a = a0 + (a1 - a0) * (float)i / segs;
            const ImVec2 cur = at(c, k, cx + r * cosf(a), cy + r * sinf(a));
            dl->AddLine(prev, cur, col, th);
            prev = cur;
        }
    }

    // -------------------------------------------------------------------------
    // the rail's chevrons (profile card, config selector). The reference draws
    // them as a 10 px wide / 5 px tall stroke ("M387 45 l5 5 l5 -5"), so they
    // are geometry too rather than a glyph from the same optional icon font.
    // -------------------------------------------------------------------------
    inline void Chevron(ImDrawList* dl, bool right, ImVec2 c, float half,
                        ImU32 col, float th)
    {
        ImVec2 pts[3];
        if (right)
        {
            pts[0] = ImVec2(c.x - half * 0.5f, c.y - half);
            pts[1] = ImVec2(c.x + half * 0.5f, c.y);
            pts[2] = ImVec2(c.x - half * 0.5f, c.y + half);
        }
        else
        {
            pts[0] = ImVec2(c.x - half, c.y - half * 0.5f);
            pts[1] = ImVec2(c.x, c.y + half * 0.5f);
            pts[2] = ImVec2(c.x + half, c.y - half * 0.5f);
        }
        dl->AddPolyline(pts, 3, col, ImDrawFlags_None, th);
    }

    // -------------------------------------------------------------------------
    // the rail icon: `box` is the 16 px reference box, `thick` the stroke weight
    // at that box; both scale together with the caller's size
    // -------------------------------------------------------------------------
    inline void Draw(ImDrawList* dl, Kind kind, ImVec2 c, float box, ImU32 col,
                     float thick = -1.f)
    {
        const float k = box / (k_half * 2.f);
        const float th = (thick > 0.f ? thick : k_stroke) * k;

        switch (kind)
        {
        case Kind::Spark:
        {
            // two opposed 4-point diamonds (8 px half-extent, 0.32 px waist)
            const float a[8] = { 0.f, -k_half,  k_half * 0.32f, -k_half * 0.32f,
                                 k_half, 0.f,   k_half * 0.32f,  k_half * 0.32f };
            const float b[8] = { 0.f,  k_half, -k_half * 0.32f,  k_half * 0.32f,
                                -k_half, 0.f,  -k_half * 0.32f, -k_half * 0.32f };
            poly(dl, c, k, a, 4, true, col, th);
            poly(dl, c, k, b, 4, true, col, th);
            break;
        }

        case Kind::Person:
        {
            dl->AddCircle(at(c, k, 0.f, -3.f), 3.1f * k, col, 24, th);
            // shoulders: "a5.4 5.4 0 0 1 10.8 0" - an upward half circle
            arc(dl, c, k, 0.f, 6.4f, 5.4f, 3.14159265f, 6.28318531f, 18, col, th);
            break;
        }

        case Kind::Clock:
        {
            dl->AddCircle(at(c, k, 0.f, 0.f), 6.6f * k, col, 28, th);
            dl->AddLine(at(c, k, 0.f, 0.f), at(c, k, 0.f, -4.2f), col, th);
            dl->AddLine(at(c, k, 0.f, 0.f), at(c, k, 3.4f, 1.6f), col, th);
            break;
        }

        case Kind::Eye:
        {
            // M-7.4,0 C-3,-5.4 3,-5.4 7.4,0 C3,5.4 -3,5.4 -7.4,0 Z
            ImVec2 pts[34];
            int n = bezier(pts, c, k, -7.4f, 0.f, -3.0f, -5.4f, 3.0f, -5.4f, 7.4f, 0.f, 16);
            n += bezier(pts + n, c, k, 7.4f, 0.f, 3.0f, 5.4f, -3.0f, 5.4f, -7.4f, 0.f, 16);
            dl->AddPolyline(pts, n, col, ImDrawFlags_Closed, th);
            dl->AddCircle(at(c, k, 0.f, 0.f), 2.3f * k, col, 20, th);
            break;
        }

        case Kind::Globe:
        {
            dl->AddCircle(at(c, k, 0.f, 0.f), 6.8f * k, col, 32, th);
            dl->AddLine(at(c, k, -6.8f, 0.f), at(c, k, 6.8f, 0.f), col, th);
            // rx 3.0 / ry 6.8 meridian, sampled closed
            ImVec2 m[33];
            for (int i = 0; i <= 32; ++i)
            {
                const float a = 6.28318531f * (float)i / 32.f;
                m[i] = at(c, k, 3.0f * cosf(a), 6.8f * sinf(a));
            }
            dl->AddPolyline(m, 33, col, ImDrawFlags_Closed, th);
            break;
        }

        case Kind::Folder:
        {
            // M-7,-5.6 h4.2 l1.6 2.2 h6.6 a1.6,1.6 0 0 1 1.6,1.6 v5.4
            // a1.6,1.6 0 0 1 -1.6,1.6 h-12.4 a1.6,1.6 0 0 1 -1.6,-1.6 v-7.6
            // a1.6,1.6 0 0 1 1.6,-1.6 Z   (1.6 px corners sampled as arcs)
            const float r = 1.6f;
            const float head[8] = { -7.0f, -5.6f, -2.8f, -5.6f, -1.2f, -3.4f, 5.4f, -3.4f };
            poly(dl, c, k, head, 4, false, col, th);
            arc(dl, c, k, 5.4f, -1.8f, r, -1.57079633f, 0.f, 6, col, th);
            dl->AddLine(at(c, k, 7.0f, -1.8f), at(c, k, 7.0f, 3.6f), col, th);
            arc(dl, c, k, 5.4f, 3.6f, r, 0.f, 1.57079633f, 6, col, th);
            dl->AddLine(at(c, k, 5.4f, 5.2f), at(c, k, -7.0f, 5.2f), col, th);
            arc(dl, c, k, -7.0f, 3.6f, r, 1.57079633f, 3.14159265f, 6, col, th);
            dl->AddLine(at(c, k, -8.6f, 3.6f), at(c, k, -8.6f, -4.0f), col, th);
            arc(dl, c, k, -7.0f, -4.0f, r, 3.14159265f, 4.71238898f, 6, col, th);
            break;
        }

        case Kind::Chat:
        {
            const float r = 1.8f;
            dl->AddLine(at(c, k, -7.7f, -5.4f), at(c, k, 4.9f, -5.4f), col, th);
            arc(dl, c, k, 4.9f, -3.6f, r, -1.57079633f, 0.f, 6, col, th);
            dl->AddLine(at(c, k, 6.7f, -3.6f), at(c, k, 6.7f, 2.6f), col, th);
            arc(dl, c, k, 4.9f, 2.6f, r, 0.f, 1.57079633f, 6, col, th);
            dl->AddLine(at(c, k, 4.9f, 4.4f), at(c, k, -1.5f, 4.4f), col, th);
            // the bubble's tail (l-3.6,3.0 v-3.0)
            const float tail[8] = { -1.5f, 4.4f, -5.1f, 7.4f, -5.1f, 4.4f, -7.7f, 4.4f };
            poly(dl, c, k, tail, 4, false, col, th);
            arc(dl, c, k, -7.7f, 2.6f, r, 1.57079633f, 3.14159265f, 6, col, th);
            dl->AddLine(at(c, k, -9.5f, 2.6f), at(c, k, -9.5f, -3.6f), col, th);
            arc(dl, c, k, -7.7f, -3.6f, r, 3.14159265f, 4.71238898f, 6, col, th);
            break;
        }

        case Kind::Flag:
        {
            // staff down the left, pennant hanging off its top half - the
            // reference's Teams icon
            dl->AddLine(at(c, k, -5.6f, -7.2f), at(c, k, -5.6f, 7.6f), col, th);
            const float pen[8] = { -5.6f, -6.6f,  6.4f, -6.6f,
                                    3.6f, -3.0f,  6.4f,  0.6f };
            poly(dl, c, k, pen, 4, false, col, th);
            dl->AddLine(at(c, k, 6.4f, 0.6f), at(c, k, -5.6f, 0.6f), col, th);
            break;
        }

        case Kind::Sliders:
        {
            // three 13 px rails with a filled knob each (the reference's
            // staggered x offsets make the rows read as sliders, not a list)
            const float dy[3] = { -3.6f, 1.2f, 6.0f };
            const float dx[3] = { 1.4f, -2.6f, 3.4f };
            for (int i = 0; i < 3; ++i)
            {
                dl->AddLine(at(c, k, -6.5f, dy[i]), at(c, k, 6.5f, dy[i]), col, th);
                dl->AddCircleFilled(at(c, k, dx[i], dy[i]), 2.0f * k, col, 16);
            }
            break;
        }
        }
    }
}
