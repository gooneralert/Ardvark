#include "pch.h"
#include "gui.h"
#include "lua_window.h"
#include "players_window.h"
#include "explorer_window.h"
#include "servers_window.h"
#include "esp_preview_window.h"
#include "tabs/aim.h"
#include "tabs/esp.h"
#include "tabs/misc.h"
#include "tabs/helpers.h"
#include "resources/fonts/fonts.h"
#include "tabs/local.h"
#include "tabs/settings_tab.h"
#include "tabs/customize.h"
#include "tabs/trigger.h"
#include "app/Settings.h"
#include "core/config/Config.h"
#include "core/globals/Globals.h"
#include "core/console/Console.h"
#include "core/roblox/classes/Classes.h"
#include "features/games/PhantomForces.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "widgets/widgets.h"
#include "widgets/text.h"
#include "glass.h"
#include "liquid_ui.h"      // LiquidUI glass kit: menu card, sidebar nav, widgets
#include "music_player_ui.h"
#include "media.h"
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>

namespace gui
{
    constexpr float content_margin = 3.f;
    constexpr float inner_padding = 12.f;
    constexpr float subtab_margin = 6.f;
    constexpr float navbar_height = 28.f;

    const ImVec4 border_color_outer = ImVec4(0.92f, 0.94f, 0.93f, 1.f); // matcha near-white
    const ImVec4 border_color_inner = ImVec4(0.18f, 0.18f, 0.18f, 1.f);

    // sidebar pages, in the reference's order and grouping
    enum : int
    {
        page_aimbot = 0, page_silent, page_trigger,
        page_players, page_extras, page_world, page_character,
        page_npc, page_teams, page_options, page_customize, page_count
    };

    static const char* k_page_titles[page_count] = {
        "Aimbot", "Silent", "Trigger", "Players", "Extras", "World",
        "Character", "NPC", "Teams", "Options", "Customize"
    };

    // sidebar model: group headers plus pages, with the visuals sub-pages
    // (Players / Extras) nested under Visuals - the reference's arrangement
    struct NavRow { const char* group; const char* label; Glass::Icon icon; int page; bool sub; };
    static const NavRow k_nav[] = {
        { "AIMBOT", "Aimbot",    Glass::Icon::Crosshairs, page_aimbot,    false },
        { nullptr,  "Silent",    Glass::Icon::Person,     page_silent,    false },
        { nullptr,  "Trigger",   Glass::Icon::Bolt,       page_trigger,   false },

        { "COMMON", "Visuals",   Glass::Icon::Eye,        page_players,   false },
        { nullptr,  "Players",   Glass::Icon::Users,      page_players,   true  },
        { nullptr,  "Extras",    Glass::Icon::Gem,        page_extras,    true  },
        { nullptr,  "World",     Glass::Icon::Globe,      page_world,     false },
        { nullptr,  "Character", Glass::Icon::Running,    page_character, false },
        { nullptr,  "NPC",       Glass::Icon::Box,        page_npc,       false },
        { nullptr,  "Teams",     Glass::Icon::Flag,       page_teams,     false },
        { nullptr,  "Options",   Glass::Icon::Sliders,    page_options,   false },
        { nullptr,  "Customize", Glass::Icon::Paintbrush, page_customize, false },
    };
    static const int k_nav_count = (int)(sizeof(k_nav) / sizeof(k_nav[0]));

    static int  s_sidebar_selected = 0;
    static bool s_menu_open = true;
    static bool lua_open = false;
    static bool players_open = false;
    static bool explorer_open = false;
    static bool servers_open = false;
    static bool esp_preview_open = false;
    static bool music_open = false;
    static bool music_media_inited = false;
    static float s_esp_anim = 0.f;   // esp preview slide-out 0..1
    static ImVec2 s_menu_pos{};
    static ImVec2 s_menu_size{};
    static int menu_kb = VK_DELETE;
    static bool menu_kb_skip = false;

    bool menu_visible()
    {
        return s_menu_open;
    }

    void set_menu_visible(bool v)
    {
        s_menu_open = v;
    }

    bool any_window_visible()
    {
        return s_menu_open;
    }

    bool menu_open() { return menu_visible(); }
    void set_menu_open(bool open) { set_menu_visible(open); }
    bool any_ui_open() { return any_window_visible(); }
    bool music_visible() { return music_open; }

    ImVec2 menu_pos() { return s_menu_pos; }

    static bool rect_contains(ImVec2 mn, ImVec2 mx, float x, float y)
    {
        return x >= mn.x && x <= mx.x && y >= mn.y && y <= mx.y;
    }

    static bool window_contains(const char* name, float x, float y)
    {
        ImGuiWindow* w = ImGui::FindWindowByName(name);
        if (!w || !w->Active || w->Hidden)
            return false;
        return rect_contains(w->Pos, ImVec2(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y), x, y);
    }

    bool point_over_ui(float x, float y)
    {
        // the watermark is draggable, so the cursor must count as over the UI there
        if (widgets::watermark_hit_test(x, y))
            return true;

        static const char* names[] = {
            "##navbar",
            "menu",
            "##lua_window",
            "##lua_errors",
            "##players_window",
            "##explorer_window",
            "##servers_window",
            "##esp_preview_window",
            "##properties_window",
            "##decompiled_window",
            "##spotify_player",
        };
        for (const char* n : names)
        {
            if (window_contains(n, x, y))
                return true;
        }
        return false;
    }

    void setup_style()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* c = style.Colors;

        style.WindowPadding     = ImVec2(12.f, 12.f);
        style.FramePadding      = ImVec2(8.f, 4.f);
        style.CellPadding       = ImVec2(6.f, 4.f);
        style.ItemSpacing       = ImVec2(8.f, 6.f);
        style.ItemInnerSpacing  = ImVec2(6.f, 4.f);
        style.IndentSpacing     = 18.f;
        style.ScrollbarSize     = 0.f;
        style.GrabMinSize       = 10.f;

        style.WindowRounding    = 8.f;
        style.ChildRounding     = 8.f;
        style.FrameRounding     = 6.f;
        style.PopupRounding     = 8.f;
        style.ScrollbarRounding = 6.f;
        style.GrabRounding      = 4.f;
        style.TabRounding       = 6.f;

        style.WindowBorderSize  = 1.f;
        style.FrameBorderSize   = 1.f;
        style.PopupBorderSize   = 1.f;
        style.WindowMinSize     = ImVec2(0.f, 0.f);

        const ImVec4 bg        = ImVec4(0.08f, 0.08f, 0.08f, 1.f);
        const ImVec4 bg2       = ImVec4(0.10f, 0.10f, 0.10f, 1.f);
        const ImVec4 bg3       = ImVec4(0.12f, 0.12f, 0.12f, 1.f);
        const ImVec4 border    = ImVec4(0.22f, 0.22f, 0.22f, 1.f);
        const ImVec4 text      = ImVec4(0.90f, 0.90f, 0.90f, 1.f);
        const ImVec4 text_dim  = ImVec4(0.55f, 0.55f, 0.55f, 1.f);
        const ImVec4 fill      = ImVec4(1.f, 1.f, 1.f, 0.10f);
        const ImVec4 fill_h    = ImVec4(1.f, 1.f, 1.f, 0.14f);
        const ImVec4 fill_a    = ImVec4(1.f, 1.f, 1.f, 0.18f);
        const ImVec4 grab      = ImVec4(0.75f, 0.75f, 0.75f, 1.f);

