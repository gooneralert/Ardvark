#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <d3d11.h>

struct ImGuiWindow;

namespace MenuTheme
{
    // -------------------------------------------------------------------------
    // Layout — based on "Cheat ImGui" reference (740 x 530), widened slightly
    // to absorb the extra horizontal breathing room in Typography/style padding
    // -------------------------------------------------------------------------
    constexpr float kWindowW      = 820.f;
    constexpr float kWindowH      = 1000.f;

    // Scale helpers — multiply base dimensions by settings::menu::menu_scale so
    // stretch-resolution users can shrink the menu without recompiling.
    // Implemented in menu_theme.cpp (not inline) to avoid pulling settings.h
    // into every translation unit that includes this header.
    float GetWindowW();
    float GetWindowH();
    // Resolution * menu_scale. Buttons/sliders/chrome all multiply by this.
    float GetUIScale();
    inline float Sz(float px) { return px * GetUIScale(); }
    // Whole-pixel font size so FreeType never bilinear-scales a fractional glyph.
    inline float FontPx(float px) { return IM_ROUND(Sz(px)); }
    float GetLogoH();
    float GetTopSelectH();
    float GetTopbarH();
    float GetChildTitlebarH();
    void  ApplyLayoutScale();
    void  BeginContentFit();
    void  EndContentFit();
    void  NoteContentHeight(float h);
    bool  IsFittingContent();
    bool  HidePanelScroll();
    constexpr float kSidebarW     = 0.f;   // sidebar removed -- tabs are in the top bar
    constexpr float kLogoH        = 0.f;
    constexpr float kTopSelectH   = 66.f;  // header row + text nav row
    constexpr float kTopbarH      = 32.f;   // footer
    constexpr float kSectionH     = 72.f;   // each sidebar section icon cell
    constexpr float kWindowRounding = 16.f;
    constexpr float kChildRounding  = 12.f;
    constexpr float kChildTitlebarH = 28.f; // child panel titlebar height

    // Back-compat aliases
    constexpr float kHeaderHeight = kTopSelectH;
    constexpr float kFooterHeight = kTopbarH;
    constexpr float kTabBarHeight = 0.f;
    constexpr float kPanelRounding = kChildRounding;

    // Dark widget surfaces — tabs, dropdowns, unselected buttons
    namespace Surfaces
    {
        constexpr ImU32  kTabUnselected = IM_COL32(36, 36, 46, 0);
        constexpr ImU32  kTabSelected   = IM_COL32(48, 48, 60, 200);
        constexpr ImVec4 kBg            { 32/255.f, 32/255.f, 40/255.f, 1.f };
        constexpr ImVec4 kBgHover       { 40/255.f, 40/255.f, 50/255.f, 1.f };
        constexpr ImVec4 kBgActive      { 48/255.f, 48/255.f, 60/255.f, 1.f };
        constexpr ImVec4 kPopupBg       { 28/255.f, 28/255.f, 36/255.f, 0.98f };
    }

    // -------------------------------------------------------------------------
    // Reference colors (from colors.h)
    // -------------------------------------------------------------------------
    // accent:           ImColor(255,129,216)  pink
    // bg:               ImColor(1,2,1)        near-black
    // child.background: ImColor(0,0,1)        very dark (0,0,1)
    // child.stroke:     ImColor(2,2,2)        near-black stroke
    // child.line:       accent color
    // section.icon:     ImColor(79,85,118)    slate-blue inactive
    // topbar name:      ImColor(255,255,255)
    // topbar year:      ImColor(75,83,117)

    // -------------------------------------------------------------------------
    // Fonts — Inter Regular + Inter SemiBold, loaded once in Menu::Initialize().
    // Draw at FontPx() sizes so glyphs stay pixel-snapped.
    // -------------------------------------------------------------------------
    namespace Fonts
    {
        inline ImFont* Regular  = nullptr; // body text — labels, values, general widgets
        inline ImFont* SemiBold = nullptr; // section headers
        inline ImFont* Bold     = nullptr; // tab headers
    }

    // -------------------------------------------------------------------------
    // Typography — visual hierarchy tokens for menu text
    //   Tab header    → section-box title (e.g. "AIMBOT" in the panel title bar)
    //   Section header→ in-content group label (e.g. "TARGET COLOR", "LIGHTING")
    //   Label         → widget label (e.g. "Enable", "FOV")
    //   Value         → widget value readout (e.g. a slider's current number)
    // -------------------------------------------------------------------------
    namespace Typography
    {
        constexpr ImU32  kTabHeaderColor      = IM_COL32(210, 210, 216, 255);
        constexpr float  kTabHeaderSize       = 13.f;
        constexpr float  kTabHeaderLetterSpacing = 0.6f;
        constexpr float  kTabHeaderPadBottom  = 10.f;

        constexpr ImU32  kSectionHeaderColor      = IM_COL32(168, 168, 176, 255);
        constexpr float  kSectionHeaderSize       = 12.f;
        constexpr ImU32  kSectionHeaderUnderline  = IM_COL32(255, 255, 255, 18);
        constexpr float  kSectionHeaderPadBottom  = 8.f;

        constexpr ImU32  kLabelColor = IM_COL32(122, 131, 165, 255);
        constexpr float  kLabelSize  = 13.f;

        constexpr ImU32  kValueColor = IM_COL32(236, 236, 244, 255);
        constexpr float  kValueSize  = 12.f;

