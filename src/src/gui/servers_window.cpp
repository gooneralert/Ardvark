#include "pch.h"
#include "glass.h"
#include "servers_window.h"
#include "widgets/text.h"
#include "imgui.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"

#include <shellapi.h>
#include <winhttp.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui
{
    static const ImVec4 border_outer = ImVec4(0.13f, 0.13f, 0.13f, 1.f);
    static const ImVec4 border_inner = ImVec4(0.18f, 0.18f, 0.18f, 1.f);

    struct server_entry_t
    {
        std::string id;
        int playing = 0;
        int max_players = 0;
        int ping = 0;
        float fps = 0.0f;
    };

    namespace
    {
        std::mutex g_mutex;
        std::vector<server_entry_t> g_servers;
        std::string g_status = "idle";
        std::string g_job_id;
        std::uint64_t g_place_id = 0;
        std::atomic<bool> g_fetching{ false };
        int g_sort = 0;
        float g_auto_refresh = 0.0f;
        float g_refresh_timer = 0.0f;

        // ---- gelato serverbrowser json helpers (1:1) ----

        std::string extract_string_field(const std::string& object, const char* key)
        {
            const auto needle = std::string("\"") + key + "\":\"";
            const auto pos = object.find(needle);
            if (pos == std::string::npos)
                return {};

            const auto start = pos + needle.size();
            const auto end = object.find('"', start);
            if (end == std::string::npos)
                return {};

            return object.substr(start, end - start);
        }

        int extract_int_field(const std::string& object, const char* key)
        {
            const auto needle = std::string("\"") + key + "\":";
            const auto pos = object.find(needle);
            if (pos == std::string::npos)
                return 0;

            const auto start = pos + needle.size();
            return std::atoi(object.c_str() + start);
        }

        float extract_float_field(const std::string& object, const char* key)
        {
            const auto needle = std::string("\"") + key + "\":";
            const auto pos = object.find(needle);
            if (pos == std::string::npos)
                return 0.0f;

            const auto start = pos + needle.size();
            return static_cast<float>(std::atof(object.c_str() + start));
        }

        std::vector<server_entry_t> parse_servers(const std::string& body)
        {
            std::vector<server_entry_t> out;

            const auto data_pos = body.find("\"data\":[");
            if (data_pos == std::string::npos)
                return out;

            auto cursor = data_pos + 8;
            while (cursor < body.size())
            {
                while (cursor < body.size() &&
                       (body[cursor] == ' ' || body[cursor] == '\n' ||
                        body[cursor] == '\r' || body[cursor] == '\t' || body[cursor] == ','))
                    ++cursor;

                if (cursor >= body.size() || body[cursor] == ']')
                    break;

                if (body[cursor] != '{')
                    break;

                int depth = 0;
                const auto begin = cursor;
                for (; cursor < body.size(); ++cursor)
                {
                    if (body[cursor] == '{')
                        ++depth;
                    else if (body[cursor] == '}')
                    {
                        --depth;
                        if (depth == 0)
                        {
                            ++cursor;
                            break;
                        }
                    }
                }

                const auto object = body.substr(begin, cursor - begin);
                server_entry_t entry;
                entry.id = extract_string_field(object, "id");
                entry.playing = extract_int_field(object, "playing");
                entry.max_players = extract_int_field(object, "maxPlayers");
                entry.ping = extract_int_field(object, "ping");
                entry.fps = extract_float_field(object, "fps");
                if (!entry.id.empty())
                    out.push_back(std::move(entry));
            }

            return out;
        }

        std::string http_get_text(const wchar_t* host, const wchar_t* path)
        {
            std::string body;

            HINTERNET ses = WinHttpOpen(L"jewsploit/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!ses)
                return body;

            HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (!con)
            {
                WinHttpCloseHandle(ses);
                return body;
            }

            HINTERNET req = WinHttpOpenRequest(con, L"GET", path, nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!req)
            {
                WinHttpCloseHandle(con);
                WinHttpCloseHandle(ses);
                return body;
            }

            if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(req, nullptr))
            {
                DWORD avail = 0;
                for (;;)
                {
                    if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0)
                        break;

                    std::vector<char> chunk(avail);
                    DWORD read = 0;
                    if (!WinHttpReadData(req, chunk.data(), avail, &read) || read == 0)
                        break;

                    body.append(chunk.data(), read);
                }
            }

            WinHttpCloseHandle(req);
            WinHttpCloseHandle(con);
            WinHttpCloseHandle(ses);
            return body;
        }

        void refresh()
        {
            if (g_fetching.exchange(true))
                return;

            if (!Cheat::Globals::InstanceDataModel.address)
            {
                g_status = "no datamodel";
                g_fetching = false;
                return;
            }

            g_place_id = Cheat::Globals::InstanceDataModel.GetPlaceId();
            g_job_id = Cheat::Globals::InstanceDataModel.GetJobId();

            if (!g_place_id)
            {
                g_status = "no place id";
                g_fetching = false;
                return;
            }

            {
                std::lock_guard lock(g_mutex);
                g_status = "fetching...";
            }

            const auto id = g_place_id;
            std::thread([id]
            {
                wchar_t path[160]{};
                std::swprintf(path, 160,
                    L"/v1/games/%llu/servers/Public?sortOrder=Desc&limit=100",
                    static_cast<unsigned long long>(id));

                std::string body = http_get_text(L"games.roblox.com", path);
                auto parsed = parse_servers(body);

                {
                    std::lock_guard lock(g_mutex);
                    g_servers = std::move(parsed);
                    g_status = g_servers.empty() ? "no servers" : "ok";
                }
                g_fetching = false;
            }).detach();
        }

        void copy_share_link(const server_entry_t& server)
        {
            if (!g_place_id || server.id.empty())
                return;

            char url[512]{};
            std::snprintf(url, sizeof(url),
                "https://www.roblox.com/games/start?placeId=%llu&gameInstanceId=%s",
                static_cast<unsigned long long>(g_place_id),
                server.id.c_str());
            ImGui::SetClipboardText(url);

            std::lock_guard lock(g_mutex);
            g_status = "copied share link";
        }

        void join_server(const server_entry_t& server)
        {
            if (!g_place_id || server.id.empty())
                return;

            char url[512]{};
            std::snprintf(url, sizeof(url),
                "roblox://experiences/start?placeId=%llu&gameInstanceId=%s",
                static_cast<unsigned long long>(g_place_id),
                server.id.c_str());

            ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);

            std::lock_guard lock(g_mutex);
            g_status = "joining...";
        }

        const char* shorten_job_id(const std::string& id, char* buf, int buf_n)
        {
            if (id.size() <= 18)
            {
                std::snprintf(buf, (size_t)buf_n, "%s", id.c_str());
                return buf;
            }

            std::snprintf(buf, (size_t)buf_n, "%.8s...%.6s",
                id.c_str(), id.c_str() + id.size() - 6);
            return buf;
        }

        ImU32 ping_color(int ping)
        {
            if (ping <= 0)
                return IM_COL32(140, 140, 153, 255);
            if (ping <= 70)
                return IM_COL32(102, 224, 132, 255);
            if (ping <= 120)
                return IM_COL32(242, 204, 89, 255);
            return IM_COL32(242, 114, 102, 255);
        }

        ImU32 fill_ratio_color(int playing, int max_players)
        {
            if (max_players <= 0)
                return IM_COL32(178, 178, 191, 255);

            const float ratio = static_cast<float>(playing) / static_cast<float>(max_players);
            if (ratio >= 0.95f)
                return IM_COL32(242, 114, 102, 255);
            if (ratio >= 0.75f)
                return IM_COL32(242, 204, 89, 255);
            return IM_COL32(191, 199, 216, 255);
        }

        bool action_button(const char* id, const char* label, ImVec2 min, ImVec2 max)
        {
            ImGui::PushID(id);
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
            widgets::text_outlined(draw,
                ImVec2(min.x + (max.x - min.x - ts.x) * 0.5f,
                       min.y + (max.y - min.y - ts.y) * 0.5f),
                ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.f)), label);

            ImGui::PopID();
            return clicked;
        }

        void info_row(ImVec2 pos, float width, const char* label, const char* value)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            widgets::text_outlined(dl, pos, IM_COL32(140, 140, 140, 255), label);
            ImVec2 vs = ImGui::CalcTextSize(value);
            widgets::text_outlined(dl, ImVec2(pos.x + width - vs.x, pos.y),
                IM_COL32(230, 230, 230, 255), value);
        }

        void draw_server_list()
        {
            std::vector<server_entry_t> list;
            {
                std::lock_guard lock(g_mutex);
                list = g_servers;
            }

            if (g_sort == 0)
                std::sort(list.begin(), list.end(),
                    [](const server_entry_t& a, const server_entry_t& b) { return a.playing > b.playing; });
            else if (g_sort == 1)
                std::sort(list.begin(), list.end(),
                    [](const server_entry_t& a, const server_entry_t& b) { return a.playing < b.playing; });
            else if (g_sort == 2)
                std::sort(list.begin(), list.end(),
                    [](const server_entry_t& a, const server_entry_t& b) { return a.ping < b.ping; });
            else
                std::sort(list.begin(), list.end(),
                    [](const server_entry_t& a, const server_entry_t& b) { return a.ping > b.ping; });

            const ImGuiTableFlags table_flags =
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_PadOuterX |
                ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("##server_table", 4, table_flags, ImVec2(-1.0f, -1.0f)))
            {
                ImGui::TableSetupColumn("players", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableSetupColumn("job id", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("fps", ImGuiTableColumnFlags_WidthFixed, 52.0f);
                ImGui::TableSetupColumn("ping", ImGuiTableColumnFlags_WidthFixed, 64.0f);
                ImGui::TableSetupScrollFreeze(0, 1);

                ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("players");
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("job id");
                ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("fps");
                ImGui::TableSetColumnIndex(3); ImGui::TextDisabled("ping");

                for (std::size_t i = 0; i < list.size(); ++i)
                {
                    const auto& server = list[i];
                    const bool is_current = !g_job_id.empty() && server.id == g_job_id;

                    ImGui::PushID((int)i);
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, 26.0f);

                    if (is_current)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                            IM_COL32(120, 160, 255, 28));

                    ImGui::TableSetColumnIndex(0);
                    {
                        char players_buf[32]{};
                        std::snprintf(players_buf, sizeof(players_buf),
                            "%d/%d", server.playing, server.max_players);
                        ImGui::TextColored(
                            ImGui::ColorConvertU32ToFloat4(
                                fill_ratio_color(server.playing, server.max_players)),
                            "%s", players_buf);
                    }

                    ImGui::TableSetColumnIndex(1);
                    {
                        char short_buf[32]{};
                        shorten_job_id(server.id, short_buf, 32);
                        ImGui::Selectable(short_buf, is_current,
                            ImGuiSelectableFlags_SpanAllColumns |
                            ImGuiSelectableFlags_AllowOverlap);

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(server.id.c_str());
                            ImGui::TextDisabled("LMB copy  |  RMB join");
                            if (is_current)
                                ImGui::TextColored(ImVec4(0.55f, 0.7f, 1.f, 1.f), "current server");
                            ImGui::EndTooltip();
                        }

                        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                            copy_share_link(server);
                        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                            join_server(server);
                    }

                    ImGui::TableSetColumnIndex(2);
                    if (server.fps > 0.5f)
                        ImGui::TextDisabled("%.0f", server.fps);
                    else
                        ImGui::TextDisabled("-");

                    ImGui::TableSetColumnIndex(3);
                    if (server.ping > 0)
                        ImGui::TextColored(
                            ImGui::ColorConvertU32ToFloat4(ping_color(server.ping)),
                            "%dms", server.ping);
                    else
                        ImGui::TextDisabled("-");

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            if (list.empty())
                ImGui::TextDisabled(
                    g_fetching.load() ? "loading servers..." : "no public servers");
        }
    }

    void render_servers_window(bool* open)
    {
        if (!open || !*open)
            return;

        constexpr float title_h = 26.f;
        constexpr float margin = 3.f;
        constexpr float gap = 6.f;
        constexpr float pad = 8.f;

        // первый показ — подтягиваем список сразу
        static bool first = true;
        if (first)
        {
            refresh();
            first = false;
        }

        // auto refresh
        if (g_auto_refresh > 0.5f && !g_fetching.load())
        {
            g_refresh_timer += ImGui::GetIO().DeltaTime;
            if (g_refresh_timer >= g_auto_refresh)
            {
                g_refresh_timer = 0.0f;
                refresh();
            }
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(ImVec2(center.x - 90.f, center.y + 30.f),
            ImGuiCond_Once, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(720.f, 460.f), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(560.f, 320.f), ImVec2(FLT_MAX, FLT_MAX));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_outer);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.06f, 0.07f, 0.34f));
        bool visible = ImGui::Begin("##servers_window", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        if (visible)
        {
            const ImVec2 gp = ImGui::GetWindowPos();
            const ImVec2 gs = ImGui::GetWindowSize();
            glass::add_rect(gp.x, gp.y, gs.x, gs.y, 8.f);
        }

        if (!visible)
        {
            ImGui::End();
            return;
        }

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        // title bar
        draw->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + title_h), IM_COL32(20, 20, 20, 255));
        ImVec2 title_ts = ImGui::CalcTextSize("server explorer");
        widgets::text_outlined(draw,
            ImVec2(wp.x + (ws.x - title_ts.x) * 0.5f, wp.y + (title_h - title_ts.y) * 0.5f),
            IM_COL32(230, 230, 230, 255), "server explorer");

        ImVec2 xsz = ImGui::CalcTextSize("X");
        ImVec2 xmin(wp.x + ws.x - xsz.x - 14.f, wp.y);
        ImGui::SetCursorScreenPos(xmin);
        ImGui::InvisibleButton("##servers_close", ImVec2(xsz.x + 14.f, title_h));
        bool xhov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            *open = false;
        widgets::text_outlined(draw,
            ImVec2(xmin.x + 7.f, wp.y + (title_h - xsz.y) * 0.5f),
            xhov ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 160, 255), "X");

        float body_top = title_h + margin;
        float body_h = ws.y - body_top - margin;
        float body_w = ws.x - margin * 2.f;

        ImGui::SetCursorPos(ImVec2(margin, body_top));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##servers_body", ImVec2(body_w, body_h),
            ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();

        float filter_w = 190.f;
        if (filter_w > body_w * 0.32f)
            filter_w = body_w * 0.32f;

        // ---- левая колонка: фильтры + сессия ----
        ImGui::SetCursorPos(ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_inner);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
        ImGui::BeginChild("##servers_filters", ImVec2(filter_w, body_h),
            ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        {
            ImVec2 fp = ImGui::GetWindowPos();
            widgets::text_outlined(draw, fp + ImVec2(pad, pad),
                IM_COL32(230, 230, 230, 255), "filters");

            ImGui::SetCursorPos(ImVec2(pad, pad + 20.f));
            ImGui::TextDisabled("sort");
            ImGui::SetNextItemWidth(-1.0f);
            const char* sorts[] = { "Players Desc", "Players Asc", "Ping Asc", "Ping Desc" };
            ImGui::Combo("##server_sort", &g_sort, sorts, IM_ARRAYSIZE(sorts));

            ImGui::Spacing();
            ImGui::TextDisabled("auto refresh (sec)");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##server_auto", &g_auto_refresh, 0.0f, 60.0f, "%.0f");

            ImGui::Spacing();
            if (action_button("##server_refresh",
                    g_fetching.load() ? "Refreshing..." : "Refresh",
                    ImVec2(fp.x + pad, ImGui::GetCursorScreenPos().y),
                    ImVec2(fp.x + filter_w - pad, ImGui::GetCursorScreenPos().y + 26.f)))
            {
                refresh();
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 32.f);
            ImGui::Separator();
            ImGui::Spacing();

            char tmp[64]{};
            std::snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)g_place_id);

            float row_w = filter_w - pad * 2.f;
            float ry = ImGui::GetCursorScreenPos().y;
            info_row(ImVec2(fp.x + pad, ry), row_w, "place", tmp);
            ry += 16.f;
            info_row(ImVec2(fp.x + pad, ry), row_w, "job",
                g_job_id.size() > 14 ? shorten_job_id(g_job_id, tmp, 64)
                                     : (g_job_id.empty() ? "-" : g_job_id.c_str()));

            int count = 0;
            {
                std::lock_guard lock(g_mutex);
                count = (int)g_servers.size();
            }

            char cnt_buf[16]{};
            std::snprintf(cnt_buf, sizeof(cnt_buf), "%d", count);
            ry += 16.f;
            info_row(ImVec2(fp.x + pad, ry), row_w, "servers", cnt_buf);
            ry += 16.f;
            info_row(ImVec2(fp.x + pad, ry), row_w, "status", g_status.c_str());

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 72.f);
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("LMB  copy share link");
            ImGui::TextDisabled("RMB  join server");
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, gap);
        ImGui::BeginChild("##servers_list", ImVec2(-1.f, body_h),
            ImGuiChildFlags_Borders);
        {
            draw_server_list();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(); // border_inner
        ImGui::EndChild();      // servers_body
        ImGui::End();
    }
} // namespace gui