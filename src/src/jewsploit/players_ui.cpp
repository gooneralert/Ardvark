#include "players_ui.h"

#include "jewsploit_shell.h"
#include "widgets/float_panel.h"
#include "widgets/child.h"
#include "widgets/btn.h"
#include "colors/colors.h"

#include "app/Settings.h"
#include "core/player/PlayerHandler.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"
#include "features/misc/PlayerAvatars.h"
#include "gui/resources/fonts/fonts.h"

#include <imgui.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
	std::uint64_t g_sel = 0;
	std::int64_t g_sel_uid = 0;
	char g_sel_user[64]{};
	char g_sel_disp[64]{};

	std::unordered_set<std::uint64_t> g_friends;
	std::uint64_t g_target = 0;
	std::uint64_t g_spectate = 0;
	std::uint64_t g_spec_prev_subj = 0;
	std::int32_t g_spec_prev_type = 0;

	struct row_t
	{
		std::uint64_t addr;
		std::int64_t uid;
		std::string user;
		std::string disp;
	};

	const float av_sz = 228.f;

	void set_sel(const row_t& r)
	{
		g_sel = r.addr;
		g_sel_uid = r.uid;
		std::snprintf(g_sel_user, sizeof(g_sel_user), "%s", r.user.c_str());
		std::snprintf(g_sel_disp, sizeof(g_sel_disp), "%s", r.disp.c_str());
	}

	void info_kv(const char* k, const char* v)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.52f, 0.58f, 1.f));
		ImGui::Text("%s", k);
		ImGui::PopStyleColor();
		ImGui::SameLine(0.f, 6.f);
		ImGui::TextUnformatted(v && v[0] ? v : "-");
	}

	void draw_hp_bar(float w, float hp, float max_hp)
	{
		float track_h = 8.f;
		float t = 0.f;
		if (max_hp > 1.f)
		{
			t = hp / max_hp;
		}

		if (t < 0.f) t = 0.f;
		if (t > 1.f) t = 1.f;

		// красный -> зелёный
		int rr = (int)(220.f * (1.f - t) + 60.f * t);
		int gg = (int)(55.f * (1.f - t) + 200.f * t);
		int bb = (int)(55.f * (1.f - t) + 80.f * t);

		ImVec2 p0 = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		float rnd = track_h * 0.5f;

		dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + track_h), IM_COL32(28, 30, 38, 230), rnd);
		dl->AddRect(p0, ImVec2(p0.x + w, p0.y + track_h), IM_COL32(255, 255, 255, 22), rnd, 0, 1.f);

		float fw = w * t;
		if (fw > 0.5f)
		{
			float fr = fw < rnd * 2.f ? fw * 0.5f : rnd;
			dl->AddRectFilled(p0, ImVec2(p0.x + fw, p0.y + track_h), IM_COL32(rr, gg, bb, 240), fr);
		}

		ImGui::Dummy(ImVec2(w, track_h));
	}

	void draw_avatar_slot()
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 p0 = ImGui::GetCursorScreenPos();
		ImVec2 p1(p0.x + av_sz, p0.y + av_sz);

		dl->AddRectFilled(p0, p1, IM_COL32(18, 20, 26, 220), 8.f);
		dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 22), 8.f, 0, 1.f);

		ID3D11ShaderResourceView* srv =
			Cheat::Features::PlayerAvatars::Get(g_sel_uid);
		if (srv)
		{
			dl->AddImageRounded(
				(ImTextureID)(uintptr_t)srv,
				p0, p1,
				ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
				IM_COL32(255, 255, 255, 255),
				8.f
			);
		}

		else
		{
			const char* t = g_sel_disp[0] ? g_sel_disp : "avatar";
			ImVec2 ts = ImGui::CalcTextSize(t);
			dl->AddText(
				ImVec2(p0.x + (av_sz - ts.x) * 0.5f, p0.y + (av_sz - ts.y) * 0.5f),
				IM_COL32(140, 145, 155, 200),
				t
			);
		}

		ImGui::Dummy(ImVec2(av_sz, av_sz));
	}

	const char* status_for(const Cheat::PlayerCache& c)
	{
		if (c.is_corpse)
		{
			return "dead";
		}

		if (g_friends.count(c.address))
		{
			return "friend";
		}

		std::uint64_t local_team = Cheat::PlayerHandler::LocalTeamFolder();
		if (local_team && Cheat::PlayerHandler::IsTeammate(c, local_team))
		{
			return "teammate";
		}

		if (!c.is_player)
		{
			return "bot";
		}

		// враг
		if (local_team && c.team_folder && c.team_folder != local_team)
		{
			return "frag";
		}

		return "neutral";
	}

	void do_teleport(const Cheat::PlayerCache& c)
	{
		if (!c.humanoidRootPart || !Cheat::Globals::Players)
		{
			return;
		}

		std::uint64_t local = g_Memory.Read<std::uint64_t>(
			Cheat::Globals::Players->address + ::Player::LocalPlayer);
		if (!g_Memory.IsValid(local))
		{
			return;
		}

		std::uint64_t lchar = g_Memory.Read<std::uint64_t>(
			local + ::Player::ModelInstance);
		if (!g_Memory.IsValid(lchar))
		{
			return;
		}

		Cheat::Instance ch(lchar);
		auto hrp = ch.FindFirstChild("HumanoidRootPart");
		if (!hrp || !g_Memory.IsValid(hrp->address))
		{
			hrp = ch.FindFirstChild("Torso");
		}

		if (!hrp || !g_Memory.IsValid(hrp->address))
		{
			return;
		}

		Vector3 pos = Cheat::BasePart(c.humanoidRootPart->address).GetPosition();
		pos.y += 3.f;
		Cheat::BasePart(hrp->address).SetPosition(pos);
	}

	void do_spectate(const Cheat::PlayerCache& c, bool on)
	{
		if (!Cheat::Globals::Workspace)
		{
			return;
		}

		auto cam = Cheat::Globals::Workspace->GetCurrentCamera();
		if (!cam || !g_Memory.IsValid(cam->address))
		{
			return;
		}

		if (on)
		{
			if (!c.humanoid || !g_Memory.IsValid(c.humanoid->address))
			{
				return;
			}

			g_spec_prev_subj = g_Memory.Read<std::uint64_t>(
				cam->address + ::Camera::CameraSubject);
			g_spec_prev_type = g_Memory.Read<std::int32_t>(
				cam->address + ::Camera::CameraType);

			g_Memory.Write<std::uint64_t>(
				cam->address + ::Camera::CameraSubject,
				c.humanoid->address);
			// Custom
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

	void draw_list(float w, float h)
	{
		if (ng::child_begin("##pl_list", "players", w, h, 10.f, true))
		{
			std::vector<row_t> rows;
			rows.reserve(Cheat::PlayerHandler::GetPlayerCount());
			Cheat::PlayerHandler::ForEachPlayer([&](const Cheat::PlayerCache& c)
			{
				if (!c.is_player)
				{
					return;
				}

				row_t r{};
				r.addr = c.address;
				r.uid = c.user_id;
				r.user = c.name.empty() ? "unknown" : c.name;
				r.disp = c.displayName.empty() ? r.user : c.displayName;
				rows.push_back(r);
			});

			if (rows.empty())
			{
				ImGui::TextColored(ImVec4(0.55f, 0.57f, 0.60f, 1.f), "no players");
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
				{
					set_sel(rows[0]);
				}

				for (int i = 0; i < (int)rows.size(); ++i)
				{
					const row_t& r = rows[i];
					ImGui::PushID(i);

					bool sel = (r.addr == g_sel);
					const char* lab = r.disp.c_str();
					if (ImGui::Selectable(lab, sel, 0, ImVec2(0.f, 22.f)))
					{
						set_sel(r);
					}

					ImGui::PopID();
				}
			}
		}
		ng::child_end();
	}

	void draw_profile(float w, float h)
	{
		if (!ng::child_begin("##pl_prof", "profile", w, h, 10.f, false))
		{
			ng::child_end();
			return;
		}

		float gap = 8.f;
		float avail_w = ImGui::GetContentRegionAvail().x;
		if (avail_w < 40.f)
		{
			avail_w = 40.f;
		}

		Cheat::PlayerCache cur{};
		bool have = false;
		if (g_sel)
		{
			cur = Cheat::PlayerHandler::GetCachedPlayer(g_sel);
			have = (cur.address == g_sel);
		}

		float info_gap = 12.f;
		float info_w = avail_w - av_sz - info_gap;
		if (info_w < 90.f)
		{
			info_w = 90.f;
		}

		ImGui::BeginGroup();
		draw_avatar_slot();
		ImGui::EndGroup();

		ImGui::SameLine(0.f, info_gap);

		ImGui::BeginGroup();
		{
			const char* disp = g_sel_disp[0] ? g_sel_disp : "-";
			const char* user = g_sel_user[0] ? g_sel_user : "-";

			ImGui::TextUnformatted(disp);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.57f, 0.62f, 1.f));
			ImGui::Text("@%s", user);
			ImGui::PopStyleColor();

			ImGui::Dummy(ImVec2(0.f, 4.f));

			float hp = 0.f;
			float max_hp = 100.f;
			if (have && cur.humanoid && g_Memory.IsValid(cur.humanoid->address))
			{
				Cheat::Humanoid hum(cur.humanoid->address);
				hp = hum.GetHealth();
				max_hp = hum.GetMaxHealth();
				if (max_hp < 1.f)
				{
					max_hp = 100.f;
				}
			}

			draw_hp_bar(info_w, hp, max_hp);

			char hp_txt[48]{};
			std::snprintf(hp_txt, sizeof(hp_txt), "%.0f / %.0f", hp, max_hp);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.57f, 0.62f, 1.f));
			ImGui::TextUnformatted(hp_txt);
			ImGui::PopStyleColor();

			ImGui::Dummy(ImVec2(0.f, 4.f));

			const char* st = have ? status_for(cur) : "-";
			ImU32 st_col = IM_COL32(180, 185, 195, 230);
			if (std::strcmp(st, "friend") == 0 || std::strcmp(st, "teammate") == 0)
			{
				st_col = IM_COL32(90, 200, 120, 240);
			}

			else if (std::strcmp(st, "frag") == 0)
			{
				st_col = IM_COL32(220, 80, 80, 240);
			}

			else if (std::strcmp(st, "dead") == 0)
			{
				st_col = IM_COL32(140, 140, 150, 220);
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 sp = ImGui::GetCursorScreenPos();
			char st_buf[48]{};
			std::snprintf(st_buf, sizeof(st_buf), "status  %s", st);
			dl->AddText(sp, st_col, st_buf);
			ImGui::Dummy(ImVec2(info_w, ImGui::GetTextLineHeight()));

			ImGui::Dummy(ImVec2(0.f, 2.f));

			char uid_buf[32]{};
			std::snprintf(uid_buf, sizeof(uid_buf), "%lld", (long long)g_sel_uid);
			info_kv("userid", uid_buf);

			float dist = 0.f;
			if (have && cur.humanoidRootPart && Cheat::Globals::Players)
			{
				std::uint64_t lp = g_Memory.Read<std::uint64_t>(
					Cheat::Globals::Players->address + ::Player::LocalPlayer);
				Cheat::PlayerCache loc = Cheat::PlayerHandler::GetCachedPlayer(lp);
				if (loc.humanoidRootPart && g_Memory.IsValid(loc.humanoidRootPart->address))
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
			info_kv("distance", dist_buf);

			const char* tool = "-";
			if (have && !cur.toolName.empty())
			{
				tool = cur.toolName.c_str();
			}
			info_kv("tool", tool);

			char age_buf[32]{};
			char team_buf[64]{};
			std::snprintf(age_buf, sizeof(age_buf), "-");
			std::snprintf(team_buf, sizeof(team_buf), "-");
			if (have && g_Memory.IsValid(cur.address))
			{
				Cheat::Player pl(cur.address);
				std::int32_t age = pl.GetAccountAge();
				std::snprintf(age_buf, sizeof(age_buf), "%d d", (int)age);

				std::uint64_t team = pl.GetTeam();
				if (g_Memory.IsValid(team))
				{
					std::string tn = Cheat::Instance(team).GetName();
					if (!tn.empty())
					{
						std::snprintf(team_buf, sizeof(team_buf), "%s", tn.c_str());
					}
				}
			}
			info_kv("age", age_buf);
			info_kv("team", team_buf);

			info_kv("rig", have ? (cur.isR6 ? "r6" : "r15") : "-");

			if (g_target == g_sel && g_sel)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.35f, 0.35f, 1.f));
				ImGui::TextUnformatted("targeted");
				ImGui::PopStyleColor();
			}

			if (g_spectate == g_sel && g_sel)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.f, 1.f));
				ImGui::TextUnformatted("spectating");
				ImGui::PopStyleColor();
			}
		}
		ImGui::EndGroup();

		ImGui::Dummy(ImVec2(0.f, gap));

		float btn_h = 32.f;
		float row_w = ImGui::GetContentRegionAvail().x;
		float bw = (row_w - gap) * 0.5f;
		if (bw < 70.f)
		{
			bw = 70.f;
		}

		bool is_friend = g_sel && g_friends.count(g_sel);
		bool is_tgt = g_sel && g_target == g_sel;
		bool is_spec = g_sel && g_spectate == g_sel;

		if (ng::btn("##pl_friend", is_friend ? "unfriend" : "friend", bw, btn_h))
		{
			if (g_sel)
			{
				if (is_friend)
				{
					g_friends.erase(g_sel);
				}

				else
				{
					g_friends.insert(g_sel);
				}
			}
		}
		ImGui::SameLine(0.f, gap);
		if (ng::btn("##pl_tgt", is_tgt ? "untarget" : "target", bw, btn_h))
		{
			if (g_sel)
			{
				g_target = is_tgt ? 0 : g_sel;
			}
		}

		if (ng::btn("##pl_spec", is_spec ? "unspectate" : "spectate", bw, btn_h))
		{
			if (have)
			{
				do_spectate(cur, !is_spec);
			}
		}
		ImGui::SameLine(0.f, gap);
		if (ng::btn("##pl_tp", "teleport", bw, btn_h))
		{
			if (have)
			{
				do_teleport(cur);
			}
		}

		(void)h;
		ng::child_end();
	}
}