        // "Active"/"Disabled" status badge shown at the far right of a tab header
        constexpr float  kBadgeSize        = 11.f;
        constexpr ImU32   kBadgeDisabledColor = IM_COL32(102, 102, 102, 255); // #666666

        // Sub-tab strip labels (e.g. "Main"/"Settings") — sized between the
        // widget-label size and the section's tab-header size
        constexpr float  kSubTabSize = 12.f;

        constexpr float  kRowSpacing     = 9.f; // vertical gap between widget rows
        constexpr float  kSectionSpacing = 10.f;  // gap between sibling section boxes
    }

    // In-content section header: label text at kSectionHeaderSize/kSectionHeaderColor
    // with a thin 20%-opacity underline beneath it, followed by kSectionHeaderPadBottom
    // of vertical space. Replaces the old ImGui::TextDisabled("X") + Spacing() pattern.
    void SectionHeader(const char* label);

    // Draws `text` letter-spaced by `spacing` pixels, left-aligned at `pos`.
    // Returns the total rendered width (advance sum), useful for centering callers.
    float AddTextSpaced(ImDrawList* dl, ImFont* font, float font_size, ImVec2 pos, ImU32 col, const char* text, float spacing);
    // Measures the width AddTextSpaced() would render at, without drawing.
    float CalcSpacedTextWidth(ImFont* font, float font_size, const char* text, float spacing);

    // -------------------------------------------------------------------------
    // Dynamic accent  (synced from settings::menu::accent_color each frame)
    // -------------------------------------------------------------------------
    void   ApplyAccent();
    ImVec4 GetAccentVec4();
    ImU32  ColAccent();
    ImU32  ColAccentSoft();

    // -------------------------------------------------------------------------
    // Static palette
    // -------------------------------------------------------------------------
    ImU32 ColSurface();
    ImU32 ColSurfaceElevated();
    ImU32 ColBorder();
    ImU32 ColBorderStrong();
    ImU32 ColTextMuted();
    ImU32 ColTextDim();

    // -------------------------------------------------------------------------
    // Full ImGui style  (called once on init)
    // -------------------------------------------------------------------------
    void Apply(ImGuiIO& io);

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
    // Pixel-snapped rounded fill with dense circular arcs (not ImGui's 4-step
    // PathArcToFast). Use this for every visible corner so they stay crisp.
    void AddRectFilledCrisp(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float rounding, ImDrawFlags flags = 0);
    void DrawBrandMark(ImDrawList* draw_list, ImVec2 pos, float scale = 1.f);
    void DrawWatermarkShell(ImDrawList* draw_list, const ImRect& bb, const char* text, ImU32 text_col);
    void DrawCursorSpotlight(ImDrawList* draw_list, const ImRect& window_bb);

    // Sub-tab bar inside child panels
    bool TopTab(const char* id, const char* label, bool selected, float fixed_w = 0.f);
    void DrawSubTabBar(const char* const* labels, int count, int* active_tab);

    // Satellite popup
    void DrawSatelliteChrome(ImGuiWindow* window, const ImRect& bb, const char* title);

    // Same gaussian halo as widget buttons (DrawGlowRectBg), white edge aura.
    void DrawWindowGlow(const ImVec2& bb_min, const ImVec2& bb_max, float rounding);
    // Music-player glow: same edge aura, brightness follows eq_bands while playing.
    void DrawWindowGlowReactive(const ImVec2& bb_min, const ImVec2& bb_max, float rounding, const float eq_bands[10]);

    // -------------------------------------------------------------------------
    // Child panel chrome  (drawn around ImAdd::BeginChild panels)
    // Draws: outer stroke, filled bg, accent bottom-line on titlebar,
    //        flame icon + label + gear icon.
    // Call BEFORE BeginChild. Returns the inner content rect.
    // -------------------------------------------------------------------------
    void DrawChildChrome(ImDrawList* dl, ImVec2 pos, ImVec2 size, const char* label);

    // -------------------------------------------------------------------------
    // Main window shell
    // -------------------------------------------------------------------------
    struct ShellRects
    {
        ImRect window;
        ImRect logo;        // top-left logo cell
        ImRect sidebar;     // section icons below logo
        ImRect top_select;  // Favorites/Standard + GUI SIZE + hue slider row
        ImRect content;     // main content area
        ImRect topbar;      // bottom name+year bar

        // back-compat
        ImRect header;
        ImRect tab_bar;
        ImRect footer;
    };

    bool BeginMainWindow(const char* id, bool* open, ShellRects* out_rects, int* nav_page = nullptr);
    void EndMainWindow();
    void DrawMainChrome(const ShellRects& rects, int* nav_page = nullptr);
    void DrawTopIconBar(int* nav_page);

    // Gelato-style drag: target follows the cursor, displayed pos eases toward it.
    // Call before Begin, then SetNextWindowPos(Always) and Begin with NoMove.
    // handle_h is the top drag strip (0 = full window). exclude_* skip title buttons.
    ImVec2 TickSmoothDrag(const char* id, const ImVec2& size, const ImVec2& default_pos,
                          float handle_h = 0.f, float exclude_left = 0.f, float exclude_right = 0.f);

    // Independent overlay toggles — each panel stays open until its button is clicked again.
    inline bool show_players  = false;  // person button (Players panel)
    inline bool show_explorer = false;  // globe button  (Explorer panel)
    inline bool show_theme    = false;  // pencil button (Theme customization panel)

    // Dropdown arrow icon texture — loaded once from g_triangle_arrow.h
    inline ID3D11ShaderResourceView* combo_arrow_srv = nullptr;
}
