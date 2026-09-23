#include "menu_theme.h"
#include <render/render.h>
#include <render/backdrop_blur.h>
#include <render/textures/texture.h>
#include <settings.h>
#include <cmath>
#include <cstring>
#include <map>

#include "icons8_crosshair_24_png.h"

static ID3D11ShaderResourceView* g_combat_icon_srv   = nullptr;
static bool                      g_combat_icon_loaded = false;

static void EnsureCombatIconLoaded()
{
    if (g_combat_icon_loaded || !render || !render->detail || !render->detail->device)
        return;
    g_combat_icon_srv = D3D11CreateTextureFromBytes(
        render->detail->device, g_icons8_crosshair_24_png, g_icons8_crosshair_24_png_len);
    g_combat_icon_loaded = true;
}

static void DrawCombatTabIcon(ImDrawList* dl, const ImVec2& center, float size, ImU32 col)
{
    EnsureCombatIconLoaded();
    if (!g_combat_icon_srv)
        return;

    const ImVec2 half(size * 0.5f, size * 0.5f);
    const ImVec2 imin(center.x - half.x, center.y - half.y);
    const ImVec2 imax(center.x + half.x, center.y + half.y);
    dl->AddImage((ImTextureID)g_combat_icon_srv, imin, imax, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), col);
}

// ---------------------------------------------------------------------------
// Reference palette  (from k6n5sk/Cheat ImGui/framework/settings/colors.h)
// ---------------------------------------------------------------------------
namespace
{
    constexpr ImVec4 kBgColor       { 26/255.f, 27/255.f, 38/255.f, 0.98f };
    constexpr ImVec4 kChildBg       { 32/255.f, 34/255.f, 46/255.f,  1.f };
    // Child stroke — hairline border
    constexpr ImVec4 kChildStroke   { 47/255.f, 52/255.f, 61/255.f, 0.45f };
    // Section icon inactive — mid grey
    constexpr ImVec4 kSectionIcon   { 120/255.f,120/255.f,128/255.f,1.f };
    // Favorite button inactive — dark grey
    constexpr ImVec4 kFavInactive   { 28/255.f, 28/255.f, 34/255.f, 1.f };
    // Topbar name: white
    constexpr ImVec4 kTopbarName    { 1.f, 1.f, 1.f, 1.f };
    // Topbar year: light grey
    constexpr ImVec4 kTopbarYear    { 150/255.f,150/255.f,158/255.f,1.f };
    // Widget label: muted grey
    constexpr ImVec4 kWidgetLabel   { 122/255.f, 131/255.f, 165/255.f, 1.f };
    // Widget inactive: mid grey
    constexpr ImVec4 kWidgetInact   { 64/255.f, 70/255.f, 102/255.f, 1.f };

    // Runtime accent (default pink: ImColor(255,129,216))
    static ImVec4 g_accent      { 255/255.f, 129/255.f, 216/255.f, 1.f };
    static ImVec4 g_accent_soft { 255/255.f, 129/255.f, 216/255.f, 0.08f };

    ImU32 ToU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

    inline float SmoothLerp(float cur, float tgt, float spd)
    {
        return cur + (tgt - cur) * ImClamp(ImGui::GetIO().DeltaTime * spd, 0.f, 1.f);
    }

    constexpr int kSidebarCount = 8;
    static const char* kSectionLabels[kSidebarCount] = {
        "Combat", "Visuals", "World", "Character", "Options", "Configs", "NPC", "Teams"
    };

    // Draw a rainbow/hue gradient bar, returns true if dragged
    bool DrawHueSlider(ImDrawList* dl, ImVec2 pos, ImVec2 size, float* hue_out)
    {
        const int kSegs = 6;
        static const ImU32 kStops[7] = {
            IM_COL32(255,  0,  0,255), IM_COL32(255,255,  0,255),
            IM_COL32(  0,255,  0,255), IM_COL32(  0,255,255,255),
            IM_COL32(  0,  0,255,255), IM_COL32(255,  0,255,255),
            IM_COL32(255,  0,  0,255),
        };
        const float seg_w = size.x / kSegs;
        for (int i = 0; i < kSegs; ++i)
        {
            dl->AddRectFilledMultiColor(
                ImVec2(pos.x + i * seg_w, pos.y),
                ImVec2(pos.x + (i+1) * seg_w, pos.y + size.y),
                kStops[i], kStops[i+1], kStops[i+1], kStops[i]);
        }

        // Draw rounded border over the gradient
        dl->AddRect(pos, pos + size, IM_COL32(255,255,255,20), 3.f, 0, 1.f);

        // Cursor indicator
        const float cx = pos.x + (*hue_out) * size.x;
        dl->AddRectFilled(ImVec2(cx - 3.f, pos.y - 1.f), ImVec2(cx + 3.f, pos.y + size.y + 1.f),
            IM_COL32(255,255,255,220), 2.f);

        // Interaction
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool over = mouse.x >= pos.x && mouse.x <= pos.x + size.x &&
                          mouse.y >= pos.y && mouse.y <= pos.y + size.y;
        bool dragging = over && ImGui::IsMouseDown(0);
        if (dragging)
        {
            *hue_out = ImClamp((mouse.x - pos.x) / size.x, 0.f, 1.f);
            return true;
        }
        return false;
    }
}

// ---------------------------------------------------------------------------
//  GetWindowW / GetWindowH — layout matches 2560x1080. Smaller screens scale
//  the whole menu uniformly (spacing included) so rows don't pack together.
// ---------------------------------------------------------------------------
static float GetBaseUIScale()
{
    // Design layout is 2560x1080. Using 1440p as the height base crushed
    // 1080p / ultrawide (2560x1080 → 0.75) and packed every row together.
    constexpr float kDesignW = 2560.f;
    constexpr float kDesignH = 1080.f;

    if (settings::menu::screen_width != 2560 || settings::menu::screen_height != 1440)
    {
        if (settings::menu::screen_width <= 0) return 1.0f;
        return static_cast<float>(settings::menu::screen_width) / kDesignW;
    }

    ImVec2 d = ImGui::GetIO().DisplaySize;
    if (d.x < 100.f || d.y < 100.f)
    {
        d.x = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
        d.y = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
    }
    if (d.x <= 0.f || d.y <= 0.f) return 1.0f;
    const float sx = d.x / kDesignW;
    const float sy = d.y / kDesignH;
    float s = (sx < sy) ? sx : sy;
    s = ImClamp(s, 0.50f, 1.0f);
    return s;
}
float MenuTheme::GetUIScale()
{
    return GetBaseUIScale() * settings::menu::menu_scale;
}

float MenuTheme::GetLogoH()          { return IM_ROUND(kLogoH * GetUIScale()); }
float MenuTheme::GetTopSelectH()     { return IM_ROUND(kTopSelectH * GetUIScale()); }
float MenuTheme::GetTopbarH()        { return IM_ROUND(kTopbarH * GetUIScale()); }
float MenuTheme::GetChildTitlebarH() { return IM_ROUND(kChildTitlebarH * GetUIScale()); }

float MenuTheme::GetWindowW() { return IM_ROUND(kWindowW * GetUIScale()); }

void MenuTheme::AddRectFilledCrisp(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float rounding, ImDrawFlags flags)
{
    if (!dl || (col & IM_COL32_A_MASK) == 0)
        return;
    if (a.x > b.x) ImSwap(a.x, b.x);
    if (a.y > b.y) ImSwap(a.y, b.y);
    a.x = IM_ROUND(a.x); a.y = IM_ROUND(a.y);
    b.x = IM_ROUND(b.x); b.y = IM_ROUND(b.y);
    if (b.x <= a.x || b.y <= a.y)
        return;

    const ImDrawFlags corners = (flags & ImDrawFlags_RoundCornersMask_);
    const ImDrawFlags use = corners ? corners : ImDrawFlags_RoundCornersAll;
    const float max_r = ImFloor(ImMin(b.x - a.x, b.y - a.y) * 0.5f);
    const float r = ImClamp(IM_ROUND(rounding), 0.f, max_r);
    if (r < 1.f)
    {
        dl->AddRectFilled(a, b, col);
        return;
    }

    const bool tl = (use & ImDrawFlags_RoundCornersTopLeft) != 0;
    const bool tr = (use & ImDrawFlags_RoundCornersTopRight) != 0;
    const bool br = (use & ImDrawFlags_RoundCornersBottomRight) != 0;
    const bool bl = (use & ImDrawFlags_RoundCornersBottomLeft) != 0;
    const int segs = ImClamp((int)(r * 1.35f) + 10, 14, 28);

    if (tl) dl->PathArcTo(ImVec2(a.x + r, a.y + r), r, IM_PI, IM_PI * 1.5f, segs);
    else    dl->PathLineTo(a);
    if (tr) dl->PathArcTo(ImVec2(b.x - r, a.y + r), r, IM_PI * 1.5f, IM_PI * 2.f, segs);
    else    dl->PathLineTo(ImVec2(b.x, a.y));
    if (br) dl->PathArcTo(ImVec2(b.x - r, b.y - r), r, 0.f, IM_PI * 0.5f, segs);
    else    dl->PathLineTo(b);
    if (bl) dl->PathArcTo(ImVec2(a.x + r, b.y - r), r, IM_PI * 0.5f, IM_PI, segs);
    else    dl->PathLineTo(ImVec2(a.x, b.y));
    dl->PathFillConvex(col);
}

