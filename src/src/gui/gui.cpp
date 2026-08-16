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
    }

    static void render_menu_window()
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Once, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(578.f, 504.f), ImGuiCond_Once);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_color_outer);
        ImGui::Begin("menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 0);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();

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

        static int sidebar_selected = 0;
        static const std::vector<const char*> sidebar_items = { "Aim", "Visuals", "Misc", "Local", "Settings" };
        constexpr float sidebar_width = 100.f;

        ImGui::SetCursorPos(ImVec2(inner_padding, subtab_margin));
        widgets::sidebar_tabs(sidebar_items, &sidebar_selected, sidebar_width);

        ImGui::SameLine();
        render_right_panel(sidebar_selected);

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
