#pragma once

#include "app/Settings.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace EspLayout {

    // 0 top / 1 bottom / 2 left / 3 right
    enum Side : int {
        Top = 0,
        Bottom = 1,
        Left = 2,
        Right = 3
    };

    // ╨░╨╣╨┤╨╕╤ê╨╜╨╕╨║╨╕ ╨┤╨╗╤Å ╨┐╤Ç╨╡╨▓╤î╤Ä (╤à╨┐╨▒╨░╤Ç ╨╛╤é╨┤╨╡╨╗╤î╨╜╨╛, ╨▓ ╤ü╤é╨╡╨║ ╨╜╨╡ ╨╗╨╡╨╖╨╡╤é)
    enum ElemId : int {
        IdName = 0,
        IdDistance = 1,
        IdTool = 2,
        IdFlags = 3,
        IdHealthText = 4
    };

    struct Box {
        float x1{}, y1{}, x2{}, y2{};
        float MidX() const { return (x1 + x2) * 0.5f; }
        float MidY() const { return (y1 + y2) * 0.5f; }
        float W() const { return x2 - x1; }
        float H() const { return y2 - y1; }
    };

    struct StackItem {
        int    id{ -1 };
        float* offset{ nullptr };
        float  extent{ 0.f };
    };

    inline Side NearestSide(float mx, float my, const Box& b)
    {
        float midX = b.MidX();
        float midY = b.MidY();
        float dx = mx - midX;
        float dy = my - midY;
        float nx = dx / (b.W() * 0.5f + 1.f);
        float ny = dy / (b.H() * 0.5f + 1.f);

        if (std::fabs(ny) >= std::fabs(nx))
        {
            if (ny < 0.f)
                return Top;

            return Bottom;
        }

        if (nx < 0.f)
            return Left;

        return Right;
    }

    inline int ClampHealthBarSide(int side)
    {
        // ╤à╨┐╨▒╨░╤Ç ╤é╨╛╨║╨░ ╤ü╨╗╨╡╨▓╨░/╤ü╨┐╤Ç╨░╨▓╨░
        if (side == Right)
            return Right;

        return Left;
    }

    inline float ClampOffset(float off)
    {
        if (off < 0.f) off = 0.f;
        if (off > 280.f) off = 280.f;
        return off;
    }

    // ╤é╨╡╨║╤ü╤é ╤ü╨▒╨╛╨║╤â ╨╛╤é ╨▒╨╛╨║╤ü╨░
    inline void PlaceText(int side, const Box& b, float offset,
                          float textW, float textH, float padL, float padR, float pad,
                          float& outX, float& outY)
    {
        float off = ClampOffset(offset);
        switch (side)
        {
        case Top:
            outX = b.MidX() - textW * 0.5f;
            outY = b.y1 - pad - textH - off;
            break;
        case Bottom:
            outX = b.MidX() - textW * 0.5f;
            outY = b.y2 + pad + off;
            break;
        case Left:
            outX = b.x1 - padL - textW;
            outY = b.y1 + pad + off;
            break;
        case Right:
        default:
            outX = b.x2 + padR;
            outY = b.y1 + pad + off;
            break;
        }
    }

    // ╨╕╨╖ ╨┐╨╛╨╖╨╕╤å╨╕╨╕ ╤é╨╡╨║╤ü╤é╨░ ╨╛╨▒╤Ç╨░╤é╨╜╨╛ ╨▓ offset
    inline float OffsetFromPos(int side, float tx, float ty, float textH,
                               const Box& b, float pad)
    {
        float off = 0.f;
        switch (side)
        {
        case Top:
            off = b.y1 - pad - textH - ty;
            break;
        case Bottom:
            off = ty - (b.y2 + pad);
            break;
        case Left:
        case Right:
        default:
            off = ty - (b.y1 + pad);
            break;
        }
        return ClampOffset(off);
    }

    inline void PlaceHealthBar(int side, const Box& b, float barW, float gap,
                               float& outX1, float& outY1, float& outX2, float& outY2)
    {
        bool right = (ClampHealthBarSide(side) == Right);
        if (right)
        {
            outX1 = b.x2 + gap;
            outX2 = outX1 + barW;
        }

        else
        {
            outX2 = b.x1 - gap;
            outX1 = outX2 - barW;
        }

        outY1 = b.y1;
        outY2 = b.y2;
    }

    // ╨╢╨╝╤æ╨╝ ╤ü╤é╨╡╨║ ╨▒╨╡╨╖ ╨┤╤ï╤Ç, pinned ╤ü╨▓╨╡╤Ç╤à╤â ╨┐╤Ç╨╕ ╨┤╤Ç╨░╨│╨╡
    inline void ResolveStack(std::vector<StackItem>& items, int pinned_id, float gap)
    {
        if (items.empty())
            return;

        std::sort(items.begin(), items.end(), [pinned_id](const StackItem& a, const StackItem& b) {
            float ao = a.offset ? *a.offset : 0.f;
            float bo = b.offset ? *b.offset : 0.f;
            if (std::fabs(ao - bo) < 0.5f)
            {
                if (a.id == pinned_id) return true;
                if (b.id == pinned_id) return false;
                return a.id < b.id;
            }

            return ao < bo;
        });

        float cursor = 0.f;
        for (auto& it : items)
        {
            if (!it.offset)
                continue;
            *it.offset = ClampOffset(cursor);
            cursor += it.extent + gap;
        }
    }

    inline void CollectSideItems(Settings& s, int side, float line_h, int flags_lines,
                                 std::vector<StackItem>& out)
    {
        out.clear();
        (void)flags_lines;
        auto add = [&](int id, bool enabled, int el_side, float* off, float extent) {
            if (!enabled || el_side != side || !off) return;
            out.push_back({ id, off, extent });
        };

        add(IdName, s.esp.name, s.esp.name_side, &s.esp.name_off, line_h);
        add(IdDistance, s.esp.distance, s.esp.distance_side, &s.esp.distance_off, line_h);
        add(IdTool, s.esp.tool, s.esp.tool_side, &s.esp.tool_off, line_h);
        add(IdHealthText, s.esp.health_text, s.esp.health_text_side, &s.esp.health_text_off, line_h);
    }

    inline void ResolveSettingsSide(Settings& s, int side, float line_h, int flags_lines, int pinned_id)
    {
        if (side < 0 || side > 3) return;
        std::vector<StackItem> items;
        CollectSideItems(s, side, line_h, flags_lines, items);
        ResolveStack(items, pinned_id, 2.f);
    }

    inline void ResolveAllSides(Settings& s, float line_h, int flags_lines, int pinned_id = -1)
    {
        for (int side = 0; side < 4; ++side)
            ResolveSettingsSide(s, side, line_h, flags_lines, pinned_id);
    }

} // namespace EspLayout
} // namespace Visuals
} // namespace Cheat

namespace EspLayout = Cheat::Visuals::EspLayout;