namespace
{
    float g_fit_h = 0.f;
    float g_fit_accum = 0.f;
    bool  g_fitting = false;
}

void MenuTheme::BeginContentFit()
{
    g_fitting = true;
    g_fit_accum = 0.f;
}

void MenuTheme::NoteContentHeight(float h)
{
    if (!g_fitting || h <= 1.f)
        return;
    if (h > g_fit_accum)
        g_fit_accum = h;
}

bool MenuTheme::IsFittingContent()
{
    return g_fitting;
}

bool MenuTheme::HidePanelScroll()
{
    if (!g_fitting)
        return false;
    const float max_h = ImGui::GetIO().DisplaySize.y - 24.f;
    return g_fit_h < 1.f || g_fit_h < max_h - 2.f;
}

void MenuTheme::EndContentFit()
{
    if (g_fitting && g_fit_accum > 1.f)
    {
        const float s = GetUIScale();
        const float chrome =
            GetTopSelectH() + GetTopbarH() + Sz(12.f) + Sz(20.f);
        const float want = chrome + g_fit_accum;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float max_h = ImMax(120.f, display.y - 24.f);
        const float min_h = GetTopSelectH() + GetTopbarH() + 80.f * s;
        g_fit_h = ImClamp(want, min_h, max_h);
    }
    g_fitting = false;
    g_fit_accum = 0.f;
}

float MenuTheme::GetWindowH()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float max_h = ImMax(120.f, display.y > 50.f ? display.y - 24.f : kWindowH);
    return IM_ROUND(ImMin(kWindowH * GetUIScale(), max_h));
}

void MenuTheme::ApplyLayoutScale()
{
    const float s = GetUIScale();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowPadding     = ImVec2(16.f * s, 14.f * s);
    st.ChildPadding      = ImVec2(18.f * s, 12.f * s);
    st.FramePadding      = ImVec2(10.f * s, 7.f * s);
    st.CellPadding       = ImVec2(3.f * s, 3.f * s);
    st.ItemSpacing       = ImVec2(10.f * s, 10.f * s);
    st.ItemInnerSpacing  = ImVec2(10.f * s, 4.f * s);
    st.ScrollbarSize     = ImMax(3.f, 3.f * s);
    st.IndentSpacing     = 14.f * s;
    st.GrabMinSize       = 8.f * s;
    st.WindowRounding    = IM_ROUND(settings::menu::panel_rounding);
    st.ChildRounding     = IM_ROUND(ImMin(settings::menu::panel_rounding, 10.f));
    st.FrameRounding     = IM_ROUND(8.f * s);
    st.PopupRounding     = IM_ROUND(12.f * s);
    st.GrabRounding      = 99.f;
    st.ScrollbarRounding = IM_ROUND(8.f * s);
    st.TabRounding       = IM_ROUND(8.f * s);
    st.CircleTessellationMaxError = 0.025f;
    st.CurveTessellationTol       = 0.05f;
    // 1.92 re-rasters at FontScaleMain. FontGlobalScale bilinear-scales after
    // bake and is what made Inter look fuzzy on 1080p (0.75x).
    ImGui::GetIO().FontGlobalScale = 1.0f;
    st.FontScaleMain = settings::menu::font_scale * s;

    static float s_prev_ui = -1.f;
    if (s_prev_ui != s)
    {
        g_fit_h = 0.f;
        s_prev_ui = s;
    }
}

// ---------------------------------------------------------------------------
//  SectionHeader — in-content group label with a thin underline beneath it
// ---------------------------------------------------------------------------
void MenuTheme::SectionHeader(const char* label)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImFont*     font = Fonts::SemiBold ? Fonts::SemiBold : ImGui::GetFont();
    const float sz   = FontPx(Typography::kSectionHeaderSize);
    const ImVec2 tsz = font->CalcTextSizeA(sz, FLT_MAX, 0.f, label);
    const ImVec2 pos = window->DC.CursorPos;

    window->DrawList->AddText(font, sz, pos, Typography::kSectionHeaderColor, label);

    const float underline_y = pos.y + tsz.y + 2.f;
    window->DrawList->AddLine(
        ImVec2(pos.x, underline_y), ImVec2(pos.x + tsz.x, underline_y),
        Typography::kSectionHeaderUnderline, 1.f);

    ImGui::Dummy(ImVec2(tsz.x, tsz.y + 2.f + Sz(Typography::kSectionHeaderPadBottom)));
}

// ---------------------------------------------------------------------------
//  AddTextSpaced / CalcSpacedTextWidth — per-glyph letter-spacing helpers.
//  ASCII-only (one byte == one glyph), which is all the uppercase tab-header
//  labels ever contain.
// ---------------------------------------------------------------------------
float MenuTheme::CalcSpacedTextWidth(ImFont* font, float font_size, const char* text, float spacing)
{
    if (!font) font = ImGui::GetFont();
    const int len = (int)strlen(text);
    if (len == 0) return 0.f;

    float total = 0.f;
    char buf[2] = { 0, 0 };
    for (int i = 0; i < len; ++i)
    {
        buf[0] = text[i];
        total += font->CalcTextSizeA(font_size, FLT_MAX, 0.f, buf).x;
        if (i + 1 < len) total += spacing;
    }
    return total;
}

float MenuTheme::AddTextSpaced(ImDrawList* dl, ImFont* font, float font_size, ImVec2 pos, ImU32 col, const char* text, float spacing)
{
    if (!font) font = ImGui::GetFont();
    const int len = (int)strlen(text);
    if (len == 0) return 0.f;

    float x = pos.x;
    char buf[2] = { 0, 0 };
    for (int i = 0; i < len; ++i)
    {
        buf[0] = text[i];
        dl->AddText(font, font_size, ImVec2(x, pos.y), col, buf);
        x += font->CalcTextSizeA(font_size, FLT_MAX, 0.f, buf).x + spacing;
    }
    return x - pos.x - spacing;
}

// ---------------------------------------------------------------------------
//  ApplyAccent
// ---------------------------------------------------------------------------
void MenuTheme::ApplyAccent()
{
    const float* c = settings::menu::accent_color;
    g_accent      = { c[0], c[1], c[2], 1.f };
    g_accent_soft = { c[0], c[1], c[2], 0.08f };

    ImGuiStyle& s = ImGui::GetStyle();
    auto ac = [&](float a) { return ImVec4(c[0],c[1],c[2],a); };

    s.Colors[ImGuiCol_CheckMark]            = ac(1.f);
    s.Colors[ImGuiCol_SliderGrab]           = ac(0.90f);
    s.Colors[ImGuiCol_SliderGrabActive]     = ac(1.f);
    s.Colors[ImGuiCol_SeparatorHovered]     = ac(0.35f);
    s.Colors[ImGuiCol_SeparatorActive]      = ac(1.f);
    s.Colors[ImGuiCol_Header]               = ac(0.10f);
    s.Colors[ImGuiCol_HeaderHovered]        = ac(0.18f);
    s.Colors[ImGuiCol_HeaderActive]         = ac(0.26f);
    s.Colors[ImGuiCol_ButtonHovered]        = Surfaces::kBgHover;
    s.Colors[ImGuiCol_ButtonActive]         = Surfaces::kBgActive;
    s.Colors[ImGuiCol_ScrollbarGrab]        = ac(0.40f);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ac(0.60f);
    s.Colors[ImGuiCol_ScrollbarGrabActive]  = ac(1.f);
    s.Colors[ImGuiCol_ResizeGrip]           = ac(0.10f);
    s.Colors[ImGuiCol_ResizeGripHovered]    = ac(0.25f);
    s.Colors[ImGuiCol_ResizeGripActive]     = ac(1.f);
    s.Colors[ImGuiCol_Tab]                  = ImVec4(0.f,0.f,0.f,1.f);
    s.Colors[ImGuiCol_TabHovered]           = ac(0.18f);
    s.Colors[ImGuiCol_TabActive]            = ImVec4(0.f,0.f,0.f,1.f);

    // Apply per-panel text + border + child colors from settings
    const float* ct  = settings::menu::color_text;
    const float* ctm = settings::menu::color_text_muted;
    const float* cb  = settings::menu::color_border;
    const float* cc  = settings::menu::color_child;
    s.Colors[ImGuiCol_Text]         = ImVec4(ct[0],  ct[1],  ct[2],  ct[3]);
    s.Colors[ImGuiCol_TextDisabled] = ImVec4(ctm[0], ctm[1], ctm[2], ctm[3]);
    s.Colors[ImGuiCol_Border]       = ImVec4(cb[0],  cb[1],  cb[2],  cb[3]);
    s.Colors[ImGuiCol_ChildBg]      = ImVec4(cc[0],  cc[1],  cc[2],  cc[3]);
}

ImVec4 MenuTheme::GetAccentVec4()       { return g_accent; }
ImU32  MenuTheme::ColAccent()           { return ToU32(g_accent); }
ImU32  MenuTheme::ColAccentSoft()       { return ToU32(g_accent_soft); }
ImU32  MenuTheme::ColSurface()          { return ToU32(kBgColor); }
ImU32  MenuTheme::ColSurfaceElevated()  { return ToU32(kChildBg); }
ImU32  MenuTheme::ColBorder()           { return ToU32(kChildStroke); }
ImU32  MenuTheme::ColBorderStrong()     { return IM_COL32(20,20,24,255); }
ImU32  MenuTheme::ColTextMuted()        { return ToU32(kWidgetInact); }
ImU32  MenuTheme::ColTextDim()          { return ToU32(kSectionIcon); }

