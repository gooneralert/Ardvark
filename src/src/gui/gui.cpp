#include "pch.h"
#include "gui.h"
#include "lua_window.h"
#include "players_window.h"
#include "explorer_window.h"
#include "esp_preview_window.h"
#include "tabs/aim.h"
#include "tabs/esp.h"
#include "tabs/misc.h"
#include "tabs/local.h"
#include "tabs/settings_tab.h"
#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/roblox/classes/Classes.h"
#include "features/games/PhantomForces.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "widgets/widgets.h"
#include "widgets/text.h"
<<<<<<< Updated upstream
=======
#include "glass.h"
#include "resources/fonts/fonts.h"
#include "core/config/Config.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "features/misc/PlayerAvatars.h"
#include "music_player_ui.h"
#include "media.h"
>>>>>>> Stashed changes
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

namespace gui
{
    constexpr float content_margin = 3.f;
    constexpr float inner_padding = 12.f;
    constexpr float subtab_margin = 6.f;
    constexpr float topbar_width = 250.f;
    constexpr float topbar_height = 26.f;
    constexpr float topbar_gap = content_margin;
    constexpr float navbar_height = 28.f;

    const ImVec4 border_color_outer = ImVec4(0.13f, 0.13f, 0.13f, 1.f);
    const ImVec4 border_color_inner = ImVec4(0.18f, 0.18f, 0.18f, 1.f);

