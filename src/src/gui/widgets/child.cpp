#include "pch.h"
#include "widgets.h"
#include "../colors/colors.h"
#include "imgui_internal.h"
#include <cmath>
#include <vector>

namespace {
    struct child_context {
        ImVec2 cursor_start{};
        ImVec2 panel_size{};
    };

    void paint_hline(ImDrawList* dl, int x0, int x1, int y, ImU32 color)
    {
        if (x1 < x0)
            return;
        dl->AddRectFilled(
            ImVec2((float)x0, (float)y),
            ImVec2((float)(x1 + 1), (float)(y + 1)),
            color);
    }

    struct child_layout {
        ImRect outer{};
        ImRect inner{};
        ImRect fill{};
    };

    constexpr float k_child_rounding = 8.f;

    // horizontal inset of a rounded corner at `depth` px from the edge
    float corner_inset(float depth, float radius)
    {
        float t = radius - depth;
        if (t <= 0.f)
            return 0.f;
        if (t > radius)
            t = radius;
        return radius - sqrtf(radius * radius - t * t);
    }

    std::vector<child_context>& child_stack() {
        static std::vector<child_context> stack;
        return stack;
    }

    child_layout make_child_layout(const ImVec2& origin, const ImVec2& size)
    {
        int ol = (int)ImFloor(origin.x);
        int ot = (int)ImFloor(origin.y);
        int orr = ol + (int)ImFloor(size.x);
        int ob = ot + (int)ImFloor(size.y);

        child_layout layout{};
        layout.outer = ImRect(
            ImVec2((float)ol, (float)ot),
            ImVec2((float)orr, (float)ob));
        layout.inner = ImRect(
            ImVec2((float)(ol + 1), (float)(ot + 1)),
            ImVec2((float)(orr - 1), (float)(ob - 1)));
        // fill чуть уже а то бордер едет
        layout.fill = ImRect(
            ImVec2((float)(ol + 2), (float)(ot + 1)),
            ImVec2((float)(orr - 2), (float)(ob - 2)));
        return layout;
    }

    void draw_inner_border_no_top(const child_layout& layout, ImDrawList* dl, ImU32 color) {
        // sides + bottom only: clip away the top line, round the bottom corners
        const float r = k_child_rounding > 1.f ? k_child_rounding - 1.f : 0.f;
        dl->PushClipRect(
            ImVec2(layout.inner.Min.x, layout.inner.Min.y + k_child_rounding),
            layout.inner.Max, true);
        dl->AddRect(layout.inner.Min, layout.inner.Max, color, r, ImDrawFlags_RoundCornersBottom);
        dl->PopClipRect();
    }

    bool has_child_title(const char* title)
    {
        return title != nullptr && title[0] != '\0';
    }

    void draw_child_background(ImDrawList* dl, const child_layout& layout, ImU32 fill_color, bool draw_header) {
        const int fl = static_cast<int>(ImFloor(layout.fill.Min.x));
        const int ft = static_cast<int>(ImFloor(layout.fill.Min.y));
        const int fr = static_cast<int>(ImCeil(layout.fill.Max.x)) - 1;
        const int fb = static_cast<int>(ImCeil(layout.fill.Max.y)) - 1;

        dl->AddRectFilled(
            ImVec2(static_cast<float>(fl), static_cast<float>(ft)),
            ImVec2(static_cast<float>(fr + 1), static_cast<float>(fb + 1)),
            fill_color,
            k_child_rounding);

        if (!draw_header)
            return;

        // header gradient rows, inset near the rounded top corners
        const int header_b = ft + 22 - 1;
        for (int y = ft; y <= header_b; ++y)
        {
            const float ins = corner_inset(static_cast<float>(y - ft) + 0.5f, k_child_rounding);
            const int l = fl + static_cast<int>(ins);
            const int r = fr - static_cast<int>(ins);
            if (r < l)
                continue;
            dl->AddRectFilled(
                ImVec2(static_cast<float>(l), static_cast<float>(y)),
                ImVec2(static_cast<float>(r + 1), static_cast<float>(y + 1)),
                colors::header_gradient_row(y - ft, 22));
        }

        if (fb > header_b) {
            dl->AddRectFilled(
                ImVec2(static_cast<float>(fl), static_cast<float>(header_b + 1)),
                ImVec2(static_cast<float>(fr + 1), static_cast<float>(fb + 1)),
                fill_color);
        }
    }