// ---------------------------------------------------------------------------
//  Apply ï¿½ full ImGui style, called once on init
// ---------------------------------------------------------------------------
void MenuTheme::Apply(ImGuiIO& io)
{
    (void)io;
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = 16.f;
    s.ChildRounding     = 12.f;
    s.FrameRounding     = 8.f;
    s.PopupRounding     = 12.f;
    s.GrabRounding      = 99.f;
    s.ScrollbarRounding = 8.f;
    s.TabRounding       = 8.f;

    s.CircleTessellationMaxError = 0.025f;
    s.CurveTessellationTol       = 0.05f;

    s.WindowBorderSize  = 0.f;
    s.ChildBorderSize   = 1.f;
    s.FrameBorderSize   = 0.f;
    s.PopupBorderSize   = 1.f;

    s.WindowPadding     = ImVec2(16.f, 14.f);
    s.ChildPadding      = ImVec2(18.f, 12.f);
    s.FramePadding      = ImVec2(10.f, 7.f);
    s.CellPadding       = ImVec2(3.f,  3.f);
    s.ItemSpacing       = ImVec2(10.f, 10.f);
    s.ItemInnerSpacing  = ImVec2(10.f, 4.f);
    s.WindowMinSize     = ImVec2(0.f,  0.f);
    s.ScrollbarSize     = 3.f;
    s.IndentSpacing     = 14.f;
    s.GrabMinSize       = 8.f;

    const ImVec4 ac = g_accent;

    s.Colors[ImGuiCol_Text]                 = kWidgetLabel;
    s.Colors[ImGuiCol_TextDisabled]         = kWidgetInact;
    s.Colors[ImGuiCol_WindowBg]             = kBgColor;
    s.Colors[ImGuiCol_ChildBg]              = kChildBg;
    s.Colors[ImGuiCol_PopupBg]              = Surfaces::kPopupBg;
    s.Colors[ImGuiCol_Border]               = kChildStroke;
    s.Colors[ImGuiCol_BorderShadow]         = ImVec4(0,0,0,0);
    s.Colors[ImGuiCol_Separator]            = ImVec4(0.15f,0.15f,0.15f,1.f);
    s.Colors[ImGuiCol_SeparatorHovered]     = ImVec4(ac.x,ac.y,ac.z,0.35f);
    s.Colors[ImGuiCol_SeparatorActive]      = ac;
    s.Colors[ImGuiCol_Header]               = ImVec4(ac.x,ac.y,ac.z,0.10f);
    s.Colors[ImGuiCol_HeaderHovered]        = ImVec4(ac.x,ac.y,ac.z,0.18f);
    s.Colors[ImGuiCol_HeaderActive]         = ImVec4(ac.x,ac.y,ac.z,0.26f);
    s.Colors[ImGuiCol_Button]               = Surfaces::kBg;
    s.Colors[ImGuiCol_ButtonHovered]        = Surfaces::kBgHover;
    s.Colors[ImGuiCol_ButtonActive]         = Surfaces::kBgActive;
    s.Colors[ImGuiCol_FrameBg]              = Surfaces::kBg;
    s.Colors[ImGuiCol_FrameBgHovered]       = Surfaces::kBgHover;
    s.Colors[ImGuiCol_FrameBgActive]        = Surfaces::kBgActive;
    s.Colors[ImGuiCol_SliderGrab]           = ImVec4(ac.x,ac.y,ac.z,0.90f);
    s.Colors[ImGuiCol_SliderGrabActive]     = ac;
    s.Colors[ImGuiCol_CheckMark]            = ac;
    s.Colors[ImGuiCol_Tab]                  = ImVec4(0.f,0.f,0.f,1.f);
    s.Colors[ImGuiCol_TabHovered]           = ImVec4(ac.x,ac.y,ac.z,0.18f);
    s.Colors[ImGuiCol_TabActive]            = ImVec4(0.f,0.f,0.f,1.f);
    s.Colors[ImGuiCol_TabUnfocused]         = ImVec4(0,0,0,1);
    s.Colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0,0,0,1.f);
    s.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0,0,0,0.4f);
    s.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(ac.x,ac.y,ac.z,0.40f);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(ac.x,ac.y,ac.z,0.60f);
    s.Colors[ImGuiCol_ScrollbarGrabActive]  = ac;
    s.Colors[ImGuiCol_TitleBg]              = kBgColor;
    s.Colors[ImGuiCol_TitleBgActive]        = kChildBg;
    s.Colors[ImGuiCol_TitleBgCollapsed]     = kBgColor;
    s.Colors[ImGuiCol_ResizeGrip]           = ImVec4(ac.x,ac.y,ac.z,0.10f);
    s.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(ac.x,ac.y,ac.z,0.25f);
    s.Colors[ImGuiCol_ResizeGripActive]     = ac;
    s.Colors[ImGuiCol_PlotLines]            = ImVec4(ac.x,ac.y,ac.z,0.60f);
    s.Colors[ImGuiCol_PlotHistogram]        = ImVec4(ac.x,ac.y,ac.z,0.40f);
    s.Colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.06f,0.06f,0.06f,1.f);
    s.Colors[ImGuiCol_TableBorderStrong]    = kChildStroke;
    s.Colors[ImGuiCol_TableBorderLight]     = kChildStroke;
    s.Colors[ImGuiCol_NavHighlight]         = ImVec4(ac.x,ac.y,ac.z,0.70f);
}

// ---------------------------------------------------------------------------
//  DrawCursorSpotlight
// ---------------------------------------------------------------------------
void MenuTheme::DrawCursorSpotlight(ImDrawList* draw_list, const ImRect& window_bb)
{
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (!window_bb.Contains(mouse)) return;

    static float s_fade = 0.f;
    s_fade = SmoothLerp(s_fade, 1.f, 10.f);
    if (s_fade < 0.002f) return;

    constexpr float kRX = 140.f, kRY = 110.f;
    constexpr float kPeakAlpha = 0.035f;
    constexpr int   kSegs = 16;
    const ImU32  col_c = ImGui::GetColorU32(ImVec4(g_accent.x,g_accent.y,g_accent.z, kPeakAlpha * s_fade));
    const ImU32  col_e = IM_COL32(0,0,0,0);
    const ImVec2 uv    = ImGui::GetFontTexUvWhitePixel();
    constexpr float step = 2.f * 3.14159265f / kSegs;

    draw_list->PushClipRect(window_bb.Min, window_bb.Max, true);
    draw_list->PrimReserve(kSegs*3, kSegs*3);
    for (int i = 0; i < kSegs; ++i)
    {
        float a0 = i * step, a1 = (i+1) * step;
        ImVec2 p0 = mouse + ImVec2(cosf(a0)*kRX, sinf(a0)*kRY);
        ImVec2 p1 = mouse + ImVec2(cosf(a1)*kRX, sinf(a1)*kRY);
        draw_list->PrimVtx(mouse, uv, col_c);
        draw_list->PrimVtx(p0,   uv, col_e);
        draw_list->PrimVtx(p1,   uv, col_e);
    }
    draw_list->PopClipRect();
}

// ---------------------------------------------------------------------------
//  DrawBrandMark ï¿½ flame-style logo mark in logo cell
// ---------------------------------------------------------------------------
void MenuTheme::DrawBrandMark(ImDrawList* dl, ImVec2 pos, float scale)
{
    const float cell_w = GetLogoH();
    const float cell_h = GetTopSelectH();
    ImFont* font = ImGui::GetFont();
    const float sz = 26.f * scale;
    const char* lbl = "M";
    const ImVec2 tsz = font->CalcTextSizeA(sz, FLT_MAX, 0.f, lbl);
    const ImVec2 tp(
        ImFloor(pos.x + (cell_w - tsz.x) * 0.5f),
        ImFloor(pos.y + (cell_h - tsz.y) * 0.5f));
    dl->AddText(font, sz, tp, ColAccent(), lbl);
}

// ---------------------------------------------------------------------------
//  DrawWatermarkShell
// ---------------------------------------------------------------------------
void MenuTheme::DrawWatermarkShell(ImDrawList* dl, const ImRect& bb,
                                   const char* text, ImU32 text_col)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    dl->AddRectFilled(bb.Min, bb.Max, ColSurfaceElevated(), 4.f);
    dl->AddRect(bb.Min, bb.Max, ColBorder(), 4.f, 0, 1.f);

    const ImVec2 tsz  = ImGui::CalcTextSize(text);
    const ImVec2 tpos { bb.Min.x + style.FramePadding.x + 4.f,
                        bb.Min.y + (bb.GetHeight() - tsz.y) * 0.5f };
    dl->AddText(tpos, text_col, text);
}