        c[ImGuiCol_Text]                  = text;
        c[ImGuiCol_TextDisabled]          = text_dim;
        c[ImGuiCol_WindowBg]              = bg;
        c[ImGuiCol_ChildBg]               = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_PopupBg]               = bg;
        c[ImGuiCol_Border]                = border;
        c[ImGuiCol_BorderShadow]          = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_FrameBg]               = bg2;
        c[ImGuiCol_FrameBgHovered]        = bg3;
        c[ImGuiCol_FrameBgActive]         = ImVec4(0.14f, 0.14f, 0.14f, 1.f);
        c[ImGuiCol_TitleBg]               = bg;
        c[ImGuiCol_TitleBgActive]         = bg;
        c[ImGuiCol_TitleBgCollapsed]      = bg;
        c[ImGuiCol_MenuBarBg]             = bg2;
        c[ImGuiCol_ScrollbarBg]           = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_CheckMark]             = text;
        c[ImGuiCol_SliderGrab]            = grab;
        c[ImGuiCol_SliderGrabActive]      = text;
        c[ImGuiCol_Button]                = bg2;
        c[ImGuiCol_ButtonHovered]         = bg3;
        c[ImGuiCol_ButtonActive]          = ImVec4(0.16f, 0.16f, 0.16f, 1.f);
        c[ImGuiCol_Header]                = fill;
        c[ImGuiCol_HeaderHovered]         = fill_h;
        c[ImGuiCol_HeaderActive]          = fill_a;
        c[ImGuiCol_Separator]             = border;
        c[ImGuiCol_SeparatorHovered]      = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_SeparatorActive]       = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ResizeGrip]            = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ResizeGripHovered]     = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ResizeGripActive]      = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_Tab]                   = bg2;
        c[ImGuiCol_TabHovered]            = fill_h;
        c[ImGuiCol_TabSelected]           = fill;
        c[ImGuiCol_TabSelectedOverline]   = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_TabDimmed]             = bg;
        c[ImGuiCol_TabDimmedSelected]     = bg2;
        c[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_PlotLines]             = text_dim;
        c[ImGuiCol_PlotLinesHovered]      = text;
        c[ImGuiCol_PlotHistogram]         = grab;
        c[ImGuiCol_PlotHistogramHovered]  = text;
        c[ImGuiCol_TableHeaderBg]         = bg2;
        c[ImGuiCol_TableBorderStrong]     = border;
        c[ImGuiCol_TableBorderLight]      = ImVec4(0.18f, 0.18f, 0.18f, 1.f);
        c[ImGuiCol_TableRowBg]            = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_TableRowBgAlt]         = ImVec4(1.f, 1.f, 1.f, 0.02f);
        c[ImGuiCol_TextSelectedBg]        = fill_a;
        c[ImGuiCol_DragDropTarget]        = text;
        c[ImGuiCol_NavCursor]             = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_NavWindowingHighlight] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.f, 0.f, 0.f, 0.35f);
        c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.f, 0.f, 0.f, 0.45f);
    }

    // -------------------------------------------------------------------------
    // Teams page - the reference's TEAMS / QUICK ACTIONS / GAME TEAMS layout.
    // We have no team database yet, so every row here is a placeholder that
    // remembers its own state; nothing consumes it.
    // -------------------------------------------------------------------------
    static void draw_teams_page()
    {
        using namespace ng_tabs;

        float left_w = 0.f, right_w = 0.f, h = 0.f;
        begin_columns(&left_w, &right_w, &h);

        begin_column("##tm_l", left_w, h);
        {
            section_header("TEAMS");
            begin_section("##tm_teams");
            {
                row_placeholder("use custom teams");
                row_placeholder("auto detect allied team");
                row_note("no teams in this game");
            }
            end_section();
        }
        end_column();

        ImGui::SameLine(0.f, panel_gap);

        begin_column("##tm_r", right_w, h);
        {
            section_header("QUICK ACTIONS");
            begin_section("##tm_quick");
            {
                row_placeholder("all enemy");
                row_placeholder("all ally");
                row_placeholder("reset to default");
            }
            end_section();

            section_header("GAME TEAMS [0]");
            begin_section("##tm_list");
            {
                row_note("no team list available");
            }
            end_section();
        }
        end_column();
    }

    // the selected page's controls - shared by the LiquidUI card and the plain
    // ImGui fallback chrome. Pages mirror the reference's tab layout.
    static void menu_tab_pages(int page)
    {
        switch (page)
        {
        case page_aimbot:    ng_tabs::draw_aimbot_page();   break;
        case page_silent:    ng_tabs::draw_silent_page();   break;
        case page_trigger:   ng_tabs::draw_trigger_page();  break;
        case page_players:   ng_tabs::draw_esp_tab();       break;
        case page_extras:    ng_tabs::draw_extras_page();   break;
        case page_world:     ng_tabs::draw_world_page();    break;
        case page_character: ng_tabs::draw_local_tab();     break;
        case page_npc:       ng_tabs::draw_npc_page();      break;
        case page_teams:     draw_teams_page();             break;
        case page_options:   ng_tabs::draw_settings_tab(&menu_kb, &menu_kb_skip); break;
        default:             ng_tabs::draw_customize_tab(); break;
        }
    }

    // draws the selected page into a child of the given size, placed at the
    // current cursor. With the LiquidUI kit driving the controls the container
    // is transparent (the glass panels inside ARE the chrome) and the renderer
    // clip keeps every glass primitive inside the page area.
    static void menu_tab_content(int sidebar_selected, float w, float h)
    {
        if (glass::ready())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 6.f));
            ImGui::BeginChild("##tab_content", ImVec2(w, h), ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleVar(2);

            if (Glass::g)
            {
                const ImVec2 cmin = ImGui::GetWindowPos();
                Glass::g->SetClipRect(cmin.x, cmin.y, cmin.x + w, cmin.y + h);
            }

            menu_tab_pages(sidebar_selected);

            if (Glass::g)
                Glass::g->ClearClipRect();

            ImGui::EndChild();
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Border, border_color_inner);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.f);
        ImGui::BeginChild("##tab_content", ImVec2(w, h), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        menu_tab_pages(sidebar_selected);

        ImGui::EndChild();
    }

    static void render_right_panel(int sidebar_selected)
    {
        ImGui::BeginChild("##right_panel", ImVec2(0.f, 0.f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float content_width = avail.x - subtab_margin * 2.f;
        float content_height = avail.y - subtab_margin * 2.f;

        ImGui::SetCursorPos(ImVec2(subtab_margin, subtab_margin));
        menu_tab_content(sidebar_selected, content_width, content_height);

        ImGui::EndChild();
    }

    // launcher pill icon kinds
    enum { NICON_MUSIC = 0, NICON_GUI, NICON_LUA, NICON_PLAYERS, NICON_EXPLORER, NICON_SERVERS, NICON_WATERMARK };

    static void draw_launcher_icon(ImDrawList* dl, int kind, ImVec2 c, ImU32 col)
    {
        const float t = 1.4f;
        switch (kind)
        {
        case NICON_GUI: // window pane
            dl->AddRect(ImVec2(c.x - 8.f, c.y - 6.f), ImVec2(c.x + 8.f, c.y + 7.f), col, 2.5f, 0, t);
            dl->AddLine(ImVec2(c.x - 8.f, c.y - 2.f), ImVec2(c.x + 8.f, c.y - 2.f), col, t);
            break;
        case NICON_LUA: // </>
        {
            ImVec2 l[3] = { ImVec2(c.x - 7.f, c.y - 4.f), ImVec2(c.x - 3.f, c.y), ImVec2(c.x - 7.f, c.y + 4.f) };
            ImVec2 r[3] = { ImVec2(c.x + 7.f, c.y - 4.f), ImVec2(c.x + 3.f, c.y), ImVec2(c.x + 7.f, c.y + 4.f) };
            dl->AddPolyline(l, 3, col, 0, t);
            dl->AddPolyline(r, 3, col, 0, t);
            dl->AddLine(ImVec2(c.x - 1.f, c.y + 4.f), ImVec2(c.x + 1.f, c.y - 4.f), col, t);
            break;
        }
        case NICON_PLAYERS: // two people
        {
            dl->AddCircle(ImVec2(c.x - 2.f, c.y - 3.5f), 2.6f, col, 0, t);
            dl->PathArcTo(ImVec2(c.x - 2.f, c.y + 8.f), 5.5f, IM_PI, 2.f * IM_PI, 12);
            dl->PathStroke(col, 0, t);
            dl->AddCircle(ImVec2(c.x + 4.5f, c.y - 4.5f), 2.1f, col, 0, t);
            dl->PathArcTo(ImVec2(c.x + 4.5f, c.y + 7.f), 4.4f, IM_PI * 0.85f, 2.05f * IM_PI, 10);
            dl->PathStroke(col, 0, t);
            break;
        }
        case NICON_EXPLORER: // folder
        {
            ImVec2 p[6] = {
                ImVec2(c.x - 8.f, c.y + 6.f), ImVec2(c.x - 8.f, c.y - 4.5f), ImVec2(c.x - 3.f, c.y - 4.5f),
                ImVec2(c.x - 1.f, c.y - 2.5f), ImVec2(c.x + 8.f, c.y - 2.5f), ImVec2(c.x + 8.f, c.y + 6.f) };
            dl->AddPolyline(p, 6, col, 0, t);
            break;
        }
        case NICON_MUSIC: // eighth note
            dl->AddCircleFilled(ImVec2(c.x - 2.5f, c.y + 5.f), 2.8f, col);
            dl->AddLine(ImVec2(c.x + 0.3f, c.y + 5.f), ImVec2(c.x + 0.3f, c.y - 7.f), col, t);
            dl->AddBezierCubic(ImVec2(c.x + 0.3f, c.y - 7.f), ImVec2(c.x + 5.5f, c.y - 5.f), ImVec2(c.x + 6.f, c.y - 3.f), ImVec2(c.x + 6.5f, c.y + 0.5f), col, t);
            break;
        case NICON_SERVERS: // globe
        {
            dl->AddCircle(c, 7.5f, col, 0, t);
            dl->AddEllipse(c, ImVec2(3.2f, 7.5f), col, 0.0f, 0, t);
            dl->AddLine(ImVec2(c.x - 7.5f, c.y), ImVec2(c.x + 7.5f, c.y), col, t);
            dl->PathArcTo(ImVec2(c.x, c.y + 12.5f), 11.8f, IM_PI * 1.18f, IM_PI * 1.82f, 12);
            dl->PathStroke(col, 0, t);
            break;
        }
        case NICON_WATERMARK: // badge (rounded card with text lines)
            dl->AddRect(ImVec2(c.x - 7.5f, c.y - 5.5f), ImVec2(c.x + 7.5f, c.y + 5.5f), col, 2.5f, 0, t);
            dl->AddLine(ImVec2(c.x - 4.f, c.y - 1.8f), ImVec2(c.x + 4.f, c.y - 1.8f), col, t);
            dl->AddLine(ImVec2(c.x - 4.f, c.y + 1.8f), ImVec2(c.x + 1.5f, c.y + 1.8f), col, t);
            break;
        }
        }

    // Scales the alpha channel of a hardcoded IM_COL32 color (style.Alpha does
    // not affect direct draw-list calls).
    static ImU32 fade_color(ImU32 c, float a)
    {
        const int alpha = (int)(((c >> IM_COL32_A_SHIFT) & 0xFF) * a);
        return (c & ~IM_COL32_A_MASK) | ((ImU32)alpha << IM_COL32_A_SHIFT);
    }

    // 0..1 open/close animation state, keyed per window id. Exponential ease
    // towards the target so opening and closing both animate smoothly.
    static float window_anim(const char* id, bool open, float speed = 14.f)
    {
        static std::unordered_map<ImGuiID, float> s_anims;
        const float dt = ImGui::GetIO().DeltaTime > 0.f ? ImGui::GetIO().DeltaTime : 1.f / 60.f;
        const float target = open ? 1.f : 0.f;
        float& v = s_anims[ImHashStr(id)];
        v += (target - v) * (1.f - std::exp(-speed * dt));
        if (std::fabs(target - v) < 0.002f)
            v = target;
        return v;
    }

    // Renders one of the tool windows with an open/close fade. The window
    // keeps rendering (fading out) after its open flag goes false, and a
    // close-button click during the fade is still forwarded to `open`.
    static void render_window_faded(const char* id, bool& open, void (*fn)(bool*))
    {
        const float a = window_anim(id, open);
        if (a <= 0.01f)
            return;
        bool flag = open;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a);
        fn(&flag);
        ImGui::PopStyleVar();
        open = flag;
    }

    static void render_navbar(float anim)
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();

        constexpr float cell    = 32.f;   // width per icon
        constexpr float pill_h  = 40.f;   // pill height (icons + active-dot zone)
        constexpr float pad_x   = 10.f;

        const float pill_w = pad_x * 2.f + cell * 7.f;
        // slides down from behind the top edge while fading in
        const ImVec2 pos(vp->Pos.x + (vp->Size.x - pill_w) * 0.5f,
                         vp->Pos.y + 10.f - (1.f - anim) * 24.f);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(pill_w, pill_h), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::Begin("##navbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | 0);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();

        // pill background: tinted glass (follows the gui tint slider) instead
        // of an opaque black pill, with the same white outline as the windows
        {
            float ptr = 0.f, ptg = 0.f, ptb = 0.f, pta = 0.f;
            glass::tint_color(&ptr, &ptg, &ptb, &pta);
            draw->AddRectFilled(wp, ImVec2(wp.x + pill_w, wp.y + pill_h),
                fade_color(IM_COL32((int)(ptr * 255.f), (int)(ptg * 255.f), (int)(ptb * 255.f),
                    (ImU32)(120.f + 100.f * pta)), anim), pill_h * 0.5f);
        }
        draw->AddRect(wp, ImVec2(wp.x + pill_w, wp.y + pill_h), fade_color(IM_COL32(255, 255, 255, 22), anim), pill_h * 0.5f, 0, 1.2f);

        struct Item { const char* id; const char* tip; int kind; };
        const Item items[7] = {
            { "nav_menu",     "menu",     NICON_GUI },
            { "nav_lua",      "lua",      NICON_LUA },
            { "nav_players",  "players",  NICON_PLAYERS },
            { "nav_explorer", "explorer", NICON_EXPLORER },
            { "nav_servers",  "servers",  NICON_SERVERS },
            { "nav_watermark", "watermark", NICON_WATERMARK },
            { "nav_music",    "music",    NICON_MUSIC },
        };

        const float icon_cy = wp.y + (pill_h - 6.f) * 0.5f;

        for (int i = 0; i < 7; ++i)
        {
            const float x0 = wp.x + pad_x + cell * i;
            ImGui::SetCursorScreenPos(ImVec2(x0, wp.y + 2.f));
            ImGui::PushID(items[i].id);
            ImGui::InvisibleButton("##nav_btn", ImVec2(cell, pill_h - 4.f));
            ImGui::PopID();
            const bool hovered = ImGui::IsItemHovered();
            const bool clicked = ImGui::IsItemClicked();
            if (hovered)
                ImGui::SetTooltip(items[i].tip);

            bool active = false;
            switch (i)
            {
            case 0: if (clicked) s_menu_open = !s_menu_open;                    active = s_menu_open; break;
            case 1: if (clicked) Cheat::g_Settings.lua.executor = !Cheat::g_Settings.lua.executor;   active = Cheat::g_Settings.lua.executor; break;
            case 2: if (clicked) Cheat::g_Settings.misc.players = !Cheat::g_Settings.misc.players;   active = Cheat::g_Settings.misc.players; break;
            case 3: if (clicked) Cheat::g_Settings.misc.explorer = !Cheat::g_Settings.misc.explorer; active = Cheat::g_Settings.misc.explorer; break;
            case 4: if (clicked) Cheat::g_Settings.misc.servers = !Cheat::g_Settings.misc.servers; active = Cheat::g_Settings.misc.servers; break;
            case 5: if (clicked) Cheat::g_Settings.gui.watermark = !Cheat::g_Settings.gui.watermark; active = Cheat::g_Settings.gui.watermark; break;
            case 6: if (clicked) { Cheat::g_Settings.misc.music = !Cheat::g_Settings.misc.music; Cheat::Console::Log(Cheat::Console::Color::Gray, "[music] toggled -> %d", (int)Cheat::g_Settings.misc.music); } active = Cheat::g_Settings.misc.music; break;
            }

            if (hovered)
                draw->AddRectFilled(ImVec2(x0 + 2.f, wp.y + 3.f), ImVec2(x0 + cell - 2.f, wp.y + pill_h - 3.f), fade_color(IM_COL32(255, 255, 255, 14), anim), 8.f);

            const ImU32 col = fade_color(ImGui::GetColorU32(active ? ImVec4(1.f, 1.f, 1.f, 1.f) : (hovered ? ImVec4(0.85f, 0.85f, 0.85f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f))), anim);
            draw_launcher_icon(draw, items[i].kind, ImVec2(x0 + cell * 0.5f, icon_cy), col);

            // active dot under the icon (like the reference pill)
            if (active)
                draw->AddCircleFilled(ImVec2(x0 + cell * 0.5f, wp.y + pill_h - 4.5f), 1.8f, col);


        }

        ImGui::End();
    }

    static bool hovered_root_named(const char* a, const char* b = nullptr)
    {
        ImGuiWindow* w = GImGui->HoveredWindow;
        if (!w) return false;
        w = w->RootWindow ? w->RootWindow : w;
        if (std::strcmp(w->Name, a) == 0) return true;
        return b && std::strcmp(w->Name, b) == 0;
    }

    static void render_menu_window(float anim);

    static void handle_menu_key()
    {
        static bool kb_prev = false;

        bool kb_down = menu_kb > 0 && (GetAsyncKeyState(menu_kb) & 0x8000) != 0;

        if (menu_kb_skip)
        {
            if (!kb_down)
                menu_kb_skip = false;
        }
        else if (kb_down && !kb_prev)
        {
            s_menu_open = !s_menu_open;
        }

        kb_prev = kb_down;
    }

    void render()
    {
        handle_menu_key();

        // frosted-glass look comes from the gui settings:
        // frost = milkiness (tint + white wash), blur = gaussian blur strength,
        // tint = configurable glass tint color
        glass::set_frost(Cheat::g_Settings.gui.frost);
        glass::set_blur(Cheat::g_Settings.gui.blur);
        {
            const float* t = Cheat::g_Settings.gui.tint;
            glass::set_tint(t[0], t[1], t[2], t[3]);
        }

        glass::new_frame();   // collect all glass-backed window rects this frame

        lua_open = Cheat::g_Settings.lua.executor;
        players_open = Cheat::g_Settings.misc.players;
        explorer_open = Cheat::g_Settings.misc.explorer;
        servers_open = Cheat::g_Settings.misc.servers;
        esp_preview_open = Cheat::g_Settings.misc.esp_preview;
        music_open = Cheat::g_Settings.misc.music;

        const float menu_a = window_anim("##menu", s_menu_open, 20.f);

        if (menu_a > 0.01f)
            render_navbar(menu_a);

        lua_open = Cheat::g_Settings.lua.executor;
        players_open = Cheat::g_Settings.misc.players;
        explorer_open = Cheat::g_Settings.misc.explorer;
        servers_open = Cheat::g_Settings.misc.servers;
        esp_preview_open = Cheat::g_Settings.misc.esp_preview;
        music_open = Cheat::g_Settings.misc.music;   // re-sync after navbar clicks

        // ESP preview now lives inside the Players page (the reference has it
        // built into the UI rather than as a window floating beside the menu);
        // see ng_tabs::draw_esp_tab - no slide-out panel here any more.

        if (menu_a > 0.01f)
            render_menu_window(menu_a);

        // Ã‘â€¡ÃÂµÃÂºÃÂ±ÃÂ¾ÃÂºÃ‘Â ÃÂ¿Ã‘â‚¬ÃÂµÃÂ²Ã‘Å’Ã‘Å½ ÃÂ¶ÃÂ¸ÃÂ²Ã‘â€˜Ã‘â€š ÃÂ²ÃÂ¾ ÃÂ²ÃÂºÃÂ»ÃÂ°ÃÂ´ÃÂºÃÂµ esp, Ã‘â€šÃÂ¾ ÃÂµÃ‘ÂÃ‘â€šÃ‘Å’ ÃÂ²ÃÂ½Ã‘Æ’Ã‘â€šÃ‘â‚¬ÃÂ¸ ÃÂ¼ÃÂµÃÂ½Ã‘Å½, ÃÂ¿ÃÂ¾Ã‘ÂÃ‘â€šÃÂ¾ÃÂ¼Ã‘Æ’
        // ÃÂµÃÂ³ÃÂ¾ ÃÂ·ÃÂ½ÃÂ°Ã‘â€¡ÃÂµÃÂ½ÃÂ¸ÃÂµ ÃÂ¿ÃÂ¾ÃÂ´Ã‘â€¦ÃÂ²ÃÂ°Ã‘â€šÃ‘â€¹ÃÂ²ÃÂ°ÃÂµÃÂ¼ Ã‘Æ’ÃÂ¶ÃÂµ ÃÂ¿ÃÂ¾Ã‘ÂÃÂ»ÃÂµ ÃÂ¾Ã‘â€šÃ‘â‚¬ÃÂ¸Ã‘ÂÃÂ¾ÃÂ²ÃÂºÃÂ¸
        esp_preview_open = Cheat::g_Settings.misc.esp_preview;

        if (menu_a > 0.01f)
        {
            render_window_faded("##lua_window", lua_open, render_lua_window);
            render_window_faded("##players_window", players_open, render_players_window);
            render_window_faded("##explorer_window", explorer_open, render_explorer_window);
            render_window_faded("##servers_window", servers_open, render_servers_window);
        }
        // music keeps running even when the menu itself is closed
        if (music_open)
        {
            if (!music_media_inited)
            {
                try { media::Init(); }
                catch (...) { Cheat::Console::Log(Cheat::Console::Color::Gray, "[music] media init failed"); }
                // spawn the card in the top right corner (the player clamps
                // these into range)
                ImGuiViewport* mvp = ImGui::GetMainViewport();
                native_music_player::g_playerOptions.x = mvp->WorkSize.x;
                native_music_player::g_playerOptions.y = 14.f;
                music_media_inited = true;
            }
            media::Tick();
            static bool logged_music_frame = false;
            if (!logged_music_frame)
            {
                logged_music_frame = true;
                Cheat::Console::Log(Cheat::Console::Color::Gray, "[music] drawing player (visible=%d)", (int)native_music_player::g_playerOptions.visible);
            }
            native_music_player::DrawMusicPlayer();
        }

        Cheat::g_Settings.lua.executor = lua_open;
        Cheat::g_Settings.misc.players = players_open;
        Cheat::g_Settings.misc.explorer = explorer_open;
        Cheat::g_Settings.misc.servers = servers_open;
        Cheat::g_Settings.misc.esp_preview = esp_preview_open;
        Cheat::g_Settings.misc.music = music_open;

        // watermark badge: like the music player it lives on the overlay and
        // keeps rendering even when the menu itself is closed
        if (Cheat::g_Settings.gui.watermark)
            widgets::watermark(1.f);

    // layuh-style backdrop: dark wash behind everything EXCEPT the frosted-glass
        // windows, whose frosted glass has to stay see-through (the menu is glass).
        // OFF by default (Customize -> "dim background"): the LiquidUI glass
        // panels stand on their own, exactly like the reference app.
        if (menu_a > 0.01f && Cheat::g_Settings.gui.dim_backdrop)
        {
            ImDrawList* bgl = ImGui::GetBackgroundDrawList();
            const ImVec2 bg_dims = ImGui::GetIO().DisplaySize;
            const ImU32 dark = ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.12f, 0.89f * menu_a));

            // collect this frame's glass rects (same coordinate space as the
            // background draw list)
            struct FR { float x0, y0, x1, y1, round; };
            std::vector<FR> holes;
            for (int i = 0; i < glass::rect_count(); ++i)
            {
                float x = 0.f, y = 0.f, w = 0.f, h = 0.f, round = 8.f;
                if (glass::rect_at(i, x, y, w, h, &round) && w > 0.5f && h > 0.5f)
                    holes.push_back(FR{ x, y, x + w, y + h, round });
            }

            if (holes.empty())
            {
                bgl->AddRectFilled(ImVec2(0.f, 0.f), bg_dims, dark, 0);
            }
            else
            {
                // vertical strips between the glass edges; darken only the gaps
                // so the wash never overlaps the windows (no double-blend seams)
                std::vector<float> ys;
                for (const FR& h : holes) { ys.push_back(h.y0); ys.push_back(h.y1); }
                std::sort(ys.begin(), ys.end());
                ys.erase(std::unique(ys.begin(), ys.end()), ys.end());

                auto draw_gap = [&](float y0, float y1)
                {
                    if (y1 - y0 <= 0.01f) return;
                    std::vector<ImVec2> cv; // x-intervals of holes spanning strip [y0,y1]
                    for (const FR& h : holes)
                        if (h.y0 <= y0 + 0.01f && h.y1 >= y1 - 0.01f)
                            cv.push_back(ImVec2(h.x0, h.x1));
                    std::sort(cv.begin(), cv.end(),
                              [](ImVec2 a, ImVec2 b) { return a.x < b.x; });
                    float px = 0.f;
                    for (const ImVec2& c : cv)
                    {
                        if (c.x > px)
                            bgl->AddRectFilled(ImVec2(px, y0), ImVec2(c.x, y1), dark, 0);
                        if (c.y > px) px = c.y;
                    }
                    if (px < bg_dims.x)
                        bgl->AddRectFilled(ImVec2(px, y0), ImVec2(bg_dims.x, y1), dark, 0);
                };

                float yprev = 0.f;
                for (float y : ys) { draw_gap(yprev, y); yprev = y; }
                draw_gap(yprev, bg_dims.y); // final strip down to the bottom edge

                // round the holes: the glass panels are rounded, so their
                // corner cutouts get painted with the backdrop colour to keep
                // every window's shape genuinely rounded.
                for (const FR& h : holes)
                    glass::mask_corners(bgl, ImVec2(h.x0, h.y0), ImVec2(h.x1, h.y1), h.round, dark);
            }

        // Snow particles background - Performance optimized
            {
                struct Snowflake { ImVec2 pos; float speed; float drift; float size; float phase; };
                static std::vector<Snowflake> flakes;
                static ImVec2 last_screen = ImVec2(0, 0);
                static bool seeded = false;
                static auto last_snow_update = std::chrono::steady_clock::now();

                if (!seeded) { srand((unsigned)time(nullptr)); seeded = true; }

                ImVec2 screen = ImGui::GetIO().DisplaySize;
                const int target_count = 70; // Reduced from 140

                if (flakes.empty() || screen.x != last_screen.x || screen.y != last_screen.y) {
                    flakes.clear(); flakes.reserve(target_count);
                    last_screen = screen;
                    for (int i = 0; i < target_count; ++i) {
                        Snowflake f;
                        f.pos = ImVec2((float)(rand() % (int)std::max(1.0f, screen.x)), (float)(rand() % (int)std::max(1.0f, screen.y)));
                        f.speed = 90.0f + (float)(rand() % 100); // px/sec fall speed
                        f.drift = 30.0f + (float)(rand() % 50); // horizontal sway
                        f.size = 1.0f + (float)(rand() % 200) / 100.0f; // 1.0 .. 3.0 px
                        f.phase = (float)(rand() % 628) / 100.0f;
                        flakes.push_back(f);
                    }
                }

                // Performance optimization: Update snow less frequently
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_snow_update);
                if (elapsed.count() < 16) { // ~60 FPS instead of every frame
                    // Skip update but still render
                }
                else {
                    // Use the real wall-clock time since the last update so the
                    // snow speed is exact regardless of the overlay framerate.
                    // (ImGui::GetIO().DeltaTime is nearly zero at uncapped FPS,
                    // which made the flakes crawl.)
                    float dt = std::chrono::duration<float>(now - last_snow_update).count();
                    if (dt > 0.1f) dt = 0.1f; // clamp tab-away jumps
                    for (auto& f : flakes) {
                        f.phase += dt * 2.4f; // faster sway
                        f.pos.y += f.speed * dt;
                        f.pos.x += cosf(f.phase) * f.drift * dt;

                        if (f.pos.y > screen.y + 8.0f) {
                            f.pos.y = -8.0f;
                            f.pos.x = (float)(rand() % (int)std::max(1.0f, screen.x));
                            f.speed = 90.0f + (float)(rand() % 100);
                            f.drift = 30.0f + (float)(rand() % 50);
                            f.size = 1.0f + (float)(rand() % 200) / 100.0f;
                        }
                        if (f.pos.x < -8.0f) f.pos.x = screen.x + 8.0f;
                        if (f.pos.x > screen.x + 8.0f) f.pos.x = -8.0f;
                    }
                    last_snow_update = now;
                }

                for (auto& f : flakes) {
                    bgl->AddCircleFilled(f.pos, f.size, IM_COL32(255, 255, 255, 150)); // Reduced alpha
                }
            }
        }

    glass::commit();   // frame's glass rects are collected; the pass runs in render_pass()
    }

    // -------------------------------------------------------------------------
    // main menu - LiquidUI glass card with a sidebar nav (the old top tab bar
    // window is gone). The page controls are still the tabs from tabs/*.cpp;
    // only the shell and the glass are LiquidUI's.
    // -------------------------------------------------------------------------
    constexpr float menu_default_w = 900.f;
    constexpr float menu_default_h = 720.f;
    constexpr float menu_card_round = 26.f;   // the kit's BeginCard radius
    constexpr float menu_side_w = 176.f;      // sidebar width inside the card

    // places the menu for the given open/close animation value. The position is
    // only forced while the entry animation is actually rising; once it settles
    // the menu can be dragged and resized freely (forcing it every frame would
    // override the custom drag). The last rendered position lives in s_menu_pos.
    static void menu_place(float anim)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        static ImVec2 s_rise_base{};
        static bool   s_rise_valid = false;
        static bool   s_was_open = false;

        const bool opening = s_menu_open && !s_was_open;
        s_was_open = s_menu_open;
        if (opening)
        {
            const bool placed = (s_menu_pos.x != 0.f || s_menu_pos.y != 0.f);
            s_rise_base = placed ? s_menu_pos
                : ImVec2(center.x - menu_default_w * 0.5f, center.y - menu_default_h * 0.5f);
            s_rise_valid = true;
        }
        if (s_rise_valid && anim < 0.999f && s_menu_open)
            ImGui::SetNextWindowPos(ImVec2(s_rise_base.x, s_rise_base.y + (1.f - anim) * 20.f),
                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(menu_default_w, menu_default_h), ImGuiCond_Once);
    }


    // menu drag / resize handles. Has to run while the menu window is still the
    // current ImGui window (both the LiquidUI card and the fallback chrome).
    static void menu_drag_resize(ImVec2 win_pos, ImVec2 win_size)
    {
        constexpr float resize_border = 6.f;
        constexpr float resize_corner = 18.f;
        constexpr float min_size_x = 470.f;    // room for sidebar + two columns
        constexpr float min_size_y = 420.f;
        ImGuiIO& io = ImGui::GetIO();

        enum resize_handle { resize_none = -1, resize_left, resize_right, resize_bottom, resize_bottom_left, resize_bottom_right };
        static int active_handle = resize_none;
        static bool dragging = false;

        auto hit = [](ImVec2 mn, ImVec2 mx, ImVec2 p) { return p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y; };

        ImVec2 l_min(win_pos.x, win_pos.y + resize_corner), l_max(win_pos.x + resize_border, win_pos.y + win_size.y - resize_corner);
        ImVec2 r_min(win_pos.x + win_size.x - resize_border, win_pos.y + resize_corner), r_max(win_pos.x + win_size.x, win_pos.y + win_size.y - resize_corner);
        ImVec2 b_min(win_pos.x + resize_corner, win_pos.y + win_size.y - resize_border), b_max(win_pos.x + win_size.x - resize_corner, win_pos.y + win_size.y);
        ImVec2 bl_min(win_pos.x, win_pos.y + win_size.y - resize_corner), bl_max(win_pos.x + resize_corner, win_pos.y + win_size.y);
        ImVec2 br_min(win_pos.x + win_size.x - resize_corner, win_pos.y + win_size.y - resize_corner), br_max(win_pos.x + win_size.x, win_pos.y + win_size.y);
        ImVec2 menu_min = win_pos, menu_max(win_pos.x + win_size.x, win_pos.y + win_size.y);

        bool on_menu = hovered_root_named("menu");

        int hovered_handle = resize_none;
        if (on_menu || active_handle != resize_none)
        {
            if (hit(bl_min, bl_max, io.MousePos)) hovered_handle = resize_bottom_left;
            else if (hit(br_min, br_max, io.MousePos)) hovered_handle = resize_bottom_right;
            else if (hit(l_min, l_max, io.MousePos)) hovered_handle = resize_left;
            else if (hit(r_min, r_max, io.MousePos)) hovered_handle = resize_right;
            else if (hit(b_min, b_max, io.MousePos)) hovered_handle = resize_bottom;
        }

        bool over_menu = hit(menu_min, menu_max, io.MousePos);
        bool over_empty = on_menu && over_menu && !ImGui::IsAnyItemHovered();

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive() && on_menu)
        {
            if (hovered_handle != resize_none)
            {
                active_handle = hovered_handle;
                dragging = false;
            }
            else if (over_empty)
            {
                dragging = true;
                active_handle = resize_none;
            }
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            active_handle = resize_none;
            dragging = false;
        }

        int shown_handle = (active_handle != resize_none) ? active_handle : (on_menu ? hovered_handle : resize_none);
        if (shown_handle == resize_left || shown_handle == resize_right)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        else if (shown_handle == resize_bottom)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        else if (shown_handle == resize_bottom_left)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
        else if (shown_handle == resize_bottom_right)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);


        if (active_handle != resize_none)
        {
            if (active_handle == resize_left || active_handle == resize_bottom_left)
            {
                float new_w = win_size.x - io.MouseDelta.x;
                if (new_w < min_size_x) new_w = min_size_x;
                win_pos.x += win_size.x - new_w;
                win_size.x = new_w;
            }
            else if (active_handle == resize_right || active_handle == resize_bottom_right)
            {
                win_size.x += io.MouseDelta.x;
                if (win_size.x < min_size_x) win_size.x = min_size_x;
            }

            if (active_handle == resize_bottom || active_handle == resize_bottom_left || active_handle == resize_bottom_right)
            {
                win_size.y += io.MouseDelta.y;
                if (win_size.y < min_size_y) win_size.y = min_size_y;
            }

            ImGui::SetWindowPos(win_pos);
            ImGui::SetWindowSize(win_size);
        }
        else if (dragging)
        {
            win_pos.x += io.MouseDelta.x;
            win_pos.y += io.MouseDelta.y;
            ImGui::SetWindowPos(win_pos);
        }
    }

    // -------------------------------------------------------------------------
    // LiquidUI card for the menu: a frosted panel behind an ImGui window (same
    // look as Glass::BeginCard, but the size stays ours so the menu keeps its
    // own drag/resize handles instead of the card being auto-sized every frame)
    // -------------------------------------------------------------------------
    static size_t g_menu_panel = (size_t)-1;

    static bool begin_menu_card(const char* id)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 18.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.f, 10.f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse   | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar  | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings;
        const bool open = ImGui::Begin(id, nullptr, flags);

        g_menu_panel = (size_t)-1;
        if (Glass::g)
        {
            Glass::Primitive p{};           // filled in by end_menu_card()
            p.corner_radius = menu_card_round;
            p.fade = 1.f;
            p.material = Glass::Material::Regular;
            g_menu_panel = Glass::g->Submit(p);
        }
        return open;
    }

    static void end_menu_card()
    {
        const ImVec2 wp = ImGui::GetWindowPos();
        const ImVec2 ws = ImGui::GetWindowSize();
        if (Glass::g && g_menu_panel != (size_t)-1)
        {
            Glass::Primitive& p = Glass::g->At(g_menu_panel);
            p.cx = wp.x + ws.x * 0.5f;
            p.cy = wp.y + ws.y * 0.5f;
            p.hw = ws.x * 0.5f;
            p.hh = ws.y * 0.5f;
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }


    // -------------------------------------------------------------------------
    // grouped sidebar: section labels (AIMBOT / COMMON), page rows with the
    // kit's icons and the visuals sub-pages indented - the reference layout
    // -------------------------------------------------------------------------
    static void draw_grouped_sidebar(float x, float y, float w)
    {
        const float row_h = 36.f;
        const float sub_row_h = 34.f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float cy = y;

        // a group's sub-pages only render while that group is the active one,
        // so the rail stays short instead of always listing every sub-page
        auto group_open = [&](int i) -> bool
        {
            int parent = i;
            while (parent > 0 && k_nav[parent].sub) --parent;
            if (s_sidebar_selected == k_nav[parent].page) return true;
            for (int j = parent + 1; j < k_nav_count && k_nav[j].sub; ++j)
                if (s_sidebar_selected == k_nav[j].page) return true;
            return false;
        };

        for (int i = 0; i < k_nav_count; ++i)
        {
            const NavRow& n = k_nav[i];
            if (n.sub && !group_open(i))
                continue;
            if (n.group)
            {
                cy += (i == 0) ? 0.f : 10.f;
                const float fs = ImGui::GetFontSize() * 0.76f;
                float gx = x + 12.f;
                for (const char* p = n.group; *p; ++p)
                {
                    char ch[2] = { (char)toupper((unsigned char)*p), 0 };
                    dl->AddText(ImGui::GetFont(), fs, ImVec2(gx, cy + 5.f),
                                IM_COL32(134, 136, 148, 255), ch);
                    gx += ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.f, ch).x + 1.f;
                }
                cy += 24.f;
            }

            const float h = n.sub ? sub_row_h : row_h;
            const float ix = x + (n.sub ? 24.f : 6.f);
            const float iw = w - (n.sub ? 30.f : 6.f);

            ImGui::SetCursorScreenPos(ImVec2(ix, cy));
            ImGui::PushID(i);
            ImGui::InvisibleButton("##nav", ImVec2(iw, h));
            const bool hov = ImGui::IsItemHovered();
            const bool pick = ImGui::IsItemDeactivated() && hov;
            const ImGuiID nid = ImGui::GetID("##nav");
            ImGui::PopID();

            const float dt = ImGui::GetIO().DeltaTime;
            Glass::Spring& hv = Glass::g->springs().Get((uint32_t)nid, 30, Glass::SpringStyle::Critical, 0.f);
            hv.target = hov ? 1.f : 0.f;
            hv.Tick(dt);
            Glass::Spring& sel = Glass::g->springs().Get((uint32_t)nid, 31, Glass::SpringStyle::Critical, 0.f);
            sel.target = (s_sidebar_selected == n.page) ? 1.f : 0.f;
            sel.Tick(dt);

            float plate = hv.x * 0.45f;
            if (sel.x > plate) plate = sel.x;
            if (plate > 1.f) plate = 1.f;

            if (plate > 0.01f)
            {
                Glass::Primitive p{};
                p.cx = ix + iw * 0.5f;
                p.cy = cy + h * 0.5f;
                p.hw = iw * 0.5f;
                p.hh = h * 0.5f - 1.f;
                p.corner_radius = 10.f;
                p.fade = plate;
                p.material = Glass::Material::Thin;
                Glass::g->Submit(p);
            }

            // the active page gets the reference's purple accent bar on the
            // left edge of its plate
            if (sel.x > 0.01f)
            {
                const float* ac = Glass::EditParams(Glass::Material::Accent).tint_rgb;
                const float bar_h = 18.f;
                dl->AddRectFilled(
                    ImVec2(ix + 1.f, cy + (h - bar_h) * 0.5f),
                    ImVec2(ix + 4.f, cy + (h + bar_h) * 0.5f),
                    IM_COL32((int)(ac[0] * 255.f), (int)(ac[1] * 255.f),
                             (int)(ac[2] * 255.f), (int)(255.f * sel.x)), 1.5f);
            }

            const bool on = (s_sidebar_selected == n.page);
            Glass::DrawIcon(dl, n.icon, ImVec2(ix + 20.f, cy + h * 0.5f), 13.f,
                            on ? IM_COL32(238, 239, 245, 255) : IM_COL32(148, 150, 162, 255), 2.f);
            dl->AddText(ImVec2(ix + 40.f, cy + (h - ImGui::GetTextLineHeight()) * 0.5f),
                        on ? IM_COL32(238, 239, 245, 255) : IM_COL32(178, 180, 192, 255),
                        n.label);

            if (pick && n.page != s_sidebar_selected)
                s_sidebar_selected = n.page;

            cy += h + 1.f;
        }
    }

    // profile card pinned to the bottom of the sidebar (the reference has one)
    // - clicking it opens the reference's profile menu
    static void draw_profile_card(float x, float y, float w)
    {
        using namespace ng_tabs;

        const float h = 54.f;

        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::PushID("##profile");
        ImGui::InvisibleButton("##open", ImVec2(w, h));
        const bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemDeactivated() && hov)
            ImGui::OpenPopup("##profile_pop");

        Glass::Primitive p{};
        p.cx = x + w * 0.5f;
        p.cy = y + h * 0.5f;
        p.hw = w * 0.5f;
        p.hh = h * 0.5f;
        p.corner_radius = 14.f;
        p.fade = hov ? 1.f : 0.85f;
        p.material = Glass::Material::Thin;
        Glass::g->Submit(p);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 ac(x + 26.f, y + h * 0.5f);
        dl->AddCircleFilled(ac, 17.f, IM_COL32(240, 214, 88, 255), 32);
        Glass::DrawIcon(dl, Glass::Icon::Person, ac, 15.f, IM_COL32(72, 60, 20, 255), 2.f);

        dl->AddText(ImVec2(x + 52.f, y + 13.f), IM_COL32(236, 237, 243, 255), "ardvark");
        dl->AddText(ImVec2(x + 52.f, y + 29.f), IM_COL32(138, 140, 152, 255), "Lifetime");
        Glass::DrawIcon(dl, Glass::Icon::ChevronR, ImVec2(x + w - 16.f, y + h * 0.5f), 6.f,
                        IM_COL32(150, 152, 164, 220), 2.f);

        // ----------------------------------------------------------------
        // profile menu (the reference's popup): quick access to the menu
        // key, accent, density and the glass switches
        // ----------------------------------------------------------------
        static const std::vector<const char*> k_design = { "compact", "default", "spacious" };

        auto& gui = Cheat::g_Settings.gui;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.055f, 0.068f, 0.985f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.10f));
        // fixed size: it is a solid dark panel (not a glass primitive - glass is
        // drawn in a single pass before all ImGui geometry, so a glass popup
        // plate would let the card's own labels show through it)
        ImGui::SetNextWindowSize(ImVec2(272.f, 8.f * k_row_h + 20.f), ImGuiCond_Always);
        if (ImGui::BeginPopup("##profile_pop"))
        {
            // solid panel: rows highlight with ImGui chrome, not a glass plate
            rows_use_imgui_chrome(true);

            row_keybind_simple("##pm_key", "Menu Key", &menu_kb);
            row_color("Style", gui.accent);
            if (row_button("Upload Profile Picture"))
            {
                // no avatar pipeline yet - the row is honest about it
            }
            row_combo("Design", &gui.density, k_design);
            row_checkbox("Glass", &gui.glass_on);
            bool blur_on = gui.blur > 0.f;
            if (row_checkbox("Blur", &blur_on))
                gui.blur = blur_on ? 90.f : 0.f;
            row_checkbox("Dark Background", &gui.dim_backdrop);
            row_placeholder("Snow Effect");

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
        ImGui::PopID();
        rows_use_imgui_chrome(false);   // unconditional: restores the default for every other row

        ImGui::SetCursorScreenPos(ImVec2(x, y + h));
    }

    // config selector in the page header ("No config" in the reference)
    static void draw_config_selector(float x, float y, float w)
    {
        const float h = 34.f;
        static char  s_current[64] = "default";
        static char  s_items[64][128]{};
        static int   s_count = 0;
        static float s_refresh_at = 0.f;

        const float now = (float)ImGui::GetTime();
        if (now >= s_refresh_at)
        {
            std::vector<std::string> list = Cheat::Config::List();
            s_count = 0;
            for (int i = 0; i < (int)list.size() && s_count < 64; ++i)
            {
                snprintf(s_items[s_count], 128, "%s", list[i].c_str());
                s_count++;
            }
            s_refresh_at = now + 1.f;
        }

        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::PushID("##cfg_sel");
        ImGui::InvisibleButton("##box", ImVec2(w, h));
        const bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemDeactivated() && hov)
            ImGui::OpenPopup("##cfg_pop");
        const ImGuiID id = ImGui::GetID("##box");
        ImGui::PopID();

        Glass::Spring& hv = Glass::g->springs().Get((uint32_t)id, 40, Glass::SpringStyle::Critical, 0.f);
        hv.target = hov ? 1.f : 0.f;
        hv.Tick(ImGui::GetIO().DeltaTime);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                          IM_COL32(255, 255, 255, (int)(16.f + 14.f * hv.x)), 10.f);
        dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                    IM_COL32(255, 255, 255, (int)(24.f + 18.f * hv.x)), 10.f);
        dl->AddText(ImVec2(x + 14.f, y + (h - ImGui::GetTextLineHeight()) * 0.5f),
                    IM_COL32(236, 237, 243, 255), s_current);
        Glass::DrawIcon(dl, Glass::Icon::ChevronD, ImVec2(x + w - 18.f, y + h * 0.5f), 6.f,
                        IM_COL32(160, 162, 174, 255), 2.f);

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f, 0.075f, 0.09f, 0.985f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1.f, 1.f, 1.f, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.f, 1.f, 1.f, 0.16f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
        ImGui::SetNextWindowPos(ImVec2(x, y + h + 4.f));
        if (ImGui::BeginPopup("##cfg_pop"))
        {
            if (s_count == 0)
                ImGui::TextUnformatted("no configs yet");

            for (int i = 0; i < s_count; ++i)
            {
                ImGui::PushID(i);
                if (ImGui::Selectable(s_items[i], strcmp(s_items[i], s_current) == 0, 0,
                                      ImVec2(w, ImGui::GetTextLineHeight() + 8.f)))
                {
                    if (Cheat::Config::Load(s_items[i]))
                        snprintf(s_current, sizeof(s_current), "%s", s_items[i]);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        ImGui::SetCursorScreenPos(ImVec2(x, y + h));
    }

    // the card's interior: brand row, LiquidUI sidebar nav, page title and the
    // selected page's controls
    static void menu_card_body(float anim)
    {
        const ImVec2 win_pos = ImGui::GetWindowPos();
        const ImVec2 win_size = ImGui::GetWindowSize();
        s_menu_pos = win_pos;
        s_menu_size = win_size;

        // the dark backdrop carves a hole for the card; the frosted panel
        // itself was queued by begin_menu_card()
        glass::add_hole(win_pos.x, win_pos.y, win_size.x, win_size.y, menu_card_round);

        ImDrawList* wdl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        // brand row: wordmark (ExtraBold) + accent pill - the reference has no
        // badge here, just the name and a small pill on the same baseline
        {
            const char* name = "ardvark";
            ImFont* wf = fonts::proxima_soft_extrabold;
            const float wsz = 15.f;
            const ImVec2 ts = wf ? wf->CalcTextSizeA(wsz, FLT_MAX, 0.f, name)
                                 : ImGui::CalcTextSize(name);
            const float by = o.y + 27.f;
            if (wf)
                wdl->AddText(wf, wsz, ImVec2(o.x + 24.f, by - ts.y * 0.5f),
                             IM_COL32(240, 241, 246, 255), name);
            else
                wdl->AddText(ImVec2(o.x + 24.f, by - ts.y * 0.5f),
                             IM_COL32(240, 241, 246, 255), name);

            const float* ac = Glass::EditParams(Glass::Material::Accent).tint_rgb;
            const ImVec2 pt = ImGui::CalcTextSize("PRO");
            const float px = o.x + 24.f + ts.x + 10.f;
            const float pw = pt.x + 16.f, ph = 19.f;
            wdl->AddRectFilled(ImVec2(px, by - ph * 0.5f), ImVec2(px + pw, by + ph * 0.5f),
                               IM_COL32((int)(ac[0] * 255.f), (int)(ac[1] * 255.f),
                                        (int)(ac[2] * 255.f), 235), 6.f);
            wdl->AddText(ImVec2(px + 8.f, by - pt.y * 0.5f),
                         IM_COL32(255, 255, 255, 255), "PRO");
        }

        // sidebar: grouped nav (AIMBOT / COMMON + the visuals sub-pages), with
        // the profile card pinned to the bottom of the rail.
        // NOTE: no rail plate here on purpose - the reference's sidebar is the
        // same colour as the page body, only the active row gets a plate.
        draw_grouped_sidebar(o.x, o.y + 54.f, menu_side_w - 8.f);
        if (avail.y > 460.f)
            draw_profile_card(o.x, o.y + avail.y - 58.f, menu_side_w - 8.f);

        // divider between the nav rail and the page
        const float sep_x = o.x + menu_side_w + 8.f;
        wdl->AddLine(ImVec2(sep_x, o.y + 2.f), ImVec2(sep_x, o.y + avail.y - 2.f), IM_COL32(0, 0, 0, 28), 1.f);
        wdl->AddLine(ImVec2(sep_x + 1.f, o.y + 2.f), ImVec2(sep_x + 1.f, o.y + avail.y - 2.f), IM_COL32(255, 255, 255, 40), 1.f);

        // page: config selector in the header, then the page's sections
        const float cx = sep_x + 20.f;
        const float cw = o.x + avail.x - cx - 4.f;
        const float chh = avail.y - 62.f;
        if (cw > 60.f && chh > 60.f)
        {
            draw_config_selector(cx, o.y + 8.f, 210.f);
            ImGui::SetCursorScreenPos(ImVec2(cx, o.y + 58.f));
            menu_tab_content(s_sidebar_selected, cw, chh);
        }

        menu_drag_resize(win_pos, win_size);
        (void)anim;
    }


    // -------------------------------------------------------------------------
    // main menu: LiquidUI glass card + sidebar; if the glass renderer could not
    // be created we fall back to the plain ImGui chrome with the old top tabs
    // so the menu stays usable.
    // -------------------------------------------------------------------------
    static void render_menu_window(float anim)
    {
        menu_place(anim);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim);

        if (glass::ready())
        {
            // fade the whole card (panel, nav, widgets) with the open/close anim
            if (Glass::g) Glass::g->SetSubmitFade(anim);
            Glass::SetInk(IM_COL32(236, 237, 243, 255), IM_COL32(151, 154, 168, 255));
            // profile menu "Design": the kit's density scale (card padding)
            {
                const int dens = Cheat::g_Settings.gui.density;
                Glass::SetDensity(dens <= 0 ? 0.92f : (dens >= 2 ? 1.10f : 1.0f));
            }
            Glass::SetWidgetScale(1.0f);

            if (begin_menu_card("menu"))
                menu_card_body(anim);
            end_menu_card();

            if (Glass::g) Glass::g->SetSubmitFade(1.f);
        }
        else
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_Border, border_color_outer);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.055f, 0.065f, 0.30f));
            ImGui::Begin("menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 0);
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();

            const ImVec2 win_pos = ImGui::GetWindowPos();
            const ImVec2 win_size = ImGui::GetWindowSize();
            s_menu_pos = win_pos;
            s_menu_size = win_size;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::BeginChild("content", win_size, ImGuiChildFlags_Borders);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();

            ImGui::SetCursorPos(ImVec2(content_margin, content_margin));
            ImGui::PushStyleColor(ImGuiCol_Border, border_color_inner);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::BeginChild("content_inner", ImVec2(win_size.x - content_margin * 2.f, win_size.y - content_margin * 2.f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            static const std::vector<const char*> sidebar_items = { "Combat", "Visuals", "Misc", "Local", "Settings" };
            static const widgets::TabIcon sidebar_icons[] = {
                widgets::TABICON_CROSSHAIR, widgets::TABICON_EYE, widgets::TABICON_SLIDERS,
                widgets::TABICON_PERSON,    widgets::TABICON_GEAR
            };
            constexpr float tab_bar_height = 40.f;
            const float inner_w = win_size.x - content_margin * 2.f - inner_padding * 2.f;

            ImGui::SetCursorPos(ImVec2(inner_padding, subtab_margin));
            widgets::top_tabs(sidebar_items, &s_sidebar_selected, inner_w, tab_bar_height, sidebar_icons);

            ImGui::SetCursorPos(ImVec2(inner_padding, subtab_margin + tab_bar_height + subtab_margin));
            render_right_panel(s_sidebar_selected);

            ImGui::EndChild();
            ImGui::EndChild();

            menu_drag_resize(win_pos, win_size);

            ImGui::End();
        }

        ImGui::PopStyleVar();   // alpha
    }
}

