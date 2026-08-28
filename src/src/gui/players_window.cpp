#include "pch.h"
#include "glass.h"
#include "players_window.h"
#include "widgets/text.h"
#include "imgui.h"
#include "core/player/PlayerHandler.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"
#include "features/misc/PlayerAvatars.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace gui
{
    static const ImVec4 border_outer = ImVec4(0.13f, 0.13f, 0.13f, 1.f);
    static const ImVec4 border_inner = ImVec4(0.18f, 0.18f, 0.18f, 1.f);

    static std::uint64_t g_sel = 0;
    static std::int64_t g_sel_uid = 0;
    static char g_sel_user[64]{};
    static char g_sel_disp[64]{};
    static std::unordered_set<std::uint64_t> g_friends;
    static std::uint64_t g_target = 0;
    static std::uint64_t g_spectate = 0;
    static std::uint64_t g_spec_prev_subj = 0;
    static std::int32_t g_spec_prev_type = 0;

    struct row_t
    {
        std::uint64_t addr;
        std::int64_t uid;
        std::string user;
        std::string disp;
    };

    static void set_sel(const row_t& r)
    {
        g_sel = r.addr;
        g_sel_uid = r.uid;
        std::snprintf(g_sel_user, sizeof(g_sel_user), "%s", r.user.c_str());
        std::snprintf(g_sel_disp, sizeof(g_sel_disp), "%s", r.disp.c_str());
    }

    static bool action_button(const char* id, const char* label, ImVec2 min, ImVec2 max)
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
        widgets::text_outlined(draw, ImVec2(min.x + (max.x - min.x - ts.x) * 0.5f, min.y + (max.y - min.y - ts.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.f)), label);

        ImGui::PopID();
        return clicked;
    }

    static void profile_row(ImVec2 pos, float width, const char* label, const char* value)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        widgets::text_outlined(dl, pos, IM_COL32(140, 140, 140, 255), label);
        ImVec2 vs = ImGui::CalcTextSize(value);
        widgets::text_outlined(dl, ImVec2(pos.x + width - vs.x, pos.y), IM_COL32(230, 230, 230, 255), value);
    }

    static const char* status_for(const Cheat::PlayerCache& c)
    {
        if (c.is_corpse)
            return "dead";
        if (g_friends.count(c.address))
            return "friend";
        std::uint64_t local_team = Cheat::PlayerHandler::LocalTeamFolder();
        if (local_team && Cheat::PlayerHandler::IsTeammate(c, local_team))
            return "teammate";
        if (!c.is_player)
            return "bot";
        if (local_team && c.team_folder && c.team_folder != local_team)
            return "frag";
        return "neutral";
    }

    static ImU32 status_col(const char* st)
    {
        if (std::strcmp(st, "friend") == 0 || std::strcmp(st, "teammate") == 0)
            return IM_COL32(90, 200, 120, 255);
        if (std::strcmp(st, "frag") == 0)
            return IM_COL32(220, 80, 80, 255);
        if (std::strcmp(st, "dead") == 0)
            return IM_COL32(140, 140, 150, 255);
        return IM_COL32(180, 185, 195, 255);
    }

    static void do_teleport(const Cheat::PlayerCache& c)
    {
        if (!c.humanoidRootPart || !Cheat::Globals::Players)
            return;

        std::uint64_t local = g_Memory.Read<std::uint64_t>(
            Cheat::Globals::Players->address + ::Player::LocalPlayer);
        if (!g_Memory.IsValid(local))
            return;

        std::uint64_t lchar = g_Memory.Read<std::uint64_t>(
            local + ::Player::ModelInstance);
        if (!g_Memory.IsValid(lchar))
            return;

        Cheat::Instance ch(lchar);
        auto hrp = ch.FindFirstChild("HumanoidRootPart");
        if (!hrp || !g_Memory.IsValid(hrp->address))
            hrp = ch.FindFirstChild("Torso");
        if (!hrp || !g_Memory.IsValid(hrp->address))
            return;

        Vector3 pos = Cheat::BasePart(c.humanoidRootPart->address).GetPosition();
        pos.y += 3.f;
        Cheat::BasePart(hrp->address).SetPosition(pos);
    }

    static void do_spectate(const Cheat::PlayerCache& c, bool on)
    {
        if (!Cheat::Globals::Workspace)
            return;

        auto cam = Cheat::Globals::Workspace->GetCurrentCamera();
        if (!cam || !g_Memory.IsValid(cam->address))
            return;

        if (on)
        {
            if (!c.humanoid || !g_Memory.IsValid(c.humanoid->address))
                return;

            g_spec_prev_subj = g_Memory.Read<std::uint64_t>(
                cam->address + ::Camera::CameraSubject);
            g_spec_prev_type = g_Memory.Read<std::int32_t>(
                cam->address + ::Camera::CameraType);

            g_Memory.Write<std::uint64_t>(
                cam->address + ::Camera::CameraSubject,
                c.humanoid->address);
            g_Memory.Write<std::int32_t>(
                cam->address + ::Camera::CameraType, 5);
            g_spectate = c.address;
        }
        else
        {
            if (g_Memory.IsValid(g_spec_prev_subj))
            {
                g_Memory.Write<std::uint64_t>(
                    cam->address + ::Camera::CameraSubject,
                    g_spec_prev_subj);
            }
            g_Memory.Write<std::int32_t>(
                cam->address + ::Camera::CameraType,
                g_spec_prev_type);
            g_spectate = 0;
            g_spec_prev_subj = 0;
        }
    }

    void render_players_window(bool* open)
    {
        if (!open || !*open)
            return;

        constexpr float title_h = 26.f;
        constexpr float margin = 3.f;
        constexpr float gap = 6.f;

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        static float fit_h = 380.f;

        ImGui::SetNextWindowPos(ImVec2(center.x - 30.f, center.y + 30.f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(620.f, fit_h), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_outer);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.06f, 0.07f, 0.34f));  // translucent so the acrylic shows through
        bool visible = ImGui::Begin("##players_window", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | 0 | ImGuiWindowFlags_NoResize);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        if (visible)
        {
            const ImVec2 gp = ImGui::GetWindowPos();
            const ImVec2 gs = ImGui::GetWindowSize();
            glass::add_rect(gp.x, gp.y, gs.x, gs.y, 8.f);   // acrylic backdrop for this window
        }

        if (!visible)
        {
            ImGui::End();
            return;
        }

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + title_h), IM_COL32(20, 20, 20, 255));
        ImVec2 title_ts = ImGui::CalcTextSize("players");
        widgets::text_outlined(draw, ImVec2(wp.x + (ws.x - title_ts.x) * 0.5f, wp.y + (title_h - title_ts.y) * 0.5f),
            IM_COL32(230, 230, 230, 255), "players");

        ImVec2 xsz = ImGui::CalcTextSize("X");
        ImVec2 xmin(wp.x + ws.x - xsz.x - 14.f, wp.y);
        ImGui::SetCursorScreenPos(xmin);
        ImGui::InvisibleButton("##players_close", ImVec2(xsz.x + 14.f, title_h));
        bool xhov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            *open = false;
        widgets::text_outlined(draw, ImVec2(xmin.x + 7.f, wp.y + (title_h - xsz.y) * 0.5f),
            xhov ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 160, 255), "X");

        float body_top = title_h + margin;
        float body_h = ws.y - body_top - margin;
        float body_w = ws.x - margin * 2.f;

        ImGui::SetCursorPos(ImVec2(margin, body_top));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##players_body", ImVec2(body_w, body_h), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();

        float list_w = body_w * 0.28f;
        if (list_w < 140.f) list_w = 140.f;
        float profile_w = body_w - list_w - gap;

        constexpr float header_h = 22.f;

        std::vector<row_t> rows;
        rows.reserve(Cheat::PlayerHandler::GetPlayerCount());
        Cheat::PlayerHandler::ForEachPlayer([&](const Cheat::PlayerCache& c)
        {
            if (!c.is_player)
                return;
            row_t r{};
            r.addr = c.address;
            r.uid = c.user_id;
            r.user = c.name.empty() ? "unknown" : c.name;
            r.disp = c.displayName.empty() ? r.user : c.displayName;
            rows.push_back(std::move(r));
        });

        if (rows.empty())
        {
            g_sel = 0;
            g_sel_uid = 0;
            g_sel_user[0] = 0;
            g_sel_disp[0] = 0;
        }
        else
        {
            bool still = false;
            for (const auto& r : rows)
            {
                if (r.addr == g_sel)
                {
                    still = true;
                    g_sel_uid = r.uid;
                    std::snprintf(g_sel_user, sizeof(g_sel_user), "%s", r.user.c_str());
                    std::snprintf(g_sel_disp, sizeof(g_sel_disp), "%s", r.disp.c_str());
                    break;
                }
            }
            if (!still)
                set_sel(rows[0]);
        }

        Cheat::PlayerCache cur{};
        bool have = false;
        if (g_sel)
        {
            cur = Cheat::PlayerHandler::GetCachedPlayer(g_sel);
            have = (cur.address == g_sel);
        }

        ImGui::SetCursorPos(ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_inner);
        ImGui::BeginChild("##players_list", ImVec2(list_w, body_h), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        {
            ImVec2 lp = ImGui::GetWindowPos();
            ImVec2 lsz = ImGui::GetWindowSize();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 hts = ImGui::CalcTextSize("players");
            widgets::text_outlined(dl, ImVec2(lp.x + (lsz.x - hts.x) * 0.5f, lp.y + (header_h - hts.y) * 0.5f), IM_COL32(210, 210, 210, 255), "players");
            dl->AddLine(ImVec2(lp.x, lp.y + header_h), ImVec2(lp.x + lsz.x, lp.y + header_h), ImGui::GetColorU32(border_inner));

            ImGui::SetCursorPos(ImVec2(0.f, header_h));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.f);
            ImGui::BeginChild("##players_scroll", ImVec2(lsz.x, lsz.y - header_h), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

            constexpr float item_h = 26.f;
            constexpr float text_pad_x = 10.f;

            if (rows.empty())
            {
                ImVec2 p = ImGui::GetCursorScreenPos();
                widgets::text_outlined(dl, ImVec2(p.x + text_pad_x, p.y + 8.f), IM_COL32(110, 110, 110, 255), "no players");
            }
            else
            {
                for (int i = 0; i < (int)rows.size(); ++i)
                {
                    const row_t& r = rows[i];
                    bool active = (r.addr == g_sel);
                    ImGui::PushID(i);
                    ImVec2 item_pos = ImGui::GetCursorScreenPos();
                    float item_w = ImGui::GetContentRegionAvail().x;
                    ImVec2 item_max(item_pos.x + item_w, item_pos.y + item_h);

                    if (ImGui::Selectable("", active, 0, ImVec2(item_w, item_h)))
                        set_sel(r);

                    bool hovered = ImGui::IsItemHovered();
                    if (hovered && !active)
                        dl->AddRectFilled(item_pos, item_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
                    if (active)
                        dl->AddRect(ImVec2(item_pos.x + 2.f, item_pos.y + 2.f), ImVec2(item_max.x - 2.f, item_max.y - 2.f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.35f)));

                    ImVec2 ts = ImGui::CalcTextSize(r.disp.c_str());
                    ImU32 text_col = ImGui::GetColorU32(active ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.65f, 0.65f, 0.65f, 1.f));
                    widgets::text_outlined(dl, ImVec2(item_pos.x + text_pad_x, item_pos.y + (item_h - ts.y) * 0.5f), text_col, r.disp.c_str());

                    ImGui::PopID();
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(list_w + gap, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, border_inner);
        ImGui::BeginChild("##players_profile", ImVec2(profile_w, body_h), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        {
            ImVec2 pp = ImGui::GetWindowPos();
            ImVec2 psz = ImGui::GetWindowSize();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            ImVec2 hts = ImGui::CalcTextSize("profile");
            widgets::text_outlined(dl, ImVec2(pp.x + (psz.x - hts.x) * 0.5f, pp.y + (header_h - hts.y) * 0.5f), IM_COL32(210, 210, 210, 255), "profile");
            dl->AddLine(ImVec2(pp.x, pp.y + header_h), ImVec2(pp.x + psz.x, pp.y + header_h), ImGui::GetColorU32(border_inner));

            constexpr float pad = 12.f;
            constexpr float btn_h = 26.f;
            constexpr float btn_gap = 6.f;
            float btn_block_h = btn_h * 2.f + btn_gap;
            float row_w = psz.x - pad * 2.f;

            if (have)
            {
                std::uint64_t paddr = cur.player_address ? cur.player_address : cur.address;
                if (g_Memory.IsValid(paddr))
                {
                    std::int64_t live = Cheat::Player(paddr).GetUserId();
                    if (live > 0)
                        g_sel_uid = live;
                }
                if (g_sel_uid <= 0 && g_sel_user[0])
                {
                    std::int64_t by_name = Cheat::Features::PlayerAvatars::LookupUserId(g_sel_user);
                    if (by_name > 0)
                        g_sel_uid = by_name;
                }
            }

            float avatar_size = std::min(160.f, psz.x * 0.42f);
            ImVec2 av_min(pp.x + pad, pp.y + header_h + pad);
            ImVec2 av_max(av_min.x + avatar_size, av_min.y + avatar_size);
            dl->AddRectFilled(av_min, av_max, IM_COL32(16, 16, 16, 255));
            dl->AddRect(av_min, av_max, ImGui::GetColorU32(border_inner));

            ID3D11ShaderResourceView* srv = Cheat::Features::PlayerAvatars::Get(g_sel_uid);
            if (srv)
            {
                dl->AddImageRounded(
                    ImTextureID((uintptr_t)srv),
                    av_min, av_max,
                    ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
                    IM_COL32(255, 255, 255, 255),
                    0.f);
            }
            else
            {
                const char* t = g_sel_disp[0] ? g_sel_disp : "no avatar";
                ImVec2 na = ImGui::CalcTextSize(t);
                widgets::text_outlined(dl,
                    ImVec2(av_min.x + (avatar_size - na.x) * 0.5f, av_min.y + (avatar_size - na.y) * 0.5f),
                    IM_COL32(70, 70, 70, 255), t);
            }

            float info_x = av_max.x + pad;
            float info_w = psz.x - pad - (info_x - pp.x);
            if (info_w < 80.f) info_w = 80.f;

            float line_h = ImGui::GetTextLineHeight();
            constexpr float bar_h = 14.f;
            constexpr float info_gap = 5.f;

            float hp = 0.f;
            float max_hp = 100.f;
            if (have && cur.humanoid && g_Memory.IsValid(cur.humanoid->address))
            {
                Cheat::Humanoid hum(cur.humanoid->address);
                hp = hum.GetHealth();
                max_hp = hum.GetMaxHealth();
                if (max_hp < 1.f)
                    max_hp = 100.f;
            }

            char hp_text[32];
            std::snprintf(hp_text, sizeof(hp_text), "%.0f / %.0f", hp, max_hp);
            ImVec2 hp_size = ImGui::CalcTextSize(hp_text);
            float value_overlap = hp_size.y * 0.55f;

            const char* disp = g_sel_disp[0] ? g_sel_disp : "-";
            char user_buf[72]{};
            if (g_sel_user[0])
                std::snprintf(user_buf, sizeof(user_buf), "@%s", g_sel_user);
            else
                std::snprintf(user_buf, sizeof(user_buf), "-");

            const char* st = have ? status_for(cur) : "-";

            float y = av_min.y;
            widgets::text_outlined(dl, ImVec2(info_x, y), IM_COL32(240, 240, 240, 255), disp);
            y += ImGui::CalcTextSize(disp).y + 3.f;
            widgets::text_outlined(dl, ImVec2(info_x, y), IM_COL32(130, 130, 130, 255), user_buf);
            y += line_h + 6.f;

            ImVec2 bar_min(info_x, y);
            ImVec2 bar_max(info_x + info_w, y + bar_h);
            dl->AddRectFilled(bar_min, bar_max, IM_COL32(31, 31, 31, 255));
            float hp_frac = max_hp > 0.f ? std::clamp(hp / max_hp, 0.f, 1.f) : 0.f;
            float fill_x = bar_min.x + info_w * hp_frac;
            if (hp_frac > 0.f)
                dl->AddRectFilled(bar_min, ImVec2(fill_x, bar_max.y), IM_COL32(217, 217, 217, 255));
            dl->AddRect(bar_min, bar_max, IM_COL32(0, 0, 0, 255));

            float value_x = fill_x - hp_size.x * 0.5f;
            if (value_x < bar_min.x) value_x = bar_min.x;
            if (value_x + hp_size.x > bar_max.x) value_x = bar_max.x - hp_size.x;
            widgets::text_outlined(dl, ImVec2(value_x, bar_max.y - value_overlap), IM_COL32(255, 255, 255, 255), hp_text);

            y = bar_max.y + (hp_size.y - value_overlap) + 8.f;
            float side_row = line_h + info_gap;
            widgets::text_outlined(dl, ImVec2(info_x, y), IM_COL32(140, 140, 140, 255), "status");
            ImVec2 ss = ImGui::CalcTextSize(st);
            widgets::text_outlined(dl, ImVec2(info_x + info_w - ss.x, y), status_col(st), st);
            y += side_row;

            if (g_target == g_sel && g_sel)
            {
                widgets::text_outlined(dl, ImVec2(info_x, y), IM_COL32(255, 90, 90, 255), "targeted");
                y += side_row;
            }
            if (g_spectate == g_sel && g_sel)
            {
                widgets::text_outlined(dl, ImVec2(info_x, y), IM_COL32(115, 190, 255, 255), "spectating");
                y += side_row;
            }

            char uid_buf[32]{};
            std::snprintf(uid_buf, sizeof(uid_buf), "%lld", (long long)g_sel_uid);

            float dist = 0.f;
            if (have && cur.humanoidRootPart && Cheat::Globals::Players)
            {
                std::uint64_t lp = g_Memory.Read<std::uint64_t>(
                    Cheat::Globals::Players->address + ::Player::LocalPlayer);
                Cheat::PlayerCache loc = Cheat::PlayerHandler::GetCachedPlayer(lp);
                if (loc.humanoidRootPart && g_Memory.IsValid(loc.humanoidRootPart->address)
                    && g_Memory.IsValid(cur.humanoidRootPart->address))
                {
                    Vector3 a = Cheat::BasePart(loc.humanoidRootPart->address).GetPosition();
                    Vector3 b = Cheat::BasePart(cur.humanoidRootPart->address).GetPosition();
                    float dx = a.x - b.x;
                    float dy = a.y - b.y;
                    float dz = a.z - b.z;
                    dist = sqrtf(dx * dx + dy * dy + dz * dz);
                }
            }
            char dist_buf[32]{};
            std::snprintf(dist_buf, sizeof(dist_buf), "%.0f studs", dist);

            const char* tool = "-";
            if (have && !cur.toolName.empty())
                tool = cur.toolName.c_str();

            char age_buf[32]{};
            char team_buf[64]{};
            std::snprintf(age_buf, sizeof(age_buf), "-");
            std::snprintf(team_buf, sizeof(team_buf), "-");
            if (have && cur.player_address && g_Memory.IsValid(cur.player_address))
            {
                Cheat::Player pl(cur.player_address);
                std::int32_t age = pl.GetAccountAge();
                std::snprintf(age_buf, sizeof(age_buf), "%d d", (int)age);

                std::uint64_t team = pl.GetTeam();
                if (g_Memory.IsValid(team))
                {
                    std::string tn = Cheat::Instance(team).GetName();
                    if (!tn.empty())
                        std::snprintf(team_buf, sizeof(team_buf), "%s", tn.c_str());
                }
            }

            const char* rig = have ? (cur.isR6 ? "r6" : "r15") : "-";

            float row_y = av_max.y + pad;
            float row_h = line_h + 6.f;
            float col_w = (row_w - 12.f) * 0.5f;

            profile_row(ImVec2(pp.x + pad, row_y + row_h * 0.f), col_w, "status", st);
            profile_row(ImVec2(pp.x + pad + col_w + 12.f, row_y + row_h * 0.f), col_w, "team", team_buf);
            profile_row(ImVec2(pp.x + pad, row_y + row_h * 1.f), col_w, "userid", uid_buf);
            profile_row(ImVec2(pp.x + pad + col_w + 12.f, row_y + row_h * 1.f), col_w, "rig", rig);
            profile_row(ImVec2(pp.x + pad, row_y + row_h * 2.f), col_w, "distance", dist_buf);
            profile_row(ImVec2(pp.x + pad + col_w + 12.f, row_y + row_h * 2.f), col_w, "tool", tool);
            profile_row(ImVec2(pp.x + pad, row_y + row_h * 3.f), col_w, "age", age_buf);
            profile_row(ImVec2(pp.x + pad + col_w + 12.f, row_y + row_h * 3.f), col_w, "user", g_sel_user[0] ? g_sel_user : "-");

            float btn_area_top = row_y + row_h * 4.f + 6.f;
            float btn_w = (row_w - btn_gap) * 0.5f;

            bool is_friend = g_sel && g_friends.count(g_sel);
            bool is_tgt = g_sel && g_target == g_sel;
            bool is_spec = g_sel && g_spectate == g_sel;

            ImVec2 f0(pp.x + pad, btn_area_top);
            ImVec2 f1(f0.x + btn_w, f0.y + btn_h);
            if (action_button("##pl_friend", is_friend ? "unfriend" : "friend", f0, f1) && g_sel)
            {
                if (is_friend)
                    g_friends.erase(g_sel);
                else
                    g_friends.insert(g_sel);
            }

            ImVec2 t0(pp.x + pad + btn_w + btn_gap, btn_area_top);
            ImVec2 t1(t0.x + btn_w, t0.y + btn_h);
            if (action_button("##pl_tgt", is_tgt ? "untarget" : "target", t0, t1) && g_sel)
                g_target = is_tgt ? 0 : g_sel;

            ImVec2 s0(pp.x + pad, btn_area_top + btn_h + btn_gap);
            ImVec2 s1(s0.x + btn_w, s0.y + btn_h);
            if (action_button("##pl_spec", is_spec ? "unspectate" : "spectate", s0, s1) && have)
                do_spectate(cur, !is_spec);

            ImVec2 p0(pp.x + pad + btn_w + btn_gap, btn_area_top + btn_h + btn_gap);
            ImVec2 p1(p0.x + btn_w, p0.y + btn_h);
            if (action_button("##pl_tp", "teleport", p0, p1) && have)
                do_teleport(cur);

            float content_bottom = btn_area_top + btn_block_h + pad;
            fit_h = content_bottom - wp.y + margin;
            if (fit_h < 280.f) fit_h = 280.f;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::EndChild();

        ImGui::End();
    }
}