// ---------------------------------------------------------------------------
//  DrawChildChrome ï¿½ reference-style child panel chrome
//  Drawn into `dl` BEFORE BeginChild is called.
//  pos/size = the total bounding box of the child (including titlebar).
// ---------------------------------------------------------------------------
void MenuTheme::DrawChildChrome(ImDrawList* dl, ImVec2 pos, ImVec2 size, const char* label)
{
    const ImVec2 br = pos + size;
    const float  r  = kChildRounding;

    // 1. Hairline stroke
    dl->AddRect(pos, br, IM_COL32(255, 255, 255, 14), r, 0, 1.f);

    // 2. Background fill
    dl->AddRectFilled(
        ImVec2(pos.x + 1.f, pos.y + 1.f),
        ImVec2(br.x  - 1.f, br.y  - 1.f),
        ToU32(kChildBg), r);

    // 3. Soft separator under the titlebar
    const float line_y = pos.y + kChildTitlebarH - 1.f;
    dl->AddLine(
        ImVec2(pos.x + 10.f, line_y),
        ImVec2(br.x  - 10.f, line_y),
        IM_COL32(255, 255, 255, 14),
        1.f);

    // 4. Small flame icon on the left (accent color)
    {
        ImFont* font  = ImGui::GetFont();
        const float sz = ImGui::GetFontSize() * 0.78f;
        const char* ic = "F";
        const ImVec2 isz = font->CalcTextSizeA(sz, FLT_MAX, 0.f, ic);
        dl->AddText(font, sz,
            ImVec2(pos.x + 10.f, pos.y + (kChildTitlebarH - isz.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(g_accent.x, g_accent.y, g_accent.z, 1.f)),
            ic);
    }

    // 5. Label text ï¿½ accent colored like in reference
    {
        ImFont* font  = ImGui::GetFont();
        const float sz = ImGui::GetFontSize() * 0.92f;
        const ImVec2 lsz = font->CalcTextSizeA(sz, FLT_MAX, 0.f, label);
        dl->AddText(font, sz,
            ImVec2(pos.x + 26.f, pos.y + (kChildTitlebarH - lsz.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(g_accent.x, g_accent.y, g_accent.z, 1.f)),
            label);
    }

    // 6. Gear / star icon on the right  ("*" as placeholder)
    {
        ImFont* font  = ImGui::GetFont();
        const float sz = ImGui::GetFontSize() * 0.75f;
        const char* ic = "*";
        const ImVec2 isz = font->CalcTextSizeA(sz, FLT_MAX, 0.f, ic);
        dl->AddText(font, sz,
            ImVec2(br.x - isz.x - 8.f, pos.y + (kChildTitlebarH - isz.y) * 0.5f),
            ImGui::GetColorU32(kSectionIcon),
            ic);
    }
}

// ---------------------------------------------------------------------------
//  TopTab  ï¿½ sub-tab bar inside child panels (unchanged design)
// ---------------------------------------------------------------------------
bool MenuTheme::TopTab(const char* id, const char* label, bool selected, float fixed_w)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const ImGuiID item_id = window->GetID(id);
    ImFont* font          = ImGui::GetFont();
    const float tab_sz    = ImGui::GetFontSize();
    const ImVec2 lsz      = font->CalcTextSizeA(tab_sz, FLT_MAX, 0.f, label);
    const float  pad_h    = 10.f;
    const float  h        = 26.f;
    const float  w        = (fixed_w > 0.f) ? fixed_w : (lsz.x + pad_h * 2.f);
    const ImVec2 pos      = window->DC.CursorPos;
    const ImRect bb(pos, pos + ImVec2(w, h));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, item_id)) return false;

    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, item_id, &hovered, &held);

    static std::map<ImGuiID, float> s_anim;
    float& anim = s_anim[item_id];
    anim = SmoothLerp(anim, selected ? 1.f : (hovered ? 0.40f : 0.f), 20.f);

    if (anim > 0.005f)
    {
        window->DrawList->AddRectFilled(bb.Min, bb.Max,
            ImGui::GetColorU32(ImVec4(g_accent.x,g_accent.y,g_accent.z, anim*(selected?0.10f:0.06f))));
    }

    const float ta = 0.30f + anim * 0.70f;
    const ImU32 tc = ImGui::GetColorU32(
        selected ? ImVec4(g_accent.x,g_accent.y,g_accent.z,ta)
                 : ImVec4(kWidgetInact.x,kWidgetInact.y,kWidgetInact.z,ta));
    window->DrawList->AddText(font, tab_sz,
        ImVec2(ImFloor(bb.Min.x + (w - lsz.x)*0.5f),
               ImFloor(bb.Min.y + (h - lsz.y)*0.5f)),
        tc, label);

    if (selected && anim > 0.005f)
    {
        window->DrawList->AddLine(
            ImVec2(bb.Min.x + w*0.15f, bb.Min.y + 1.f),
            ImVec2(bb.Max.x - w*0.15f, bb.Min.y + 1.f),
            ImGui::GetColorU32(ImVec4(g_accent.x,g_accent.y,g_accent.z, anim*0.85f)), 2.f);
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x + w, pos.y));
    return pressed;
}

// ---------------------------------------------------------------------------
//  DrawSubTabBar
// ---------------------------------------------------------------------------
void MenuTheme::DrawSubTabBar(const char* const* labels, int count, int* active_tab)
{
    for (int i = 0; i < count; ++i)
    {
        if (i > 0) ImGui::SameLine(0, 2.f);
        if (TopTab(labels[i], labels[i], *active_tab == i))
            *active_tab = i;
    }
}

// ---------------------------------------------------------------------------
//  DrawWindowGlow  — same gaussian rings as ImAdd::Button / DrawGlowRectBg
// ---------------------------------------------------------------------------
namespace
{
    constexpr float kGlowSpread  = 56.f;
    constexpr int   kGlowLayers  = 20;
    constexpr float kGlowAlpha01 = 0.14f;
    constexpr float kGlowColW    = 0.22f; // music player (reactive)
    constexpr float kGlowColWDim = 0.70f; // slight all-around halo (peak ≈ 0.10)

    void DrawGlowRings(ImDrawList* dl, const ImVec2& bb_min, const ImVec2& bb_max,
                       float rounding, float spread, float peak)
    {
        for (int i = 1; i <= kGlowLayers; ++i)
        {
            const float t = (float)i / (float)kGlowLayers;
            const float a = peak * expf(-2.1f * t * t);
            if (a < 0.002f) continue;
            const float exp = spread * t;
            dl->AddRectFilled(
                bb_min - ImVec2(exp, exp),
                bb_max + ImVec2(exp, exp),
                IM_COL32(0, 0, 0, (int)(a * 255.f)),
                rounding + exp);
        }
    }
}

void MenuTheme::DrawWindowGlow(const ImVec2& bb_min, const ImVec2& bb_max, float rounding)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    dl->PushClipRect(ImVec2(0.f, 0.f), ds, false);
    DrawGlowRings(dl, bb_min, bb_max, rounding, 32.f, 0.042f);
    dl->PopClipRect();
}

void MenuTheme::DrawWindowGlowReactive(const ImVec2& bb_min, const ImVec2& bb_max, float rounding, const float eq_bands[10])
{
    float bands[10]{};
    float energy = 0.f;
    if (eq_bands)
    {
        for (int i = 0; i < 10; ++i)
        {
            bands[i] = ImClamp(eq_bands[i], 0.f, 1.f);
            energy += bands[i];
        }
    }

    const float now = (float)ImGui::GetTime();
    if (energy < 0.025f)
    {
        for (int i = 0; i < 10; ++i)
        {
            const float u = (float)i / 9.f;
            bands[i] = ImClamp(
                0.18f
                + 0.32f * (0.5f + 0.5f * sinf(now * 2.4f + u * 5.2f))
                + 0.22f * (0.5f + 0.5f * sinf(now * 4.1f - u * 3.6f))
                + 0.10f * (0.5f + 0.5f * sinf(now * 7.3f + u * 9.0f)),
                0.04f, 1.f);
        }
        energy = 0.f;
        for (int i = 0; i < 10; ++i)
            energy += bands[i];
    }

    const float bass = ImMax(bands[0], bands[1]);

    const float dt = ImGui::GetIO().DeltaTime;
    static float s_env = 0.f;
    static float s_flash = 0.f;
    const float attack = (bass > s_env) ? ImClamp(20.f * dt, 0.f, 1.f) : ImClamp(7.f * dt, 0.f, 1.f);
    s_env += (bass - s_env) * attack;
    const float onset = ImMax(0.f, bass - s_env * 0.72f);
    s_flash = ImMax(s_flash - dt * 5.2f, onset * 2.4f);
    s_flash = ImClamp(s_flash, 0.f, 1.f);

    const float base = kGlowColW * kGlowAlpha01;
    const float peak = base * (0.85f + s_env * 0.55f + s_flash * 1.15f);
    const float spread = kGlowSpread + s_env * 2.5f + s_flash * 3.5f;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    dl->PushClipRect(ImVec2(0.f, 0.f), ds, false);
    DrawGlowRings(dl, bb_min, bb_max, rounding, spread, peak);
    dl->PopClipRect();
}

