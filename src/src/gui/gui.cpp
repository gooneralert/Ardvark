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
#include "tabs/local.h"
#include "tabs/settings_tab.h"
#include "tabs/trigger.h"
#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/console/Console.h"
#include "core/roblox/classes/Classes.h"
#include "features/games/PhantomForces.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "widgets/widgets.h"
#include "widgets/text.h"
#include "glass.h"
#include "music_player_ui.h"
#include "media.h"
#include <cstring>
#include <cmath>
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

    static void render_right_panel(int sidebar_selected)
    {
        ImGui::BeginChild("##right_panel", ImVec2(0.f, 0.f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float content_width = avail.x - subtab_margin * 2.f;
        float content_height = avail.y - subtab_margin * 2.f;

        ImGui::SetCursorPos(ImVec2(subtab_margin, subtab_margin));
        ImGui::PushStyleColor(ImGuiCol_Border, border_color_inner);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.f);
        ImGui::BeginChild("##tab_content", ImVec2(content_width, content_height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        if (sidebar_selected == 0)
            ng_tabs::draw_aim_tab();
        else if (sidebar_selected == 1)
            ng_tabs::draw_trigger_tab();
        else if (sidebar_selected == 2)
            ng_tabs::draw_esp_tab();
        else if (sidebar_selected == 3)
            ng_tabs::draw_misc_tab();
        else if (sidebar_selected == 4)
            ng_tabs::draw_local_tab();
        else
            ng_tabs::draw_settings_tab(&menu_kb, &menu_kb_skip);

        ImGui::EndChild();
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

        // pill background
        draw->AddRectFilled(wp, ImVec2(wp.x + pill_w, wp.y + pill_h), fade_color(IM_COL32(16, 16, 18, 235), anim), pill_h * 0.5f);
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
        // frost = milkiness (tint + white wash), blur = gaussian blur strength
        glass::set_frost(Cheat::g_Settings.gui.frost);
        glass::set_blur(Cheat::g_Settings.gui.blur);

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

        // ESP preview: only while the Visuals tab is active; slides out from
        // underneath the main GUI (rendered before it so the menu covers it)
        {
            const float dt = ImGui::GetIO().DeltaTime > 0.f ? ImGui::GetIO().DeltaTime : 1.f / 60.f;
            const bool esp_wanted = s_menu_open && s_sidebar_selected == 2 && Cheat::g_Settings.misc.esp_preview;
            s_esp_anim += ((esp_wanted ? 1.f : 0.f) - s_esp_anim) * (1.f - std::exp(-14.f * dt));
            if (!esp_wanted && s_esp_anim < 0.001f)
                s_esp_anim = 0.f;
            if (s_esp_anim > 0.01f)
                render_esp_preview_window(&esp_preview_open, s_menu_pos, s_menu_size, s_esp_anim);
        }

        if (menu_a > 0.01f)
            render_menu_window(menu_a);
        else
            glass::set_menu_rect(0, 0, 0, 0);   // hide the acrylic backdrop once the fade-out finishes

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

    glass::commit();   // size/position the acrylic backdrop over every collected rect
    }

    static void render_menu_window(float anim)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        // fade + rise-in. The position is only forced while the entry
        // animation is actually rising; once it settles the menu can be
        // dragged and resized freely (forcing it every frame would override
        // the custom drag). The last rendered position is saved in our own
        // static (ImGui may recreate the window between sessions and forget
        // its position), so the menu always rises from where it was left.
        static bool   s_menuPlaced = false;   // menu has rendered at least once
        static ImVec2 s_riseBase{};
        static bool   s_riseValid = false;
        static bool   s_wasOpen = false;
        const bool opening = s_menu_open && !s_wasOpen;   // just (re)opened
        s_wasOpen = s_menu_open;
        if (opening)
        {
            // entry animation starting: rise from wherever the menu was last
            s_riseBase = s_menuPlaced ? s_menu_pos
                : ImVec2(center.x - 289.f, center.y - 252.f);   // centered (578x504)
            s_riseValid = true;
        }
        if (s_riseValid && anim < 0.999f && s_menu_open)
            ImGui::SetNextWindowPos(ImVec2(s_riseBase.x, s_riseBase.y + (1.f - anim) * 20.f),
                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(578.f, 504.f), ImGuiCond_Once);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_color_outer);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.055f, 0.065f, 0.30f));
        ImGui::Begin("menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 0);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();

        // OS-level acrylic backdrop, sized to exactly the menu rectangle
        // (only the menu gets the blur, not the surrounding ESP overlay)
        glass::add_rect(win_pos.x, win_pos.y, win_size.x, win_size.y, 8.f);

        // frosted-glass backdrop (blurred game behind the menu), inset so the window border stays visible
        glass::draw(
            ImGui::GetWindowDrawList(),
            ImVec2(win_pos.x + 1.f, win_pos.y + 1.f),
            ImVec2(win_pos.x + win_size.x - 1.f, win_pos.y + win_size.y - 1.f),
            7.f);

        s_menu_pos = win_pos;
        s_menu_size = win_size;
        s_menuPlaced = true;

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

        static const std::vector<const char*> sidebar_items = { "Aim", "Triggerbot", "Visuals", "Misc", "Local", "Settings" };
        constexpr float tab_bar_height = 40.f;

        const float inner_w = win_size.x - content_margin * 2.f - inner_padding * 2.f;

        // matcha-style top tab bar
        ImGui::SetCursorPos(ImVec2(inner_padding, subtab_margin));
        widgets::top_tabs(sidebar_items, &s_sidebar_selected, inner_w, tab_bar_height);

        ImGui::SetCursorPos(ImVec2(inner_padding, subtab_margin + tab_bar_height + subtab_margin));
        render_right_panel(s_sidebar_selected);

        ImGui::EndChild();
        ImGui::EndChild();

        constexpr float resize_border = 6.f;
        constexpr float resize_corner = 18.f;
        constexpr float min_size_x = 408.f;
        constexpr float min_size_y = 324.f;
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

        ImGui::End();
        ImGui::PopStyleVar();   // alpha
    }
}