    void draw_child_separator(ImDrawList* dl, const child_layout& layout, ImU32 inner_color) {
        const int fl = static_cast<int>(ImFloor(layout.fill.Min.x));
        const int ft = static_cast<int>(ImFloor(layout.fill.Min.y));
        const int fr = static_cast<int>(ImCeil(layout.fill.Max.x)) - 1;
        const int sep_y = ft + 22;
        paint_hline(dl, fl, fr, sep_y, inner_color);
    }

    void draw_child_title_on_gradient(
        ImDrawList* dl,
        const child_layout& layout,
        const char* title,
        ImFont* title_font,
        float title_font_size) {
        if (!title || !title[0])
            return;

        if (!title_font)
            title_font = ImGui::GetFont();
        if (title_font_size <= 0.0f)
            title_font_size = ImGui::GetFontSize();

        const int fl = static_cast<int>(ImFloor(layout.fill.Min.x));
        const int ft = static_cast<int>(ImFloor(layout.fill.Min.y));

        float title_pad_x = 6.f;
        ImVec2 tsz = title_font->CalcTextSizeA(title_font_size, FLT_MAX, 0.f, title);
        ImVec2 tpos(
            ImFloor((float)fl + title_pad_x),
            ImFloor((float)ft + (22.f - tsz.y) * 0.5f));

        widgets::draw_outlined_text(
            dl, title_font, title_font_size, tpos,
            colors::text_active_u32(), title);
    }

    void draw_child_frame(
        ImDrawList* dl,
        const child_layout& layout,
        ImU32 outer_color,
        ImU32 inner_color,
        ImU32 fill_color,
        const char* title,
        ImFont* title_font,
        float title_font_size) {
        const bool draw_header = has_child_title(title);
        draw_child_background(dl, layout, fill_color, draw_header);
        if (draw_header)
            draw_child_separator(dl, layout, inner_color);

        dl->AddRect(layout.outer.Min, layout.outer.Max, outer_color, k_child_rounding);

        draw_inner_border_no_top(layout, dl, inner_color);
        draw_child_title_on_gradient(dl, layout, title, title_font, title_font_size);
    }
}

namespace widgets {
    bool begin_child_panel(
        const char* id,
        const ImVec2& size,
        const char* title,
        ImFont* title_font,
        float title_font_size,
        const ImVec4* outer_border,
        const ImVec4* inner_border,
        const ImVec4* fill)
    {
        ImVec2 cursor = ImGui::GetCursorPos();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        child_layout layout = make_child_layout(origin, size);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 outer_col = ImGui::GetColorU32(outer_border ? *outer_border : colors::content_outer_border);
        ImU32 inner_col = ImGui::GetColorU32(inner_border ? *inner_border : colors::content_inner_border);

        // translucent fill: lets the frosted-glass backdrop show through
        ImVec4 fill_v = fill ? *fill : colors::child_fill;
        fill_v.w *= 0.55f;
        ImU32 fill_col = ImGui::GetColorU32(fill_v);

        draw_child_frame(
            dl, layout, outer_col, inner_col, fill_col,
            title, title_font, title_font_size);

        ImVec2 content_size(layout.fill.GetWidth(), layout.fill.GetHeight());
        bool draw_header = has_child_title(title);
        float header_h = 0.f;
        if (draw_header)
            header_h = 22.f + 1.f; // hdr rows
        float inset_y = 1.f;

        ImVec4 child_bg = fill_v;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, child_bg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

        child_stack().push_back(child_context{cursor, size});

        ImGui::SetCursorPos(ImVec2(cursor.x + 2.f, cursor.y + inset_y + header_h));

        float child_h = content_size.y - inset_y - header_h;
        if (child_h < 0.f) child_h = 0.f;
        const bool ok = ImGui::BeginChild(
            id,
            ImVec2(content_size.x, child_h),
            false,
            ImGuiWindowFlags_NoScrollbar);
        if (ok)
            menu_row_reset();
        return ok;
    }

    void end_child_panel()
    {
        std::vector<child_context>& stack = child_stack();
        IM_ASSERT(!stack.empty());
        child_context ctx = stack.back();
        stack.pop_back();

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ctx.cursor_start);
        ImGui::Dummy(ctx.panel_size);
    }
}
