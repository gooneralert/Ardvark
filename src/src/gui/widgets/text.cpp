#include "pch.h"
#include "widgets.h"
#include "../resources/fonts/fonts.h"
#include "imgui_internal.h"
#include <cmath>

namespace {
    ImU32 lerp_u32(ImU32 a, ImU32 b, float t) {
        const ImVec4 va = ImGui::ColorConvertU32ToFloat4(a);
        const ImVec4 vb = ImGui::ColorConvertU32ToFloat4(b);
        return ImGui::ColorConvertFloat4ToU32(ImLerp(va, vb, ImSaturate(t)));
    }

    float text_width(ImFont* font, float font_size, const char* text) {
        if (!text)
            return 0.0f;
        if (font)
            return font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text).x;
        return ImGui::CalcTextSize(text).x;
    }

    void draw_text_outlined(ImDrawList* dl, ImFont* font, float font_size, ImVec2 pos, ImU32 color, const char* text)
    {
        if (!dl || !text)
            return;

        font_size = fonts::snap_px(font_size);
        float x = std::floor(pos.x);
        float y = std::floor(pos.y);
        ImU32 shadow = IM_COL32(0, 0, 0, 255);

        // 8 направлений тени, дешево и читаемо
        for (int i = -1; i <= 1; ++i)
        {
            for (int j = -1; j <= 1; ++j)
            {
                if (i == 0 && j == 0)
                    continue;
                if (font)
                    dl->AddText(font, font_size, ImVec2(x + i, y + j), shadow, text);

                else
                    dl->AddText(ImVec2(x + i, y + j), shadow, text);
            }
        }

        if (font)
            dl->AddText(font, font_size, ImVec2(x, y), color, text);

        else
            dl->AddText(ImVec2(x, y), color, text);
    }
}

namespace widgets {
    void draw_outlined_text(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, ImU32 color, const char* text, float extra_width) {
        if (!draw_list || !text)
            return;

        if (font_size <= 0.0f)
            font_size = fonts::ui_size(font);
        if (!font)
            font = fonts::ui();

        if (extra_width > 0.0f) {
            const float natural = text_width(font, font_size, text);
            if (natural > 0.0f) {
                const float scale = (natural + extra_width) / natural;
                char glyph[2] = {};
                float x = pos.x;
                for (const char* p = text; *p; ++p) {
                    glyph[0] = *p;
                    draw_text_outlined(draw_list, font, font_size, ImVec2(x, pos.y), color, glyph);
                    x += text_width(font, font_size, glyph) * scale;
                }
                return;
            }
        }

        draw_text_outlined(draw_list, font, font_size, pos, color, text);
    }

    void draw_outlined_text_spans(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, const text_span* spans, int span_count) {
        if (!draw_list || !spans || span_count <= 0)
            return;

        if (font_size <= 0.0f)
            font_size = fonts::ui_size(font);
        if (!font)
            font = fonts::ui();

        float x = pos.x;
        for (int i = 0; i < span_count; ++i) {
            draw_outlined_text(draw_list, font, font_size, ImVec2(x, pos.y), spans[i].color, spans[i].text);
            x += text_width(font, font_size, spans[i].text);
        }
    }

    static void draw_shimmer_glyphs(
        ImDrawList* draw_list,
        ImFont* font,
        float font_size,
        ImVec2 pos,
        float origin_x,
        ImU32 base_color,
        ImU32 highlight_color,
        const char* text,
        float phase,
        float wavelength_px)
    {
        if (!draw_list || !text || !text[0])
            return;

        if (wavelength_px < 1.f)
            wavelength_px = 1.f;

        // 2 pi
        float k = 6.283185f / wavelength_px;

        char glyph[2] = {};
        float x = pos.x;
        for (const char* p = text; *p; ++p)
        {
            glyph[0] = *p;
            float gw = text_width(font, font_size, glyph);
            float sample_x = x + gw * 0.5f - origin_x;
            float wave = 0.5f + 0.5f * std::sin(sample_x * k - phase);
            ImU32 col = lerp_u32(base_color, highlight_color, wave);
            draw_text_outlined(draw_list, font, font_size, ImVec2(x, pos.y), col, glyph);
            x += gw;
        }
    }

    void draw_outlined_text_shimmer(
        ImDrawList* draw_list,
        ImFont* font,
        float font_size,
        ImVec2 pos,
        ImU32 base_color,
        ImU32 highlight_color,
        const char* text,
        float phase,
        float wavelength_px)
    {
        if (font_size <= 0.0f)
            font_size = fonts::ui_size(font);
        if (!font)
            font = fonts::ui();
        draw_shimmer_glyphs(
            draw_list, font, font_size, pos, pos.x,
            base_color, highlight_color, text, phase, wavelength_px);
    }

    void draw_outlined_text_spans_shimmer(
        ImDrawList* draw_list,
        ImFont* font,
        float font_size,
        ImVec2 pos,
        const text_span* spans,
        int span_count,
        ImU32 highlight_color,
        float phase,
        float wavelength_px)
    {
        if (!draw_list || !spans || span_count <= 0)
            return;

        if (font_size <= 0.0f)
            font_size = fonts::ui_size(font);
        if (!font)
            font = fonts::ui();

        const float origin_x = pos.x;
        float x = pos.x;
        for (int i = 0; i < span_count; ++i) {
            draw_shimmer_glyphs(
                draw_list, font, font_size, ImVec2(x, pos.y), origin_x,
                spans[i].color, highlight_color, spans[i].text, phase, wavelength_px);
            x += text_width(font, font_size, spans[i].text);
        }
    }
}