    static bool s_menu_open = true;
    static bool lua_open = false;
    static bool players_open = false;
    static bool explorer_open = false;
    static bool esp_preview_open = false;
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
        static const char* names[] = {
            "##navbar",
            "menu",
            "menu_topbar",
            "##lua_window",
            "##lua_errors",
            "##players_window",
            "##explorer_window",
            "##esp_preview_window",
            "##properties_window",
            "##decompiled_window",
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

        style.WindowRounding    = 0.f;
        style.ChildRounding     = 0.f;
        style.FrameRounding     = 0.f;
        style.PopupRounding     = 0.f;
        style.ScrollbarRounding = 0.f;
        style.GrabRounding      = 0.f;
        style.TabRounding       = 0.f;

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
        ImGui::BeginChild("##tab_content", ImVec2(content_width, content_height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if (sidebar_selected == 0)
            ng_tabs::draw_aim_tab();
        else if (sidebar_selected == 1)
            ng_tabs::draw_esp_tab();
        else if (sidebar_selected == 2)
            ng_tabs::draw_misc_tab();
        else if (sidebar_selected == 3)
            ng_tabs::draw_local_tab();
        else
            ng_tabs::draw_settings_tab(&menu_kb, &menu_kb_skip);

        ImGui::EndChild();
        ImGui::EndChild();
    }

    static void render_navbar()
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(vp->Size.x, navbar_height), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_color_inner);
        ImGui::Begin("##navbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | 0);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        draw->AddLine(ImVec2(wp.x, wp.y + ws.y - 1.f), ImVec2(wp.x + ws.x, wp.y + ws.y - 1.f), ImGui::GetColorU32(border_color_inner));

        static const char* items[] = { "GUI", "Lua", "Players", "Explorer" };
        constexpr float pad_x = 14.f;
        constexpr float item_gap = 18.f;

        float x = wp.x + pad_x;
        float y = wp.y + (ws.y - ImGui::CalcTextSize(items[0]).y) * 0.5f;

        for (int i = 0; i < 4; ++i)
        {
            ImVec2 ts = ImGui::CalcTextSize(items[i]);
            ImVec2 min(x, wp.y);
            ImVec2 max(x + ts.x, wp.y + ws.y);

            ImGui::SetCursorScreenPos(min);
            ImGui::InvisibleButton(items[i], ImVec2(ts.x, ws.y));
            bool hovered = ImGui::IsItemHovered();
            bool clicked = ImGui::IsItemClicked();

            if (i == 0 && clicked)
                s_menu_open = !s_menu_open;
            if (i == 1 && clicked)
                Cheat::g_Settings.lua.executor = !Cheat::g_Settings.lua.executor;
            if (i == 2 && clicked)
                Cheat::g_Settings.misc.players = !Cheat::g_Settings.misc.players;
            if (i == 3 && clicked)
                Cheat::g_Settings.misc.explorer = !Cheat::g_Settings.misc.explorer;

            bool active = (i == 0 && s_menu_open)
                || (i == 1 && Cheat::g_Settings.lua.executor)
                || (i == 2 && Cheat::g_Settings.misc.players)
                || (i == 3 && Cheat::g_Settings.misc.explorer);
            ImU32 col = ImGui::GetColorU32(active ? ImVec4(1.f, 1.f, 1.f, 1.f) : (hovered ? ImVec4(0.85f, 0.85f, 0.85f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f)));
            widgets::text_outlined(draw, ImVec2(x, y), col, items[i]);

            x += ts.x + item_gap;
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

    static void render_menu_window();

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

        lua_open = Cheat::g_Settings.lua.executor;
        players_open = Cheat::g_Settings.misc.players;
        explorer_open = Cheat::g_Settings.misc.explorer;
        esp_preview_open = Cheat::g_Settings.misc.esp_preview;

        if (s_menu_open)
            render_navbar();

        lua_open = Cheat::g_Settings.lua.executor;
        players_open = Cheat::g_Settings.misc.players;
        explorer_open = Cheat::g_Settings.misc.explorer;
<<<<<<< Updated upstream
=======
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
            const bool esp_wanted = s_menu_open && s_sidebar_selected == 1 && Cheat::g_Settings.misc.esp_preview;
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

        // Ãƒâ€˜Ã¢â‚¬Â¡ÃƒÂÃ‚ÂµÃƒÂÃ‚ÂºÃƒÂÃ‚Â±ÃƒÂÃ‚Â¾ÃƒÂÃ‚ÂºÃƒâ€˜Ã‚Â ÃƒÂÃ‚Â¿Ãƒâ€˜Ã¢â€šÂ¬ÃƒÂÃ‚ÂµÃƒÂÃ‚Â²Ãƒâ€˜Ã…â€™Ãƒâ€˜Ã…Â½ ÃƒÂÃ‚Â¶ÃƒÂÃ‚Â¸ÃƒÂÃ‚Â²Ãƒâ€˜Ã¢â‚¬ËœÃƒâ€˜Ã¢â‚¬Å¡ ÃƒÂÃ‚Â²ÃƒÂÃ‚Â¾ ÃƒÂÃ‚Â²ÃƒÂÃ‚ÂºÃƒÂÃ‚Â»ÃƒÂÃ‚Â°ÃƒÂÃ‚Â´ÃƒÂÃ‚ÂºÃƒÂÃ‚Âµ esp, Ãƒâ€˜Ã¢â‚¬Å¡ÃƒÂÃ‚Â¾ ÃƒÂÃ‚ÂµÃƒâ€˜Ã‚ÂÃƒâ€˜Ã¢â‚¬Å¡Ãƒâ€˜Ã…â€™ ÃƒÂÃ‚Â²ÃƒÂÃ‚Â½Ãƒâ€˜Ã†â€™Ãƒâ€˜Ã¢â‚¬Å¡Ãƒâ€˜Ã¢â€šÂ¬ÃƒÂÃ‚Â¸ ÃƒÂÃ‚Â¼ÃƒÂÃ‚ÂµÃƒÂÃ‚Â½Ãƒâ€˜Ã…Â½, ÃƒÂÃ‚Â¿ÃƒÂÃ‚Â¾Ãƒâ€˜Ã‚ÂÃƒâ€˜Ã¢â‚¬Å¡ÃƒÂÃ‚Â¾ÃƒÂÃ‚Â¼Ãƒâ€˜Ã†â€™
        // ÃƒÂÃ‚ÂµÃƒÂÃ‚Â³ÃƒÂÃ‚Â¾ ÃƒÂÃ‚Â·ÃƒÂÃ‚Â½ÃƒÂÃ‚Â°Ãƒâ€˜Ã¢â‚¬Â¡ÃƒÂÃ‚ÂµÃƒÂÃ‚Â½ÃƒÂÃ‚Â¸ÃƒÂÃ‚Âµ ÃƒÂÃ‚Â¿ÃƒÂÃ‚Â¾ÃƒÂÃ‚Â´Ãƒâ€˜Ã¢â‚¬Â¦ÃƒÂÃ‚Â²ÃƒÂÃ‚Â°Ãƒâ€˜Ã¢â‚¬Å¡Ãƒâ€˜Ã¢â‚¬Â¹ÃƒÂÃ‚Â²ÃƒÂÃ‚Â°ÃƒÂÃ‚ÂµÃƒÂÃ‚Â¼ Ãƒâ€˜Ã†â€™ÃƒÂÃ‚Â¶ÃƒÂÃ‚Âµ ÃƒÂÃ‚Â¿ÃƒÂÃ‚Â¾Ãƒâ€˜Ã‚ÂÃƒÂÃ‚Â»ÃƒÂÃ‚Âµ ÃƒÂÃ‚Â¾Ãƒâ€˜Ã¢â‚¬Å¡Ãƒâ€˜Ã¢â€šÂ¬ÃƒÂÃ‚Â¸Ãƒâ€˜Ã‚ÂÃƒÂÃ‚Â¾ÃƒÂÃ‚Â²ÃƒÂÃ‚ÂºÃƒÂÃ‚Â¸
>>>>>>> Stashed changes
        esp_preview_open = Cheat::g_Settings.misc.esp_preview;

        if (s_menu_open)
            render_menu_window();

        // чекбокс превью живёт во вкладке esp, то есть внутри меню, поэтому
        // его значение подхватываем уже после отрисовки
        esp_preview_open = Cheat::g_Settings.misc.esp_preview;

        if (s_menu_open && lua_open)
            render_lua_window(&lua_open);
        if (s_menu_open && players_open)
            render_players_window(&players_open);
        if (s_menu_open && explorer_open)
            render_explorer_window(&explorer_open);
        if (s_menu_open && esp_preview_open)
            render_esp_preview_window(&esp_preview_open, s_menu_pos, s_menu_size);

        Cheat::g_Settings.lua.executor = lua_open;
        Cheat::g_Settings.misc.players = players_open;
        Cheat::g_Settings.misc.explorer = explorer_open;
        Cheat::g_Settings.misc.esp_preview = esp_preview_open;
<<<<<<< Updated upstream
=======
        Cheat::g_Settings.misc.music = music_open;

        // watermark badge: like the music player it lives on the overlay and
        // keeps rendering even when the menu itself is closed
        if (Cheat::g_Settings.gui.watermark)
            widgets::watermark(1.f);

    // layuh-style backdrop: dark wash behind everything EXCEPT the frosted-glass
        // windows, whose acrylic blur has to stay see-through (the menu is glass).
        if (menu_a > 0.01f)
        {
            ImDrawList* bgl = ImGui::GetBackgroundDrawList();
            const ImVec2 bg_dims = ImGui::GetIO().DisplaySize;
            const ImU32 dark = ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.12f, 0.89f * menu_a));

            // collect this frame's glass rects (same coordinate space as the
            // background draw list)
            struct FR { float x0, y0, x1, y1; };
            std::vector<FR> holes;
            for (int i = 0; i < glass::rect_count(); ++i)
            {
                float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
                if (glass::rect_at(i, x, y, w, h) && w > 0.5f && h > 0.5f)
                    holes.push_back(FR{ x, y, x + w, y + h });
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

                // round the holes: the OS acrylic backdrop is square, so its
                // blur peeks through the square hole corners. Paint the corner
                // cutout sectors FULLY OPAQUE (1px outside the hole, slightly
                // larger radius) so no blur sliver survives at the corners and
                // every glass window renders as a genuinely rounded shape.
                const ImU32 opaque = ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.12f, menu_a));
                for (const FR& h : holes)
                    glass::mask_corners(bgl, ImVec2(h.x0 - 1.f, h.y0 - 1.f), ImVec2(h.x1 + 1.f, h.y1 + 1.f), 9.f, opaque);
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

    glass::commit();   // size/position the acrylic backdrop over every collected rect
>>>>>>> Stashed changes
    }

