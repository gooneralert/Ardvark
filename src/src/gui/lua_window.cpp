#include "pch.h"
#include "lua_window.h"
#include "editor/lua_editor.h"
#include "widgets/text.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "features/lua/LuaExecutor.h"
#include "features/lua/vm/LuaVM.h"
#include "features/lua/State.h"
#include "features/lua/protocol/Log.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace gui
{
    static const ImVec4 border_outer = ImVec4(0.13f, 0.13f, 0.13f, 1.f);
    static const ImVec4 border_inner = ImVec4(0.18f, 0.18f, 0.18f, 1.f);

    struct lua_tab
    {
        std::string name;
        editor::lua_editor ed;
        bool dirty = false;
    };

    static editor::lua_editor err_ed;
    static bool err_has_errors = false;
    static bool err_inited = false;

    static void lua_set_output_ok()
    {
        err_has_errors = false;
        err_ed.readonly = true;
        err_ed.set_text("empty - press run");
        err_ed.readonly = true;
    }

    static void sync_output_from_log()
    {
        using namespace Cheat::Features::LuaDetail;
        std::lock_guard<std::mutex> lock(g_output_mu);
        if (g_output.empty())
        {
            if (err_has_errors || err_ed.get_text() != "empty - press run")
                lua_set_output_ok();
            return;
        }

        std::string text;
        bool any_err = false;
        for (size_t i = 0; i < g_output.size(); ++i)
        {
            if (i) text.push_back('\n');
            text += g_output[i].text;
            if (g_output[i].level == LogLevel::Error || g_output[i].level == LogLevel::Warn)
                any_err = true;
        }
        err_has_errors = any_err;
        err_ed.readonly = true;
        if (err_ed.get_text() != text)
            err_ed.set_text(text);
        err_ed.readonly = true;
    }

    void lua_push_error(const char* line)
    {
        if (!line) return;
        Cheat::Features::LuaExecutor::Log(Cheat::Features::LuaExecutor::LogLevel::Error, "%s", line);
        sync_output_from_log();
    }

    static void render_lua_output_panel(ImVec2 lua_pos, ImVec2 lua_size)
    {
        if (!err_inited)
        {
            lua_set_output_ok();
            err_inited = true;
        }

        constexpr float panel_h = 120.f;
        constexpr float gap = 4.f;
        constexpr float margin = 3.f;

        ImGui::SetNextWindowPos(ImVec2(lua_pos.x, lua_pos.y + lua_size.y + gap), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(lua_size.x, panel_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_outer);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.f));
        ImGui::Begin("##lua_errors", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        sync_output_from_log();

        ImU32 title_col = err_has_errors ? IM_COL32(220, 90, 90, 255) : IM_COL32(110, 200, 120, 255);
        widgets::text_outlined(draw, ImVec2(wp.x + 8.f, wp.y + 4.f), title_col, "output");

        ImGui::SetCursorPos(ImVec2(margin, 18.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_inner);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.06f, 1.f));
        float body_h = ws.y - 18.f - margin;
        float body_w = ws.x - margin * 2.f;
        err_ed.readonly = true;
        err_ed.render("##lua_err_ed", ImVec2(body_w, body_h));
        ImGui::PopStyleColor(2);

        ImGui::End();

        ImGuiWindow* out = ImGui::FindWindowByName("##lua_errors");
        ImGuiWindow* lua = ImGui::FindWindowByName("##lua_window");
        if (out && lua)
        {
            ImGuiContext& g = *GImGui;
            auto root = [](ImGuiWindow* w) -> ImGuiWindow* { return w ? w->RootWindow : nullptr; };
            ImGuiWindow* nav = root(g.NavWindow);
            ImGuiWindow* mov = root(g.MovingWindow);
            if (nav == lua || nav == out || mov == lua || mov == out)
            {
                ImGui::BringWindowToDisplayFront(lua);
                ImGui::BringWindowToDisplayFront(out);
            }
        }
    }

    static std::string exe_dir()
    {
        char buf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        fs::path p(buf);
        return p.parent_path().string();
    }

    static std::string scripts_dir()
    {
        return (fs::path(exe_dir()) / "scripts").string();
    }

    static void ensure_scripts()
    {
        fs::path dir = scripts_dir();
        std::error_code ec;
        if (!fs::exists(dir, ec))
            fs::create_directories(dir, ec);

        auto seed = [&](const char* name, const char* body) {
            fs::path f = dir / name;
            if (!fs::exists(f, ec))
            {
                std::ofstream out(f.string(), std::ios::binary);
                if (out) out << body;
            }
        };

        seed("Main.lua",
            "local function greet(name)\n"
            "    print(\"hello, \" .. name)\n"
            "end\n"
            "\n"
            "greet(\"world\")\n");
        seed("draw.lua",
            "local size = draw.text_size(\"test\")\n"
            "draw.text(Vector2.new(10, 10), \"test\")\n");
        seed("atmos.lua", "-- atmos stub\n");
    }

    static std::vector<std::string> list_scripts()
    {
        ensure_scripts();
        std::vector<std::string> out;
        std::error_code ec;
        for (auto& e : fs::directory_iterator(scripts_dir(), ec))
        {
            if (!e.is_regular_file(ec)) continue;
            if (e.path().extension() == ".lua")
                out.push_back(e.path().stem().string());
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    static bool load_script(editor::lua_editor& ed, const std::string& stem)
    {
        fs::path f = fs::path(scripts_dir()) / (stem + ".lua");
        std::ifstream in(f.string(), std::ios::binary);
        if (!in) return false;
        std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ed.set_text(body);
        return true;
    }

    static bool save_script(const editor::lua_editor& ed, const std::string& stem)
    {
        ensure_scripts();
        fs::path f = fs::path(scripts_dir()) / (stem + ".lua");
        std::ofstream out(f.string(), std::ios::binary);
        if (!out) return false;
        out << ed.get_text();
        return true;
    }

    static std::string unique_untitled(const std::vector<std::string>& scripts, const std::vector<lua_tab>& tabs)
    {
        auto taken = [&](const std::string& n) {
            for (auto& s : scripts) if (s == n) return true;
            for (auto& t : tabs) if (t.name == n) return true;
            return false;
        };
        if (!taken("untitled"))
            return "untitled";
        for (int i = 2; ; ++i)
        {
            std::string n = "untitled" + std::to_string(i);
            if (!taken(n))
                return n;
        }
    }

    static int find_tab(std::vector<lua_tab>& tabs, const std::string& name)
    {
        for (int i = 0; i < (int)tabs.size(); ++i)
            if (tabs[i].name == name)
                return i;
        return -1;
    }

    static void open_tab(std::vector<lua_tab>& tabs, int& active, const std::string& name)
    {
        int idx = find_tab(tabs, name);
        if (idx < 0)
        {
            lua_tab t;
            t.name = name;
            load_script(t.ed, name);
            tabs.push_back(std::move(t));
            idx = (int)tabs.size() - 1;
        }
        active = idx;
    }

    static void close_tab(std::vector<lua_tab>& tabs, int& active, int idx)
    {
        if (idx < 0 || idx >= (int)tabs.size())
            return;
        tabs.erase(tabs.begin() + idx);
        if (tabs.empty())
            active = -1;
        else if (active > idx)
            active--;
        else if (active == idx)
            active = std::min(idx, (int)tabs.size() - 1);
    }

    static bool action_button(const char* label, ImVec2 min, ImVec2 max)
    {
        ImGui::PushID(label);
        ImGui::SetCursorScreenPos(min);
        ImGui::InvisibleButton("##act", ImVec2(max.x - min.x, max.y - min.y));
        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked();

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.f)));
        if (held)
            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.10f)));
        else if (hovered)
            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
        draw->AddRect(min, max, ImGui::GetColorU32(border_inner));

        ImVec2 ts = ImGui::CalcTextSize(label);
        widgets::text_outlined(draw, ImVec2(min.x + (max.x - min.x - ts.x) * 0.5f, min.y + (max.y - min.y - ts.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.f)), label);

        ImGui::PopID();
        return clicked;
    }

    void render_lua_window(bool* open)
    {
        if (!open || !*open)
            return;

        static std::vector<lua_tab> tabs;
        static int active_tab = -1;
        static bool inited = false;
        static std::vector<std::string> scripts;
        static float refresh_t = 0.f;
        static int renaming_tab = -1;
        static char rename_buf[64] = {};
        static bool rename_focus_pending = false;
        static float tab_scroll = 0.f;

        if (!inited)
        {
            ensure_scripts();
            scripts = list_scripts();
            open_tab(tabs, active_tab, "Main");
            inited = true;
        }

        refresh_t += ImGui::GetIO().DeltaTime;
        if (refresh_t > 1.5f)
        {
            refresh_t = 0.f;
            scripts = list_scripts();
        }

        constexpr float title_h = 24.f;
        constexpr float btn_h = 28.f;
        constexpr float margin = 3.f;
        constexpr float gap = 6.f;
        constexpr float tab_h = 24.f;

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(ImVec2(center.x + 40.f, center.y - 20.f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(560.f, 360.f), ImGuiCond_Once);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_outer);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.f));
        bool visible = ImGui::Begin("##lua_window", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | 0);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        if (!visible)
        {
            ImGui::End();
            return;
        }

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + title_h), IM_COL32(20, 20, 20, 255));
        widgets::text_outlined(draw, ImVec2(wp.x + 10.f, wp.y + (title_h - ImGui::CalcTextSize("Lua").y) * 0.5f),
            IM_COL32(230, 230, 230, 255), "Lua");

        ImVec2 xsz = ImGui::CalcTextSize("X");
        ImVec2 xmin(wp.x + ws.x - xsz.x - 14.f, wp.y);
        ImGui::SetCursorScreenPos(xmin);
        ImGui::InvisibleButton("##lua_close", ImVec2(xsz.x + 14.f, title_h));
        bool xhov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            *open = false;
        widgets::text_outlined(draw, ImVec2(xmin.x + 7.f, wp.y + (title_h - xsz.y) * 0.5f),
            xhov ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 160, 255), "X");

        float body_top = title_h + margin;
        float body_h = ws.y - body_top - margin - btn_h - margin;
        float body_w = ws.x - margin * 2.f;

        ImGui::SetCursorPos(ImVec2(margin, body_top));
        ImGui::PushStyleColor(ImGuiCol_Border, border_inner);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##lua_body", ImVec2(body_w, body_h), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();

        float list_w = body_w * 0.22f;
        if (list_w < 130.f) list_w = 130.f;
        float ed_w = body_w - list_w - gap - margin * 2.f;
        float inner_h = body_h - margin * 2.f;

        ImGui::SetCursorPos(ImVec2(margin, margin));
        ImGui::BeginChild("##lua_left", ImVec2(ed_w, inner_h), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

        int close_request = -1;
        int switch_request = -1;
        int new_tab_request = 0;

        auto commit_rename = [&]() {
            if (renaming_tab < 0 || renaming_tab >= (int)tabs.size())
            {
                renaming_tab = -1;
                return;
            }
            std::string new_name = rename_buf;
            while (!new_name.empty() && (new_name.back() == ' '))
                new_name.pop_back();

            bool collides = false;
            for (int i = 0; i < (int)tabs.size(); ++i)
                if (i != renaming_tab && tabs[i].name == new_name)
                    collides = true;

            if (!new_name.empty() && !collides && new_name != tabs[renaming_tab].name)
            {
                fs::path oldf = fs::path(scripts_dir()) / (tabs[renaming_tab].name + ".lua");
                fs::path newf = fs::path(scripts_dir()) / (new_name + ".lua");
                std::error_code ec;
                if (!fs::exists(newf, ec))
                {
                    // старый файл на диске есть только если вкладку уже сохраняли
                    if (fs::exists(oldf, ec))
                        fs::rename(oldf, newf, ec);
                    tabs[renaming_tab].name = new_name;
                    scripts = list_scripts();
                }
            }
            renaming_tab = -1;
        };

        {
            ImVec2 bar_pos = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImGuiIO& io = ImGui::GetIO();
            constexpr float tab_pad_x = 10.f;
            constexpr float close_w = 14.f;
            constexpr float plus_w = 26.f;
            float tabs_view_w = ed_w - plus_w - 2.f;

            float content_w = 0.f;
            for (int i = 0; i < (int)tabs.size(); ++i)
            {
                ImVec2 ts = ImGui::CalcTextSize(tabs[i].name.c_str());
                float name_w = std::max(ts.x, (i == renaming_tab) ? 60.f : 0.f);
                content_w += tab_pad_x + name_w + 4.f + close_w + tab_pad_x * 0.5f;
            }

            float max_scroll = std::max(0.f, content_w - tabs_view_w);
            bool over_bar = io.MousePos.x >= bar_pos.x && io.MousePos.x < bar_pos.x + tabs_view_w &&
                io.MousePos.y >= bar_pos.y && io.MousePos.y < bar_pos.y + tab_h;
            if (over_bar && max_scroll > 0.f)
                tab_scroll -= io.MouseWheel * 40.f;
            tab_scroll = std::clamp(tab_scroll, 0.f, max_scroll);

            dl->AddLine(ImVec2(bar_pos.x, bar_pos.y + tab_h - 1.f), ImVec2(bar_pos.x + ed_w, bar_pos.y + tab_h - 1.f), ImGui::GetColorU32(border_inner));
            dl->PushClipRect(bar_pos, ImVec2(bar_pos.x + tabs_view_w, bar_pos.y + tab_h), true);

            float x = bar_pos.x - tab_scroll;
            for (int i = 0; i < (int)tabs.size(); ++i)
            {
                bool active = (i == active_tab);
                bool renaming = (i == renaming_tab);
                ImVec2 ts = ImGui::CalcTextSize(tabs[i].name.c_str());
                float name_w = std::max(ts.x, renaming ? 60.f : 0.f);
                float label_w = tab_pad_x + name_w + 4.f;
                float tw = label_w + close_w + tab_pad_x * 0.5f;

                ImVec2 tmin(x, bar_pos.y);
                ImVec2 tmax(x + tw, bar_pos.y + tab_h);
                bool visible = tmax.x >= bar_pos.x && tmin.x <= bar_pos.x + tabs_view_w;

                bool hovered = false;
                bool close_hov = false;
                if (!renaming && visible)
                {
                    ImGui::PushID(i);
                    ImGui::SetCursorScreenPos(tmin);
                    ImGui::InvisibleButton("##tab", ImVec2(label_w, tab_h));
                    hovered = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked())
                        switch_request = i;
                    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        renaming_tab = i;
                        strncpy_s(rename_buf, tabs[i].name.c_str(), sizeof(rename_buf) - 1);
                        rename_focus_pending = true;
                    }

                    ImGui::SetCursorScreenPos(ImVec2(tmin.x + label_w, tmin.y));
                    ImGui::InvisibleButton("##tab_x", ImVec2(close_w + tab_pad_x * 0.5f, tab_h));
                    close_hov = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked())
                        close_request = i;
                    ImGui::PopID();
                    hovered = hovered || close_hov;
                }

                if (visible)
                {
                    if (active || renaming)
                        dl->AddRectFilled(tmin, ImVec2(tmax.x, tmax.y - 1.f), ImGui::GetColorU32(ImVec4(0.14f, 0.14f, 0.14f, 1.f)));
                    else if (hovered)
                        dl->AddRectFilled(tmin, ImVec2(tmax.x, tmax.y - 1.f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)));

                    if (i > 0)
                        dl->AddLine(ImVec2(tmin.x, tmin.y + 4.f), ImVec2(tmin.x, tmax.y - 5.f), ImGui::GetColorU32(border_inner));

                    if (active || renaming)
                        dl->AddLine(ImVec2(tmin.x, tmin.y), ImVec2(tmax.x, tmin.y), ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 0.6f)));
                }

                if (renaming && visible)
                {
                    ImGui::PushID(i);
                    ImGui::SetCursorScreenPos(ImVec2(tmin.x + tab_pad_x - 4.f, tmin.y + (tab_h - ImGui::GetTextLineHeight()) * 0.5f - 2.f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.08f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.10f, 0.10f, 0.10f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.12f, 0.12f, 0.12f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(1.f, 1.f, 1.f, 0.18f));
                    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0.f, 0.f, 0.f, 0.f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 2.f));
                    ImGui::SetNextItemWidth(name_w + 8.f);
                    if (rename_focus_pending)
                    {
                        ImGui::SetKeyboardFocusHere();
                        rename_focus_pending = false;
                    }
                    bool enter = ImGui::InputText("##rename", rename_buf, sizeof(rename_buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                    bool deactivated = ImGui::IsItemDeactivated();
                    ImGui::PopStyleVar(3);
                    ImGui::PopStyleColor(7);
                    ImGui::PopID();
                    if (enter || deactivated)
                        commit_rename();
                }
                else if (visible)
                {
                    ImU32 text_col = ImGui::GetColorU32(active ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.6f, 0.6f, 0.6f, 1.f));
                    widgets::text_outlined(dl, ImVec2(tmin.x + tab_pad_x, tmin.y + (tab_h - ts.y) * 0.5f), text_col, tabs[i].name.c_str());

                    ImVec2 cx_pos(tmin.x + label_w, tmin.y + (tab_h - ts.y) * 0.5f);
                    ImU32 cx_col = ImGui::GetColorU32(close_hov ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.45f, 0.45f, 0.45f, 1.f));
                    widgets::text_outlined(dl, cx_pos, cx_col, "x");
                }

                x += tw;
            }

            dl->PopClipRect();

            ImVec2 pmin(bar_pos.x + tabs_view_w + 2.f, bar_pos.y);
            ImVec2 pmax(pmin.x + plus_w, bar_pos.y + tab_h);
            ImGui::SetCursorScreenPos(pmin);
            ImGui::InvisibleButton("##new_tab", ImVec2(plus_w, tab_h));
            bool plus_hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked())
                new_tab_request = 1;
            if (plus_hovered)
                dl->AddRectFilled(pmin, ImVec2(pmax.x, pmax.y - 1.f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
            ImVec2 plus_ts = ImGui::CalcTextSize("+");
            ImU32 plus_col = ImGui::GetColorU32(plus_hovered ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f));
            widgets::text_outlined(dl, ImVec2(pmin.x + (plus_w - plus_ts.x) * 0.5f, pmin.y + (tab_h - plus_ts.y) * 0.5f), plus_col, "+");

            ImGui::SetCursorScreenPos(ImVec2(bar_pos.x, bar_pos.y + tab_h));
        }

        if (close_request >= 0)
        {
            bool was_renaming_this = (close_request == renaming_tab);
            close_tab(tabs, active_tab, close_request);
            if (was_renaming_this)
                renaming_tab = -1;
            else if (renaming_tab > close_request)
                renaming_tab--;
        }
        else if (switch_request >= 0)
            active_tab = switch_request;

        if (new_tab_request)
        {
            std::string name = unique_untitled(scripts, tabs);
            open_tab(tabs, active_tab, name); // файла на диске ещё нет, откроет пустую вкладку
            renaming_tab = active_tab;
            strncpy_s(rename_buf, name.c_str(), sizeof(rename_buf) - 1);
            rename_focus_pending = true;
        }

        ImGui::PushStyleColor(ImGuiCol_Border, border_inner);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.06f, 1.f));
        float ed_h = ImGui::GetContentRegionAvail().y;
        if (active_tab >= 0 && active_tab < (int)tabs.size())
        {
            tabs[active_tab].ed.render("##lua_ed", ImVec2(ed_w, ed_h));
        }
        else
        {
            ImGui::BeginChild("##lua_ed_empty", ImVec2(ed_w, ed_h), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 msg_size = ImGui::CalcTextSize("no file open");
            ImGui::SetCursorPos(ImVec2((avail.x - msg_size.x) * 0.5f, (avail.y - msg_size.y) * 0.5f));
            ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "no file open");
            ImGui::EndChild();
        }
        ImGui::PopStyleColor(2);

        ImGui::EndChild();

        ImGui::SameLine(0.f, gap);

        ImGui::BeginChild("##lua_list", ImVec2(list_w - margin, inner_h), ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
        {
            constexpr float item_h = 24.f;
            constexpr float text_pad_x = 8.f;
            ImDrawList* dl = ImGui::GetWindowDrawList();

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.f, 0.f, 0.f, 0.f));

            for (int i = 0; i < (int)scripts.size(); ++i)
            {
                const std::string& name = scripts[i];
                int open_idx = find_tab(tabs, name);
                bool is_active = (open_idx >= 0 && open_idx == active_tab);
                bool is_open = (open_idx >= 0);

                ImGui::PushID(i);
                ImVec2 item_pos = ImGui::GetCursorScreenPos();
                float item_w = ImGui::GetContentRegionAvail().x;
                ImVec2 item_max(item_pos.x + item_w, item_pos.y + item_h);

                if (ImGui::Selectable("", is_active, 0, ImVec2(item_w, item_h)))
                    open_tab(tabs, active_tab, name);

                bool hovered = ImGui::IsItemHovered();
                if (hovered && !is_active)
                    dl->AddRectFilled(item_pos, item_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
                if (is_active)
                    dl->AddRect(ImVec2(item_pos.x + 2.f, item_pos.y + 2.f), ImVec2(item_max.x - 2.f, item_max.y - 2.f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.35f)));

                ImVec2 ts = ImGui::CalcTextSize(name.c_str());
                ImU32 text_col = ImGui::GetColorU32(is_active ? ImVec4(1.f, 1.f, 1.f, 1.f) : (is_open ? ImVec4(0.8f, 0.8f, 0.8f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f)));
                widgets::text_outlined(dl, ImVec2(item_pos.x + text_pad_x, item_pos.y + (item_h - ts.y) * 0.5f), text_col, name.c_str());

                ImGui::PopID();
            }

            ImGui::PopStyleColor(3);
        }
        ImGui::EndChild();

        ImGui::EndChild();
        ImGui::PopStyleColor();

        float btn_y = ws.y - margin - btn_h;
        float total_w = ws.x - margin * 2.f;
        const char* labels[] = { "Run", "Save", "Clear", "Clear Out", "Copy Out", "Folder" };
        constexpr int btn_count = 6;
        float bw = (total_w - gap * (btn_count - 1)) / (float)btn_count;

        for (int i = 0; i < btn_count; ++i)
        {
            ImVec2 min(wp.x + margin + i * (bw + gap), wp.y + btn_y);
            ImVec2 max(min.x + bw, min.y + btn_h);
            if (action_button(labels[i], min, max))
            {
                bool has_active = active_tab >= 0 && active_tab < (int)tabs.size();
                if (i == 0 && has_active)
                {
                    Cheat::Features::LuaExecutor::Initialize();
                    Cheat::Features::LuaExecutor::ClearOutput();
                    std::string src = tabs[active_tab].ed.get_text();
                    Cheat::Features::LuaVM::Execute(src, tabs[active_tab].name.c_str());
                    sync_output_from_log();
                }
                else if (i == 1 && has_active)
                {
                    save_script(tabs[active_tab].ed, tabs[active_tab].name);
                    scripts = list_scripts();
                }
                else if (i == 2 && has_active)
                    tabs[active_tab].ed.set_text("");
                else if (i == 3)
                {
                    Cheat::Features::LuaExecutor::ClearOutput();
                    lua_set_output_ok();
                }
                else if (i == 4)
                    ImGui::SetClipboardText(err_ed.get_text().c_str());
                else if (i == 5)
                {
                    ensure_scripts();
                    ShellExecuteA(nullptr, "open", scripts_dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
        }

        ImVec2 lua_pos = wp;
        ImVec2 lua_size = ws;
        ImGui::End();

        render_lua_output_panel(lua_pos, lua_size);
    }
}