ImVec2 MenuTheme::TickSmoothDrag(const char* id, const ImVec2& size, const ImVec2& default_pos,
                                 float handle_h, float exclude_left, float exclude_right)
{
    struct State
    {
        ImVec2 target{};
        ImVec2 shown{};
        ImVec2 grab{};
        bool   dragging = false;
        bool   lmb_prev = false;
        bool   inited   = false;
    };
    static std::map<ImGuiID, State> s_states;
    State& st = s_states[ImHashStr(id)];

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 sz(ImMax(size.x, 1.f), ImMax(size.y, 1.f));
    if (!st.inited)
    {
        st.target = st.shown = default_pos;
        st.inited = true;
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool lmb = ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                     ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    const bool pressed = lmb && !st.lmb_prev;
    st.lmb_prev = lmb;

    float hh = handle_h;
    if (hh <= 0.f)
        hh = sz.y;
    const float hx0 = st.shown.x + exclude_left;
    const float hy0 = st.shown.y;
    const float hx1 = st.shown.x + sz.x - exclude_right;
    const float hy1 = st.shown.y + hh;
    const bool over = mouse.x >= hx0 && mouse.x < hx1 && mouse.y >= hy0 && mouse.y < hy1;

    if (pressed && over && !ImGui::IsAnyItemActive())
    {
        st.dragging = true;
        st.target   = st.shown;
        st.grab     = mouse - st.shown;
    }
    if (!lmb)
        st.dragging = false;

    if (st.dragging)
    {
        st.target.x = ImClamp(mouse.x - st.grab.x, 0.f, ImMax(0.f, display.x - sz.x));
        st.target.y = ImClamp(mouse.y - st.grab.y, 0.f, ImMax(0.f, display.y - sz.y));
        backdrop_blur::set_paused(true);
    }

    // Gelato dynamic_easing: lerp by speed/fps (same 18 as the main menu).
    const float fps = ImGui::GetIO().Framerate;
    const float a   = ImClamp(18.f / (fps > 1.f ? fps : 60.f), 0.f, 1.f);
    st.shown.x += (st.target.x - st.shown.x) * a;
    st.shown.y += (st.target.y - st.shown.y) * a;
    return ImFloor(st.shown);
}

// ---------------------------------------------------------------------------
//  DrawSatelliteChrome  (ESP preview / popup windows)
// ---------------------------------------------------------------------------
void MenuTheme::DrawSatelliteChrome(ImGuiWindow* window, const ImRect& bb, const char* title)
{
    ImDrawList* dl = window->DrawList;
    const bool is_players     = (title && strcmp(title, "Players")     == 0);
    const bool is_explorer    = (title && strcmp(title, "Explorer")    == 0);
    const bool is_esp_preview = (title && strcmp(title, "ESP Preview") == 0);
    const bool title_left     = is_players;

    if (is_esp_preview)
    {
        const float card_r = 16.f;
        const float header_h = kChildTitlebarH;
        DrawWindowGlow(bb.Min, bb.Max, card_r);
        AddRectFilledCrisp(dl, bb.Min, bb.Max, IM_COL32(23, 23, 31, 255), card_r);
        if (bb.Max.y > bb.Min.y + header_h)
            AddRectFilledCrisp(dl, ImVec2(bb.Min.x, bb.Min.y + header_h), bb.Max,
                IM_COL32(26, 27, 38, 255), card_r, ImDrawFlags_RoundCornersBottom);
        ImFont* hdr_font = Fonts::Bold ? Fonts::Bold : (Fonts::SemiBold ? Fonts::SemiBold : ImGui::GetFont());
        ImFont* hdr_reg  = Fonts::Regular ? Fonts::Regular : ImGui::GetFont();
        const float hdr_sz = FontPx(13.f);
        const float pill_sz = FontPx(12.f);
        float hx = ImFloor(bb.Min.x + 22.f);
        {
            const ImVec2 tsz = hdr_font->CalcTextSizeA(pill_sz, FLT_MAX, 0.f, "ESP");
            const float ty = ImFloor(bb.Min.y + (header_h - tsz.y) * 0.5f);
            const float pad_x = 7.f, pad_y = 2.f;
            const ImVec2 o0(IM_ROUND(hx - pad_x), IM_ROUND(ty - pad_y));
            const ImVec2 o1(IM_ROUND(hx + tsz.x + pad_x), IM_ROUND(ty + tsz.y + pad_y));
            const float r = IM_ROUND(ImMin((o1.y - o0.y) * 0.5f, pad_x));
            dl->AddRect(o0, o1, IM_COL32(0, 0, 0, 255), r, 0, 1.f);
            dl->AddText(hdr_font, pill_sz, ImVec2(hx, ty), IM_COL32(0, 0, 0, 255), "ESP");
            hx = ImFloor(o1.x + 16.f);
        }
        {
            const ImVec2 tsz = hdr_reg->CalcTextSizeA(hdr_sz, FLT_MAX, 0.f, "Preview");
            const float ty = ImFloor(bb.Min.y + (header_h - tsz.y) * 0.5f);
            dl->AddText(hdr_reg, hdr_sz, ImVec2(hx, ty), IM_COL32(122, 131, 165, 255), "Preview");
            hx = ImFloor(hx + tsz.x + 18.f);
        }
        {
            const ImVec2 tsz = hdr_font->CalcTextSizeA(pill_sz, FLT_MAX, 0.f, "3D");
            const float ty = ImFloor(bb.Min.y + (header_h - tsz.y) * 0.5f);
            const float pad_x = 7.f, pad_y = 2.f;
            const ImVec2 o0(IM_ROUND(hx - pad_x), IM_ROUND(ty - pad_y));
            const ImVec2 o1(IM_ROUND(hx + tsz.x + pad_x), IM_ROUND(ty + tsz.y + pad_y));
            const float r = IM_ROUND(ImMin((o1.y - o0.y) * 0.5f, pad_x));
            dl->AddRect(o0, o1, IM_COL32(0, 0, 0, 255), r, 0, 1.f);
            dl->AddText(hdr_font, pill_sz, ImVec2(hx, ty), IM_COL32(0, 0, 0, 255), "3D");
        }
        return;
    }

    DrawWindowGlow(bb.Min, bb.Max, kWindowRounding);
    AddRectFilledCrisp(dl, bb.Min, bb.Max, IM_COL32(23, 23, 31, 255), kWindowRounding);

    // Titlebar bg
    if (!is_esp_preview)
    {
        AddRectFilledCrisp(dl,
            ImVec2(bb.Min.x, bb.Min.y),
            ImVec2(bb.Max.x, bb.Min.y + kChildTitlebarH),
            ToU32(kBgColor), kWindowRounding, ImDrawFlags_RoundCornersTop);
        dl->AddLine(
            ImVec2(bb.Min.x+12.f, bb.Min.y+kChildTitlebarH-1.f),
            ImVec2(bb.Max.x-12.f, bb.Min.y+kChildTitlebarH-1.f),
            IM_COL32(255, 255, 255, 14), 1.f);
    }

    // Title
    if (title && title[0])
    {
        ImFont* font = Fonts::SemiBold ? Fonts::SemiBold : ImGui::GetFont();
        const float fsz = FontPx(Typography::kLabelSize);
        const ImVec2 tsz = font->CalcTextSizeA(fsz, FLT_MAX, 0.f, title);
        float tx;
        if (is_explorer || is_esp_preview)
            tx = bb.Min.x + (bb.Max.x - bb.Min.x - tsz.x) * 0.5f;
        else if (title_left)
            tx = bb.Min.x + 14.f;
        else
            tx = bb.Max.x - tsz.x - 14.f;
        const ImVec2 tpos { tx, bb.Min.y + (kChildTitlebarH - tsz.y) * 0.5f };
        dl->AddText(font, fsz, tpos, Typography::kLabelColor, title);
    }
}
// ---------------------------------------------------------------------------
//  BeginMainWindow
// ---------------------------------------------------------------------------
bool MenuTheme::BeginMainWindow(const char* id, bool* open,
                                ShellRects* out_rects, int* nav_page)
{
    const float   wW   = GetWindowW();
    const float   wH   = GetWindowH();
    const float   logo_h = GetLogoH();
    const float   top_h  = GetTopSelectH();
    const float   bar_h  = GetTopbarH();
    const float   gap_h  = Sz(2.f);
    const ImVec2  display = ImGui::GetIO().DisplaySize;

    // Drag from the logo cell; resize from edges/corners like Players / ESP Preview.
    // Mouse is ImGui client-space (same as TickSmoothDrag) so this works when
    // the overlay is snapped to the Roblox client rather than the full screen.
    static ImVec2 s_menu_pos(0.f, 0.f);
    static ImVec2 s_smooth_pos = s_menu_pos;
    static bool   s_initialized = false;
    static bool   s_dragging    = false;
    static ImVec2 s_drag_offset = ImVec2(0.f, 0.f);
    static bool   s_lmb_prev    = false;
    static int    s_resize_mask = 0; // 1=L, 2=R, 4=T, 8=B

    static float  s_last_scale  = -1.f;
    static int    s_layout_rev  = 0;
    const float   ui_scale = GetUIScale();
    static ImVec2 s_win_size(wW, wH);
    constexpr int kLayoutRev = 6;
    if (!s_initialized || s_last_scale != ui_scale || s_layout_rev != kLayoutRev)
    {
        s_menu_pos  = ImVec2(
            ImFloor(display.x * 0.5f - wW * 0.5f),
            ImFloor(display.y * 0.5f - wH * 0.5f - 60.f));
        s_smooth_pos  = s_menu_pos;
        s_win_size    = ImVec2(IM_ROUND(wW), IM_ROUND(wH));
        s_initialized = true;
        s_last_scale  = ui_scale;
        s_layout_rev  = kLayoutRev;
        s_resize_mask = 0;
        s_dragging    = false;
    }

    const float min_w = ImMax(420.f, wW * 0.55f);
    const float min_h = 320.f;
    const float edge  = 8.f;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float mx = mouse.x;
    const float my = mouse.y;
    const bool  lmb_down = ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                           ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    const bool  lmb_just_pressed = lmb_down && !s_lmb_prev;
    s_lmb_prev = lmb_down;

    const float wx0 = s_smooth_pos.x;
    const float wy0 = s_smooth_pos.y;
    const float wx1 = wx0 + s_win_size.x;
    const float wy1 = wy0 + s_win_size.y;

    const bool over_logo = mx >= wx0 && mx <= wx1 && my >= wy0 && my <= wy0 + Sz(32.f);
    const bool in_frame  = mx >= wx0 - 2.f && mx <= wx1 + 2.f && my >= wy0 - 2.f && my <= wy1 + 2.f;
    int hover_mask = 0;
    if (in_frame)
    {
        if (mx <= wx0 + edge) hover_mask |= 1;
        if (mx >= wx1 - edge) hover_mask |= 2;
        if (my <= wy0 + edge) hover_mask |= 4;
        if (my >= wy1 - edge) hover_mask |= 8;
    }

    if (lmb_just_pressed)
    {
        if (hover_mask)
        {
            s_resize_mask = hover_mask;
            s_dragging    = false;
            s_menu_pos    = s_smooth_pos;
        }
        else if (over_logo)
        {
            s_dragging    = true;
            s_resize_mask = 0;
            s_menu_pos    = s_smooth_pos;
            s_drag_offset = ImVec2(mx - s_smooth_pos.x, my - s_smooth_pos.y);
        }
    }
    if (!lmb_down)
    {
        s_dragging    = false;
        s_resize_mask = 0;
    }

    if (s_resize_mask)
    {
        ImVec2 pos = s_menu_pos;
        ImVec2 sz  = s_win_size;
        if (s_resize_mask & 2)
            sz.x = ImClamp(mx - pos.x, min_w, ImMax(min_w, display.x - pos.x));
        if (s_resize_mask & 8)
            sz.y = ImClamp(my - pos.y, min_h, ImMax(min_h, display.y - pos.y));
        if (s_resize_mask & 1)
        {
            const float new_x = ImClamp(mx, 0.f, pos.x + sz.x - min_w);
            sz.x  = pos.x + sz.x - new_x;
            pos.x = new_x;
        }
        if (s_resize_mask & 4)
        {
            const float new_y = ImClamp(my, 0.f, pos.y + sz.y - min_h);
            sz.y  = pos.y + sz.y - new_y;
            pos.y = new_y;
        }
        s_menu_pos   = ImVec2(IM_ROUND(pos.x), IM_ROUND(pos.y));
        s_smooth_pos = s_menu_pos;
        s_win_size   = ImVec2(IM_ROUND(sz.x), IM_ROUND(sz.y));
    }

    const int cursor_mask = s_resize_mask ? s_resize_mask : hover_mask;
    if (cursor_mask)
    {
        const bool nwse = ((cursor_mask & 5) == 5) || ((cursor_mask & 10) == 10);
        const bool nesw = ((cursor_mask & 6) == 6) || ((cursor_mask & 9) == 9);
        if (nwse)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
        else if (nesw)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
        else if (cursor_mask & 3)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        else
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    backdrop_blur::set_paused(s_dragging || s_resize_mask != 0);

    if (s_dragging)
    {
        s_menu_pos.x = ImClamp(mx - s_drag_offset.x, 0.f, ImMax(0.f, display.x - s_win_size.x));
        s_menu_pos.y = ImClamp(my - s_drag_offset.y, 0.f, ImMax(0.f, display.y - s_win_size.y));
    }
    else if (!s_resize_mask)
    {
        s_menu_pos.x = ImClamp(s_menu_pos.x, 0.f, ImMax(0.f, display.x - s_win_size.x));
        s_menu_pos.y = ImClamp(s_menu_pos.y, 0.f, ImMax(0.f, display.y - s_win_size.y));
    }

    if (!s_resize_mask)
    {
        const float fps   = ImGui::GetIO().Framerate;
        const float alpha = ImClamp(18.f / (fps > 1.f ? fps : 60.f), 0.f, 1.f);
        s_smooth_pos.x += (s_menu_pos.x - s_smooth_pos.x) * alpha;
        s_smooth_pos.y += (s_menu_pos.y - s_smooth_pos.y) * alpha;
        s_smooth_pos.x = IM_ROUND(s_smooth_pos.x);
        s_smooth_pos.y = IM_ROUND(s_smooth_pos.y);
    }

    s_win_size.x = IM_ROUND(s_win_size.x);
    s_win_size.y = IM_ROUND(s_win_size.y);
    ImGui::SetNextWindowSizeConstraints(ImVec2(min_w, min_h), display);
    ImGui::SetNextWindowSize(s_win_size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(IM_ROUND(s_smooth_pos.x), IM_ROUND(s_smooth_pos.y)), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,  0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,         ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,          ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,          ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,    ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Border,           ImVec4(0,0,0,0));
    // Pass nullptr for p_open so ImGui never auto-closes the window when
    // the user clicks outside (e.g. on the top icon bar pill).
    // Visibility is controlled externally via m_bMainWindowOpen.
    (void)open;
    const bool ok = ImGui::Begin(id, nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);

    if (!ok || !out_rects) return ok;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!s_resize_mask)
        s_win_size = window->Size;
    const ImVec2 wMin(IM_ROUND(window->Pos.x), IM_ROUND(window->Pos.y));
    const ImVec2 wMax(wMin.x + IM_ROUND(window->Size.x), wMin.y + IM_ROUND(window->Size.y));
    const ImRect wbb(wMin, wMax);

    // -- Rects ---------------------------------------------------------------
    // No sidebar column. Layout is:
    //   [  TOP BAR (logo + tabs)  ]  kTopSelectH tall, full width
    //   [  CONTENT                ]  full width, fills between top bar and topbar
    //   [  TOPBAR                 ]  kTopbarH tall, full width
    const ImRect top_select_rect(wMin, ImVec2(wMax.x, wMin.y + top_h));
    const ImRect topbar_rect(ImVec2(wMin.x, wMax.y - bar_h), wMax);
    const ImRect content_rect(
        ImVec2(wMin.x, wMin.y + top_h + gap_h),
        ImVec2(wMax.x, wMax.y - bar_h - gap_h));

    // sidebar_rect kept for back-compat (zero-width)
    const ImRect logo_rect(wMin, wMin + ImVec2(logo_h, top_h));
    const ImRect sidebar_rect(wMin, ImVec2(wMin.x, wMax.y));

    out_rects->window      = wbb;
    out_rects->logo        = logo_rect;
    out_rects->sidebar     = sidebar_rect;
    out_rects->top_select  = top_select_rect;
    out_rects->content     = content_rect;
    out_rects->topbar      = topbar_rect;
    out_rects->header      = top_select_rect;
    out_rects->tab_bar     = ImRect(content_rect.Min, content_rect.Min);
    out_rects->footer      = topbar_rect;

    const float card_r = IM_ROUND(settings::menu::panel_rounding > 0.f ? settings::menu::panel_rounding : kWindowRounding);
    DrawWindowGlow(wMin, wMax, card_r);

    ImDrawList* dl = window->DrawList;
    dl->PushClipRect(wMin, wMax, false);

    const ImU32 col_app    = IM_COL32(26, 27, 38, 255);
    const ImU32 col_chrome = IM_COL32(23, 23, 31, 255);
    const float header_h = IM_ROUND(Sz(32.f));
    const float footer_y = IM_ROUND(topbar_rect.Min.y);

    // Header and footer are the same color, so one rounded shell + a flat
    // mid-band. Corners are anti-aliased exactly once.
    AddRectFilledCrisp(dl, wMin, wMax, col_chrome, card_r);
    if (footer_y > wMin.y + header_h)
        dl->AddRectFilled(ImVec2(wMin.x, wMin.y + header_h), ImVec2(wMax.x, footer_y), col_app);
    dl->PopClipRect();

    ImFont* hdr_font = Fonts::Bold ? Fonts::Bold : (Fonts::SemiBold ? Fonts::SemiBold : ImGui::GetFont());
    ImFont* hdr_reg  = Fonts::Regular ? Fonts::Regular : ImGui::GetFont();
    const float hdr_sz = FontPx(13.f);
    const float matcha_sz = FontPx(15.f);
    const char* hdr_tabs[] = { "Matcha", "Interface", "Standard" };
    float hx = ImFloor(wMin.x + 22.f);
    const float hy = ImFloor(wMin.y + (header_h - hdr_sz) * 0.5f);
    for (int i = 0; i < 3; ++i)
    {
        ImFont* use_f = (i == 1) ? hdr_reg : hdr_font;
        const float use_sz = (i == 0) ? matcha_sz : hdr_sz;
        const ImVec2 tsz = use_f->CalcTextSizeA(use_sz, FLT_MAX, 0.f, hdr_tabs[i]);
        const float ty = ImFloor(wMin.y + (header_h - tsz.y) * 0.5f);
        if (i == 2)
        {
            const float oval_pad_x = IM_ROUND(Sz(10.f));
            const float oval_pad_y = IM_ROUND(Sz(4.f));
            const ImVec2 o0(IM_ROUND(hx - oval_pad_x), IM_ROUND(ty - oval_pad_y));
            const ImVec2 o1(IM_ROUND(hx + tsz.x + oval_pad_x), IM_ROUND(ty + tsz.y + oval_pad_y));
            const float oval_r = IM_ROUND(ImMin((o1.y - o0.y) * 0.5f, oval_pad_x));
            dl->AddRect(o0, o1, IM_COL32(0, 0, 0, 255), oval_r, 0, 1.f);
            dl->AddText(use_f, use_sz, ImVec2(hx, ty), IM_COL32(0, 0, 0, 255), hdr_tabs[i]);
        }
        else
        {
            const ImU32 col = (i == 0) ? IM_COL32(0, 0, 0, 255) : IM_COL32(122, 131, 165, 255);
            dl->AddText(use_f, use_sz, ImVec2(hx, ty), col, hdr_tabs[i]);
        }
        hx = ImFloor(hx + tsz.x + Sz(18.f));
    }
    {
        const char* det = "Dejected";
        const ImVec2 dsz = hdr_font->CalcTextSizeA(hdr_sz, FLT_MAX, 0.f, det);
        dl->AddText(hdr_font, hdr_sz, ImVec2(ImFloor(wMax.x - 22.f - dsz.x), hy), IM_COL32(122, 131, 165, 255), det);
    }

    if (nav_page)
    {
        const float nav_y0 = wMin.y + header_h;
        const float nav_h  = top_h - header_h;
        float nx = wMin.x + 22.f;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::SetCursorScreenPos(ImVec2(wMin.x, nav_y0));
        ImGui::BeginChild("##matcha_topbar_tabs", ImVec2(wMax.x - wMin.x, nav_h),
            ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            ImDrawList* tdl = ImGui::GetWindowDrawList();
            ImFont* nf = Fonts::SemiBold ? Fonts::SemiBold : ImGui::GetFont();
            const float nsz = FontPx(14.f);
            const float pill_pad_x = Sz(8.f);
            float tab_x[kSidebarCount];
            float tab_w[kSidebarCount];
            float tab_tw[kSidebarCount];
            float text_sum = 0.f;
            for (int i = 0; i < kSidebarCount; ++i)
            {
                tab_tw[i] = nf->CalcTextSizeA(nsz, FLT_MAX, 0.f, kSectionLabels[i]).x;
                tab_w[i] = tab_tw[i];
                text_sum += tab_tw[i];
            }

            // Line Teams up with Silent Aim's FOV tab (right card, third inner tab).
            const float body_pad = Sz(10.f);
            const float body_x = wMin.x + body_pad;
            const float body_w = (wMax.x - wMin.x) - body_pad * 2.f;
            const float col_gap = 18.f;
            const float col_w = ImTrunc((body_w - col_gap) * 0.5f);
            const float inner_sz = FontPx(13.f);
            const float sa_w = nf->CalcTextSizeA(inner_sz, FLT_MAX, 0.f, "Silent Aim").x;
            const float pred_w = nf->CalcTextSizeA(inner_sz, FLT_MAX, 0.f, "Prediction").x;
            const float fov_x = body_x + col_w + col_gap + 14.f + sa_w + 16.f + pred_w + 16.f;
            const float span = ImMax(fov_x - nx, text_sum);
            const float gap = (kSidebarCount > 1)
                ? ImMax(Sz(18.f), (span - (text_sum - tab_tw[kSidebarCount - 1])) / (float)(kSidebarCount - 1))
                : Sz(18.f);
            float cursor = nx;
            for (int i = 0; i < kSidebarCount; ++i)
            {
                tab_x[i] = cursor;
                cursor += tab_tw[i] + gap;
            }

            const float pill_h = IM_ROUND(Sz(22.f));
            const float tgt_rel = (tab_x[*nav_page] - pill_pad_x) - wMin.x;
            const float tgt_w = tab_w[*nav_page] + pill_pad_x * 2.f;
            static float s_pill_rel = tgt_rel;
            static float s_pill_w = tgt_w;
            s_pill_rel = SmoothLerp(s_pill_rel, tgt_rel, 14.f);
            s_pill_w = SmoothLerp(s_pill_w, tgt_w, 14.f);
            const float pill_x = wMin.x + s_pill_rel;
            const float pill_y = IM_ROUND(nav_y0 + (nav_h - pill_h) * 0.5f);
            AddRectFilledCrisp(tdl,
                ImVec2(IM_ROUND(pill_x), pill_y),
                ImVec2(IM_ROUND(pill_x + s_pill_w), pill_y + pill_h),
                IM_COL32(42, 48, 60, 255), pill_h * 0.5f);

            for (int i = 0; i < kSidebarCount; ++i)
            {
                const ImVec2 tmin(tab_x[i] - pill_pad_x, nav_y0);
                const ImVec2 tmax(tab_x[i] + tab_w[i] + pill_pad_x, nav_y0 + nav_h);
                const ImRect tbb(tmin, tmax);
                const ImGuiID tid = ImGui::GetID(kSectionLabels[i]);
                ImGui::SetCursorScreenPos(tmin);
                ImGui::ItemSize(tmax - tmin);
                bool hovered = false, held = false, pressed = false;
                if (ImGui::ItemAdd(tbb, tid))
                    pressed = ImGui::ButtonBehavior(tbb, tid, &hovered, &held);
                if (pressed) *nav_page = i;
                const ImU32 col = (*nav_page == i) ? IM_COL32(122, 131, 165, 255)
                    : hovered ? IM_COL32(122, 131, 165, 200)
                    : IM_COL32(64, 70, 102, 255);
                tdl->AddText(nf, nsz, ImVec2(ImFloor(tab_x[i]), ImFloor(nav_y0 + (nav_h - nsz) * 0.5f)), col, kSectionLabels[i]);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }

    return true;
}

// ---------------------------------------------------------------------------
//  DrawMainChrome -- just the bottom topbar (version + active)
// ---------------------------------------------------------------------------
void MenuTheme::DrawMainChrome(const ShellRects& rects, int* nav_page)
{
    (void)nav_page;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font    = ImGui::GetFont();
    ImFont* body    = MenuTheme::Fonts::Regular ? MenuTheme::Fonts::Regular : font;
    const float fsz = FontPx(11.f);
    const float ty  = ImFloor(rects.topbar.Min.y + (GetTopbarH() - fsz) * 0.5f);

    const ImU32 foot = IM_COL32(122, 131, 165, 255);
    {
        const char* left = "67M online";
        dl->AddCircleFilled(ImVec2(ImFloor(rects.topbar.Min.x + 22.f), ImFloor(ty + fsz * 0.45f)), 4.f, IM_COL32(160, 160, 172, 255), 12);
        dl->AddText(body, fsz, ImVec2(ImFloor(rects.topbar.Min.x + 32.f), ty), foot, left);
    }
    {
        const char* disc = "matcha.pink/discord";
        const float disc_sz = FontPx(13.f);
        const ImVec2 dsz = body->CalcTextSizeA(disc_sz, FLT_MAX, 0.f, disc);
        const float  dx  = ImFloor(rects.topbar.Min.x + (rects.topbar.Max.x - rects.topbar.Min.x - dsz.x) * 0.5f);
        const float  dy  = ImFloor(rects.topbar.Min.y + (GetTopbarH() - dsz.y) * 0.5f);
        dl->AddText(body, disc_sz, ImVec2(dx, dy), foot, disc);
    }
    {
        const char* prefix = "Build: ";
        const char* date = "Aug 22 2026";
        const ImVec2 psz = body->CalcTextSizeA(fsz, FLT_MAX, 0.f, prefix);
        const ImVec2 dsz = body->CalcTextSizeA(fsz, FLT_MAX, 0.f, date);
        const float bx = ImFloor(rects.topbar.Max.x - psz.x - dsz.x - 18.f);
        dl->AddText(body, fsz, ImVec2(bx, ty), foot, prefix);
        dl->AddText(body, fsz, ImVec2(ImFloor(bx + psz.x), ty), IM_COL32(0, 0, 0, 255), date);
    }
}

// ---------------------------------------------------------------------------
//  DrawTopIconBar â€” top-CENTER pill drawn on the foreground drawlist.
//  Clicks handled via raw Win32 (works regardless of ImGui window z-order).
//  Icons match blushes.lol: house=Combat(0), person=Players(3), code=Settings(5)

// ---------------------------------------------------------------------------
//  SVG-sourced icon helpers  (Remixicon, scaled to sz x sz centred on c)
// ---------------------------------------------------------------------------

// home-2-fill  -- roof chevron (6-point ring) + solid body
static void DrawIconHouse(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
{
    auto S = [&](float x, float y) -> ImVec2 {
        return ImVec2(c.x + (x/24.f - 0.5f)*sz, c.y + (y/24.f - 0.5f)*sz);
    };
    // Roof chevron as a single 6-point polygon (outer tip -> outer sides -> inner sides)
    ImVec2 roof[6] = {
        S(12.f,  2.69f),   // apex
        S(22.08f,9.47f),   // outer right
        S(20.92f,11.10f),  // inner right
        S(12.f,  4.73f),   // inner apex
        S(3.08f, 11.10f),  // inner left
        S(1.92f, 9.47f)    // outer left
    };
    dl->AddConvexPolyFilled(roof, 6, col);
    // Body
    ImVec2 body[6] = {
        S(12.f,7.69f), S(19.58f,12.69f), S(20.f,13.5f),
        S(20.f,21.f),  S(4.f,21.f),      S(4.f,13.5f)
    };
    dl->AddConvexPolyFilled(body, 6, col);
}

// user-fill  -- circle head + arc shoulders body
static void DrawIconPerson(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
{
    auto S = [&](float x, float y) -> ImVec2 {
        return ImVec2(c.x + (x/24.f - 0.5f)*sz, c.y + (y/24.f - 0.5f)*sz);
    };
    // Head: circle at (12,7) r=6
    dl->AddCircleFilled(S(12.f,7.f), 6.f/24.f*sz, col, 32);
    // Body: SVG "M4 22 C4 17.58 7.58 14 12 14 C16.42 14 20 17.58 20 22"
    // Approximate with a smooth arc polygon (shoulder curve)
    const int segs = 24;
    ImVector<ImVec2> body;
    body.push_back(S(4.f, 22.f));
    // Left arc: from (4,22) curving up to (12,14) -- cubic bezier P0(4,22) P1(4,14) P2(12,14) P3(12,14)
    for (int i = 1; i < segs/2; ++i) {
        float t  = (float)i / (segs/2);
        float t2 = t*t, t3 = t2*t, s = 1.f-t, s2 = s*s, s3 = s2*s;
        float x  = s3*4.f  + 3*s2*t*4.f  + 3*s*t2*12.f + t3*12.f;
        float y  = s3*22.f + 3*s2*t*14.f + 3*s*t2*14.f + t3*14.f;
        body.push_back(S(x, y));
    }
    body.push_back(S(12.f, 14.f));
    // Right arc: from (12,14) to (20,22) -- cubic bezier P0(12,14) P1(12,14) P2(20,14) P3(20,22)
    for (int i = 1; i < segs/2; ++i) {
        float t  = (float)i / (segs/2);
        float t2 = t*t, t3 = t2*t, s = 1.f-t, s2 = s*s, s3 = s2*s;
        float x  = s3*12.f + 3*s2*t*12.f + 3*s*t2*20.f + t3*20.f;
        float y  = s3*14.f + 3*s2*t*14.f + 3*s*t2*14.f + t3*22.f;
        body.push_back(S(x, y));
    }
    body.push_back(S(20.f, 22.f));
    dl->AddConvexPolyFilled(body.Data, body.Size, col);
}

// earth/globe  (Explorer tab) -- circle + meridians + equator
static void DrawIconExplorer(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
{
    auto S = [&](float x, float y) -> ImVec2 {
        return ImVec2(c.x + (x/24.f - 0.5f)*sz, c.y + (y/24.f - 0.5f)*sz);
    };
    const float th = ImMax(1.5f, sz * 0.065f);
    const float r  = 9.5f/24.f * sz;
    const float pi = 3.14159265f;
    dl->AddCircle(S(12.f,12.f), r, col, 48, th);
    dl->AddLine(S(2.5f,12.f),  S(21.5f,12.f), col, th);
    dl->AddLine(S(12.f,2.5f),  S(12.f,21.5f), col, th);
    // Upper latitude arc
    for (int side = -1; side <= 1; side += 2) {
        const int segs = 20;
        ImVec2 prev = S(12.f, 2.5f);
        for (int i = 1; i <= segs; ++i) {
            float t   = (float)i/segs;
            float a   = pi*t;
            ImVec2 cur = S(12.f + side*4.8f*sinf(a), 2.5f+19.f*t);
            dl->AddLine(prev, cur, col, th);
            prev = cur;
        }
    }
    // Extra latitude ring at y=8 and y=16
    for (float ry : {8.f, 16.f}) {
        float half = sqrtf(ImMax(0.f, r*r - ((ry/24.f-0.5f)*sz)*((ry/24.f-0.5f)*sz)));
        dl->AddLine(
            ImVec2(c.x - half, c.y + (ry/24.f - 0.5f)*sz),
            ImVec2(c.x + half, c.y + (ry/24.f - 0.5f)*sz),
            col, th);
    }
}

// pencil/brush icon  -- fat diagonal pencil with tip + eraser
static void DrawIconBrush(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
{
    // 45-degree pencil: tip bottom-left, eraser top-right
    const float hw  = sz * 0.09f;   // half shaft width
    const float len = sz * 0.36f;   // half shaft length
    const float dx  = -0.7071f, dy = 0.7071f;   // shaft axis (225 deg)
    const float px  =  0.7071f, py = 0.7071f;   // perpendicular
    const float cx  = c.x + sz*0.03f, cy = c.y - sz*0.03f;
    // Shaft quad
    ImVec2 tl(cx + (-len)*dx - hw*px, cy + (-len)*dy - hw*py);
    ImVec2 tr(cx + (-len)*dx + hw*px, cy + (-len)*dy + hw*py);
    ImVec2 br(cx + ( len)*dx + hw*px, cy + ( len)*dy + hw*py);
    ImVec2 bl(cx + ( len)*dx - hw*px, cy + ( len)*dy - hw*py);
    ImVec2 shaft[4] = { tl, tr, br, bl };
    dl->AddConvexPolyFilled(shaft, 4, col);
    // Tip: sharp triangle extending from shaft end
    ImVec2 tip(cx + (len + sz*0.16f)*dx, cy + (len + sz*0.16f)*dy);
    dl->AddTriangleFilled(bl, br, tip, col);
    // Eraser: contrasting filled cap at top
    const float capL = sz * 0.09f;
    ImVec2 el(cx + (-len - capL)*dx - hw*0.9f*px, cy + (-len - capL)*dy - hw*0.9f*py);
    ImVec2 er(cx + (-len - capL)*dx + hw*0.9f*px, cy + (-len - capL)*dy + hw*0.9f*py);
    ImVec2 cap[4] = { el, er, tr, tl };
    dl->AddConvexPolyFilled(cap, 4, col);
    // Dividing line between shaft and eraser
    dl->AddLine(tl, tr, IM_COL32(12,12,16,200), ImMax(1.f, sz*0.04f));
}

// ---------------------------------------------------------------------------
//  DrawTopIconBar  --  top-CENTER pill, 4 buttons
//  house=Combat(0)  person=Players(overlay)  globe=Explorer(overlay)  pencil=Theme(overlay)
// ---------------------------------------------------------------------------
void MenuTheme::DrawTopIconBar(int* nav_page)
{
    if (!nav_page) return;

    constexpr int          kCount    = 4;
    constexpr float        kBtnSz   = 28.f;   // hit-area per button
    constexpr float        kGap     = 8.f;    // spacing between buttons
    constexpr float        kPadH    = 10.f;   // horizontal pill padding
    constexpr float        kPadV    = 6.f;    // vertical pill padding
    constexpr float        kRound   = 16.f;

    const float bar_w = kPadH * 2.f + kCount * kBtnSz + (kCount - 1) * kGap;
    const float bar_h = kPadV * 2.f + kBtnSz;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 bp(ImFloor(display.x * 0.5f - bar_w * 0.5f), 10.f);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const float ui_a = ImClamp(ImGui::GetStyle().Alpha, 0.f, 1.f);
    const int pill_a = (int)(240.f * ui_a);
    const int stroke_a = (int)(14.f * ui_a);
    DrawWindowGlow(bp, bp + ImVec2(bar_w, bar_h), kRound);
    backdrop_blur::fill(dl, bp, bp + ImVec2(bar_w, bar_h), kRound, IM_COL32(20, 20, 26, pill_a));
    dl->AddRect(bp, bp + ImVec2(bar_w, bar_h), IM_COL32(255, 255, 255, stroke_a), kRound, 0, 1.f);

    POINT cur{}; GetCursorPos(&cur);
    const bool lbtn = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    static bool s_lbtn_prev = false;
    const bool clicked_this_frame = lbtn && !s_lbtn_prev;
    s_lbtn_prev = lbtn;

    for (int i = 0; i < kCount; ++i)
    {
        const ImVec2 bmin(bp.x + kPadH + i * (kBtnSz + kGap), bp.y + kPadV);
        const ImVec2 bmax(bmin.x + kBtnSz, bmin.y + kBtnSz);
        const bool hov = (cur.x >= (int)bmin.x && cur.x <= (int)bmax.x &&
                          cur.y >= (int)bmin.y && cur.y <= (int)bmax.y);
        if (!hov || !clicked_this_frame)
            continue;
        if (i == 0)
            *nav_page = (*nav_page == 0) ? -1 : 0;
        else if (i == 1)
            show_players = !show_players;
        else if (i == 2)
            show_explorer = !show_explorer;
        else
            show_theme = !show_theme;
    }
}

// ---------------------------------------------------------------------------
//  EndMainWindow
// ---------------------------------------------------------------------------
void MenuTheme::EndMainWindow()
{
    ImGui::End();
}