    static void render_menu_window()
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
<<<<<<< Updated upstream
        ImGui::SetNextWindowPos(center, ImGuiCond_Once, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(578.f, 504.f), ImGuiCond_Once);
=======
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
                : ImVec2(center.x - 420.f, center.y - 320.f);   // centered (840x640)
            s_riseValid = true;
        }
        if (s_riseValid && anim < 0.999f && s_menu_open)
            ImGui::SetNextWindowPos(ImVec2(s_riseBase.x, s_riseBase.y + (1.f - anim) * 20.f),
                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(840.f, 640.f), ImGuiCond_Once);
>>>>>>> Stashed changes

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_color_outer);
        ImGui::Begin("menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 0);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();

        s_menu_pos = win_pos;
        s_menu_size = win_size;

        // matcha status pill floating above the menu (fps / uptime / user)
        {
            ImDrawList* bgl = ImGui::GetBackgroundDrawList();
            char fps_buf[24];
            snprintf(fps_buf, sizeof(fps_buf), "%d FPS", (int)(ImGui::GetIO().Framerate + 0.5f));
            const int total = (int)ImGui::GetTime();
            char time_buf[16];
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);

            // local username (refreshed every 2s, shared with the sidebar card)
            static std::string s_user;
            static std::int64_t s_uid = 0;
            static float s_user_next = 0.f;
            const float unow = (float)ImGui::GetTime();
            if (unow >= s_user_next)
            {
                s_user_next = unow + 2.f;
                if (Cheat::Globals::Players && Cheat::Globals::Players->address)
                {
                    const std::uint64_t lp = g_Memory.Read<std::uint64_t>(
                        Cheat::Globals::Players->address + ::Player::LocalPlayer);
                    if (g_Memory.IsValid(lp))
                    {
                        s_user = Cheat::Instance(lp).GetName();
                        s_uid = (std::int64_t)Cheat::Player(lp).GetUserId();
                    }
                }
            }

<<<<<<< Updated upstream
        static int sidebar_selected = 0;
        static const std::vector<const char*> sidebar_items = { "Aim", "Visuals", "Misc", "Local", "Settings" };
        constexpr float sidebar_width = 100.f;

        ImGui::SetCursorPos(ImVec2(inner_padding, subtab_margin));
        widgets::sidebar_tabs(sidebar_items, &sidebar_selected, sidebar_width);

        ImGui::SameLine();
        render_right_panel(sidebar_selected);
=======
            const ImVec2 fps_ts  = ImGui::CalcTextSize(fps_buf);
            const ImVec2 time_ts = ImGui::CalcTextSize(time_buf);
            const ImVec2 user_ts = ImGui::CalcTextSize(s_user.empty() ? "user" : s_user.c_str());

            constexpr float pill_h = 40.f;
            constexpr float pad    = 14.f;
            const float pill_w = pad + 22.f + 12.f + (12.f + 6.f + fps_ts.x) + 18.f +
                                 (12.f + 6.f + time_ts.x) + 18.f + (20.f + 8.f + user_ts.x) + pad;

            float px = win_pos.x + (win_size.x - pill_w) * 0.5f;
            float py = win_pos.y - pill_h - 10.f;
            if (py < 6.f) py = 6.f;

            bgl->AddRectFilled(ImVec2(px, py), ImVec2(px + pill_w, py + pill_h), IM_COL32(26, 26, 30, (int)(245 * anim)), pill_h * 0.5f);
            bgl->AddRect(ImVec2(px, py), ImVec2(px + pill_w, py + pill_h), IM_COL32(255, 255, 255, (int)(18 * anim)), pill_h * 0.5f, 0, 1.2f);
>>>>>>> Stashed changes

            const float cy = py + pill_h * 0.5f;
            float cx = px + pad;

            // logo chip
            bgl->AddRectFilled(ImVec2(cx, cy - 11.f), ImVec2(cx + 22.f, cy + 11.f), IM_COL32(232, 121, 249, (int)(255 * anim)), 6.f);
            {
                const ImVec2 ts = ImGui::CalcTextSize("M");
                bgl->AddText(ImVec2(cx + (22.f - ts.x) * 0.5f, cy - ts.y * 0.5f), IM_COL32(255, 255, 255, (int)(255 * anim)), "M");
            }
            cx += 22.f + 12.f;

            // fps chip
            bgl->AddCircleFilled(ImVec2(cx + 6.f, cy), 5.f, IM_COL32(140, 225, 90, (int)(255 * anim)));
            bgl->AddText(ImVec2(cx + 18.f, cy - fps_ts.y * 0.5f), IM_COL32(240, 240, 242, (int)(255 * anim)), fps_buf);
            cx += 12.f + 6.f + fps_ts.x + 18.f;

            // uptime chip
            bgl->AddCircle(ImVec2(cx + 6.f, cy), 5.f, IM_COL32(232, 121, 249, (int)(255 * anim)), 0, 1.3f);
            bgl->AddText(ImVec2(cx + 18.f, cy - time_ts.y * 0.5f), IM_COL32(240, 240, 242, (int)(255 * anim)), time_buf);
            cx += 12.f + 6.f + time_ts.x + 18.f;

            // user chip
            ID3D11ShaderResourceView* srv = s_uid > 0 ? Cheat::Features::PlayerAvatars::Get(s_uid) : nullptr;
            if (srv)
                bgl->AddImageRounded(ImTextureID((uintptr_t)srv), ImVec2(cx, cy - 10.f), ImVec2(cx + 20.f, cy + 10.f),
                    ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, (int)(255 * anim)), 10.f);
            else
                bgl->AddCircleFilled(ImVec2(cx + 10.f, cy), 10.f, IM_COL32(232, 121, 249, (int)(120 * anim)));
            bgl->AddText(ImVec2(cx + 28.f, cy - user_ts.y * 0.5f), IM_COL32(240, 240, 242, (int)(255 * anim)),
                s_user.empty() ? "user" : s_user.c_str());
        }

        constexpr float side_w   = 190.f;
        constexpr float side_pad = 16.f;
        ImDrawList* wdl = ImGui::GetWindowDrawList();

        // vertical divider between sidebar and content
        wdl->AddLine(ImVec2(win_pos.x + side_w, win_pos.y + 12.f),
                     ImVec2(win_pos.x + side_w, win_pos.y + win_size.y - 12.f),
                     IM_COL32(255, 255, 255, 14), 1.f);

        // ---- sidebar ----
        {
            const float spx = win_pos.x;
            float y = win_pos.y + 18.f;

            // brand: "Matcha" (fredoka) + PRO badge
            {
                ImGui::PushFont(fonts::fredoka_one ? fonts::fredoka_one : ImGui::GetFont());
                const ImVec2 lts = ImGui::CalcTextSize("Matcha");
                wdl->AddText(ImVec2(spx + side_pad, y), IM_COL32(248, 248, 250, 255), "Matcha");
                ImGui::PopFont();

                const ImVec2 b0(spx + side_pad + lts.x + 8.f, y + 2.f);
                const ImVec2 b1(b0.x + 36.f, b0.y + 17.f);
                wdl->AddRectFilled(b0, b1, IM_COL32(232, 121, 249, 255), 5.f);
                const ImVec2 pts = ImGui::CalcTextSize("PRO");
                wdl->AddText(ImVec2(b0.x + (36.f - pts.x) * 0.5f, b0.y + (17.f - pts.y) * 0.5f),
                    IM_COL32(255, 255, 255, 255), "PRO");
                y += 44.f;
            }

            struct NavItem { const char* label; widgets::TabIcon icon; int tab; };
            struct NavGroup { const char* title; NavItem items[4]; int count; };
            static const NavGroup k_groups[] = {
                { "AIMBOT", { { "Aimbot",   widgets::TABICON_CROSSHAIR, 0 } }, 1 },
                { "COMMON", { { "Visuals",  widgets::TABICON_EYE,     1 },
                              { "World",    widgets::TABICON_SLIDERS, 2 },
                              { "Character",widgets::TABICON_PERSON,  3 } }, 3 },
                { "SYSTEM", { { "Options",  widgets::TABICON_GEAR,    4 } }, 1 },
            };

            for (const NavGroup& g : k_groups)
            {
                wdl->AddText(ImVec2(spx + side_pad, y + 3.f), IM_COL32(255, 255, 255, 96), g.title);
                y += 24.f;

                for (int i = 0; i < g.count; ++i)
                {
                    const NavItem& it = g.items[i];
                    const bool active = (s_sidebar_selected == it.tab);

                    const ImVec2 i0(spx + 6.f, y);
                    const ImVec2 i1(spx + side_w - 6.f, y + 34.f);

                    ImGui::SetCursorScreenPos(i0);
                    ImGui::PushID(it.label);
                    ImGui::InvisibleButton("##nav", ImVec2(i1.x - i0.x, 34.f));
                    const bool hov = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked())
                        s_sidebar_selected = it.tab;
                    ImGui::PopID();

                    if (active)
                        wdl->AddRectFilled(i0, i1, IM_COL32(255, 255, 255, 20), 8.f);
                    else if (hov)
                        wdl->AddRectFilled(i0, i1, IM_COL32(255, 255, 255, 10), 8.f);

                    const ImU32 col = ImGui::GetColorU32(active ? ImVec4(0.95f, 0.95f, 0.96f, 1.f)
                                       : (hov ? ImVec4(0.80f, 0.80f, 0.83f, 1.f) : ImVec4(0.62f, 0.62f, 0.66f, 1.f)));
                    widgets::tab_icon(wdl, it.icon, ImVec2(i0.x + 26.f, (i0.y + i1.y) * 0.5f),
                        active ? IM_COL32(232, 121, 249, 255) : col);
                    wdl->AddText(ImVec2(i0.x + 46.f, (i0.y + i1.y - ImGui::GetTextLineHeight()) * 0.5f), col, it.label);

                    y += 38.f;
                }
                y += 8.f;
            }
        }

        // ---- user card (pinned to the sidebar bottom) ----
        {
            constexpr float card_h = 52.f;
            const float cy0 = win_pos.y + win_size.y - card_h - 12.f;
            const ImVec2 c0(win_pos.x + 6.f, cy0);
            const ImVec2 c1(win_pos.x + side_w - 6.f, cy0 + card_h);
            wdl->AddRectFilled(c0, c1, IM_COL32(255, 255, 255, 8), 10.f);

            // local identity (refreshed every 2s)
            static std::string s_user;
            static std::int64_t s_uid = 0;
            static float s_next = 0.f;
            const float now = (float)ImGui::GetTime();
            if (now >= s_next)
            {
                s_next = now + 2.f;
                if (Cheat::Globals::Players && Cheat::Globals::Players->address)
                {
                    const std::uint64_t lp = g_Memory.Read<std::uint64_t>(
                        Cheat::Globals::Players->address + ::Player::LocalPlayer);
                    if (g_Memory.IsValid(lp))
                    {
                        s_user = Cheat::Instance(lp).GetName();
                        s_uid = (std::int64_t)Cheat::Player(lp).GetUserId();
                    }
                }
            }

            const float av = 34.f;
            const ImVec2 a0(c0.x + 9.f, c0.y + (card_h - av) * 0.5f);
            const ImVec2 a1(a0.x + av, a0.y + av);
            ID3D11ShaderResourceView* srv = s_uid > 0 ? Cheat::Features::PlayerAvatars::Get(s_uid) : nullptr;
            if (srv)
                wdl->AddImageRounded(ImTextureID((uintptr_t)srv), a0, a1,
                    ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), av * 0.5f);
            else
            {
                wdl->AddCircleFilled(ImVec2((a0.x + a1.x) * 0.5f, (a0.y + a1.y) * 0.5f), av * 0.5f, IM_COL32(232, 121, 249, 70));
                const char* init = s_user.empty() ? "?" : s_user.substr(0, 1).c_str();
                const ImVec2 ts = ImGui::CalcTextSize(init);
                wdl->AddText(ImVec2((a0.x + a1.x - ts.x) * 0.5f, (a0.y + a1.y - ts.y) * 0.5f), IM_COL32(255, 255, 255, 220), init);
            }

            wdl->AddText(ImVec2(a1.x + 10.f, c0.y + 9.f), IM_COL32(242, 242, 244, 255),
                s_user.empty() ? "user" : s_user.c_str());
            wdl->AddText(ImVec2(a1.x + 10.f, c0.y + 27.f), IM_COL32(255, 255, 255, 100), "lifetime");

            const float chx = c1.x - 14.f;
            const float chy = c0.y + card_h * 0.5f;
            const ImVec2 ch[3] = { ImVec2(chx - 2.f, chy - 4.f), ImVec2(chx + 2.f, chy), ImVec2(chx - 2.f, chy + 4.f) };
            wdl->AddPolyline(ch, 3, IM_COL32(255, 255, 255, 110), 0, 1.4f);
        }

        // ---- right content column ----
        ImGui::SetCursorScreenPos(ImVec2(win_pos.x + side_w + 16.f, win_pos.y + 14.f));
        ImGui::BeginChild("##matcha_content", ImVec2(win_size.x - side_w - 32.f, win_size.y - 28.f),
            ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        {
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            const float cw = ImGui::GetWindowSize().x;

            // config dropdown (loads the selected config on change)
            static std::vector<std::string> s_cfgs;
            static std::vector<const char*> s_cfg_items;
            static int s_cfg_sel = 0;
            static float s_cfg_next = 0.f;
            const float cnow = (float)ImGui::GetTime();
            if (cnow >= s_cfg_next)
            {
                s_cfg_next = cnow + 2.f;
                s_cfgs = Cheat::Config::List();
                s_cfg_items.clear();
                for (const auto& c : s_cfgs)
                    s_cfg_items.push_back(c.c_str());
                if (s_cfg_items.empty())
                    s_cfg_items.push_back("No config");
                if (s_cfg_sel >= (int)s_cfg_items.size())
                    s_cfg_sel = 0;
            }

            ImGui::PushItemWidth(210.f);
            const int prev_sel = s_cfg_sel;
            widgets::combo("##matcha_config", &s_cfg_sel, s_cfg_items, 30.f);
            ImGui::PopItemWidth();
            if (s_cfg_sel != prev_sel && s_cfg_sel >= 0 && s_cfg_sel < (int)s_cfgs.size())
                Cheat::Config::Load(s_cfgs[s_cfg_sel]);

            // hairline under the dropdown row
            const float hy = ImGui::GetCursorScreenPos().y + 8.f;
            cdl->AddLine(ImVec2(ImGui::GetWindowPos().x, hy), ImVec2(ImGui::GetWindowPos().x + cw, hy), IM_COL32(255, 255, 255, 14), 1.f);
            ImGui::Dummy(ImVec2(0.f, 22.f));

            render_right_panel(s_sidebar_selected);
        }
        ImGui::EndChild();

        constexpr float resize_border = 6.f;
        constexpr float resize_corner = 18.f;
        constexpr float min_size_x = 660.f;
        constexpr float min_size_y = 500.f;
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
        float topbar_x = win_pos.x + (win_size.x - topbar_width) * 0.5f;
        ImVec2 topbar_min(topbar_x, win_pos.y - topbar_gap - topbar_height), topbar_max(topbar_x + topbar_width, win_pos.y - topbar_gap);

        bool on_menu = hovered_root_named("menu", "menu_topbar");

        int hovered_handle = resize_none;
        if (on_menu || active_handle != resize_none)
        {
            if (hit(bl_min, bl_max, io.MousePos)) hovered_handle = resize_bottom_left;
            else if (hit(br_min, br_max, io.MousePos)) hovered_handle = resize_bottom_right;
            else if (hit(l_min, l_max, io.MousePos)) hovered_handle = resize_left;
            else if (hit(r_min, r_max, io.MousePos)) hovered_handle = resize_right;
            else if (hit(b_min, b_max, io.MousePos)) hovered_handle = resize_bottom;
        }

        bool over_menu = hit(menu_min, menu_max, io.MousePos) || hit(topbar_min, topbar_max, io.MousePos);
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

        topbar_x = win_pos.x + (win_size.x - topbar_width) * 0.5f;
        ImGui::SetNextWindowPos(ImVec2(topbar_x, win_pos.y - topbar_gap - topbar_height), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(topbar_width, topbar_height), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_color_outer);
        ImGui::Begin("menu_topbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | 0);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImVec2 topbar_size = ImGui::GetWindowSize();

        ImGui::SetCursorPos(ImVec2(content_margin, content_margin));
        ImGui::PushStyleColor(ImGuiCol_Border, border_color_inner);
        ImGui::BeginChild("topbar_inner", ImVec2(topbar_size.x - content_margin * 2.f, topbar_size.y - content_margin * 2.f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        std::string topbar_str = "Ardvark";
        if (Cheat::Globals::InstanceDataModel.address)
        {
            auto pid = Cheat::Globals::InstanceDataModel.GetPlaceId();
            std::string title;
            if (Cheat::Games::PhantomForces::IsActivePlace())
                title = "Phantom Forces";
            else if (pid == 863266079ull)
                title = "Apocalypse Rising 2";
            else if (pid == 16530963934ull)
                title = "Havoc";
            else if (pid == 301549746ull)
                title = "Counter Blox";
            else if (pid == 2788229376ull)
                title = "Da Hood";
            else if (pid == 2753915549ull)
                title = "Blox Fruits";
            else if (pid == 155615604ull)
                title = "Prison Life";
            else if (pid == 142823291ull)
                title = "Murder Mystery 2";
            else
            {
                std::string pname = Cheat::Globals::InstanceDataModel.GetName();
                if (!pname.empty() && pname != "Unknown" && pname != "DataModel" && pname != "UGC" && pname != "Workspace" && pname != "Game" && pname != "game")
                    title = pname;
                else if (pid > 0)
                    title = std::to_string(pid);
            }

            if (!title.empty())
                topbar_str += " | " + title;
        }
        const char* topbar_text = topbar_str.c_str();
        ImVec2 text_size = ImGui::CalcTextSize(topbar_text);
        ImVec2 inner_size = ImGui::GetWindowSize();
        ImVec2 text_pos = ImGui::GetWindowPos();
        text_pos.x += (inner_size.x - text_size.x) * 0.5f;
        text_pos.y += (inner_size.y - text_size.y) * 0.5f;
        widgets::text_outlined(ImGui::GetWindowDrawList(), text_pos, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)), topbar_text);

        ImGui::EndChild();
        ImGui::End();
    }
}