void ng_players::draw(float alpha)
{
	if (alpha <= 0.001f)
	{
		return;
	}

	bool open = menu::open && Cheat::g_Settings.misc.players;

	ImGui::PushFont(fonts::ui(), fonts::ui_size());
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

	bool began = ng::float_panel_begin(
		"##jewsploit_players",
		"players",
		&open,
		700.f, 500.f,
		520.f, 460.f
	);

	if (began)
	{
		ImVec2 wsz = ImGui::GetWindowSize();
		float head = ng::float_panel_head_h();
		float pad = 16.f;
		float gap = 10.f;

		float avail_w = wsz.x - pad * 2.f;
		float avail_h = wsz.y - head - pad - 8.f;
		if (avail_w < 360.f)
		{
			avail_w = 360.f;
		}

		if (avail_h < 240.f)
		{
			avail_h = 240.f;
		}

		// узкий лист, правая шире
		float list_w = 168.f;
		if (list_w > avail_w * 0.28f)
		{
			list_w = avail_w * 0.28f;
		}

		if (list_w < 140.f)
		{
			list_w = 140.f;
		}

		float right_w = avail_w - list_w - gap;
		if (right_w < 320.f)
		{
			right_w = 320.f;
		}

		float x = pad;
		float y = head + 10.f;

		ImGui::SetCursorPos(ImVec2(x, y));
		draw_list(list_w, avail_h);

		ImGui::SetCursorPos(ImVec2(x + list_w + gap, y));
		draw_profile(right_w, avail_h);

		ng::float_panel_end();
	}

	if (!open && menu::open)
	{
		Cheat::g_Settings.misc.players = false;
	}

	ImGui::PopStyleVar();
	ImGui::PopFont();
}
