#include "pch.h"
#define NOMINMAX
#include "widgets.h"

#include "jewsploit/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include "imgui_internal.h"
#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"

#undef GetClassName

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace
{
	ImVec2 g_wm_size{};
	bool g_wm_dragging = false;

	ImU32 with_a(ImU32 c, float a)
	{
		if (a < 0.f) a = 0.f;
		if (a > 1.f) a = 1.f;
		ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
		v.w *= a;
		return ImGui::ColorConvertFloat4ToU32(v);
	}

	float snap(float v)
	{
		return (float)(int)(v + 0.5f);
	}

	std::string local_name()
	{
		if (!Cheat::Globals::Players || !Cheat::Globals::Players->address)
			return {};

		const auto lp = g_Memory.Read<std::uint64_t>(
			Cheat::Globals::Players->address + ::Player::LocalPlayer);
		if (!lp || !g_Memory.IsValid(lp))
			return {};

		Cheat::Player pl(lp);
		std::string dn = pl.GetDisplayName();
		if (!dn.empty() && dn != "Unknown")
			return dn;
		return pl.GetName();
	}
}

namespace widgets
{
	bool watermark_hit_test(float x, float y)
	{
		if (!Cheat::g_Settings.gui.watermark)
			return false;
		if (g_wm_dragging)
			return true;
		if (g_wm_size.x <= 0.f || g_wm_size.y <= 0.f)
			return false;

		float wx = Cheat::g_Settings.gui.watermark_x;
		float wy = Cheat::g_Settings.gui.watermark_y;
		return x >= wx && y >= wy && x < wx + g_wm_size.x && y < wy + g_wm_size.y;
	}

	void watermark(float alpha)
	{
		if (alpha < 0.f) alpha = 0.f;
		if (alpha > 1.f) alpha = 1.f;
		if (alpha <= 0.001f)
		{
			g_wm_dragging = false;
			return;
		}

		auto& gui = Cheat::g_Settings.gui;
		ImGuiIO& io = ImGui::GetIO();
		ImDrawList* dl = ImGui::GetForegroundDrawList();

		ImFont* font = fonts::ui();
		float fs = fonts::ui_size(font);
		ImGui::PushFont(font, fs);

		std::vector<std::string> segs;
		segs.reserve(Cheat::Settings::WM_FIELD_COUNT);

		if (gui.watermark_fields[Cheat::Settings::WM_BUILD])
		{
			char buf[64];
			std::snprintf(buf, sizeof(buf), "build  %s", __DATE__);
			segs.emplace_back(buf);
		}

		if (gui.watermark_fields[Cheat::Settings::WM_PLAYER])
		{
			std::string nm = local_name();
			segs.emplace_back(nm.empty() ? "user  -" : ("user  " + nm));
		}

		if (gui.watermark_fields[Cheat::Settings::WM_PLACE_ID])
		{
			char buf[64];
			unsigned long long place = 0;
			if (Cheat::Globals::InstanceDataModel.address)
				place = (unsigned long long)Cheat::Globals::InstanceDataModel.GetPlaceId();
			std::snprintf(buf, sizeof(buf), "place  %llu", place);
			segs.emplace_back(buf);
		}

		if (gui.watermark_fields[Cheat::Settings::WM_GAME_ID])
		{
			char buf[64];
			unsigned long long game = 0;
			if (Cheat::Globals::InstanceDataModel.address)
				game = (unsigned long long)Cheat::Globals::InstanceDataModel.GetGameId();
			std::snprintf(buf, sizeof(buf), "game  %llu", game);
			segs.emplace_back(buf);
		}

		if (gui.watermark_fields[Cheat::Settings::WM_TIME])
		{
			char buf[32];
			std::time_t now = std::time(nullptr);
			std::tm local{};
			localtime_s(&local, &now);
			std::strftime(buf, sizeof(buf), "%H:%M:%S", &local);
			segs.emplace_back(std::string("time  ") + buf);
		}

		if (gui.watermark_fields[Cheat::Settings::WM_FPS])
		{
			char buf[32];
			std::snprintf(buf, sizeof(buf), "fps  %.0f", io.Framerate);
			segs.emplace_back(buf);
		}

		std::string brand_str = "jewsploit";
		if (Cheat::Globals::InstanceDataModel.address)
		{
			auto pid = Cheat::Globals::InstanceDataModel.GetPlaceId();
			std::string title;
			if (pid == 292439477ull || pid == 286090429ull)
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
				brand_str += " | " + title;
		}
		const char* brand = brand_str.c_str();
		ImVec2 brand_ts = ImGui::CalcTextSize(brand);
		float text_h = brand_ts.y;

		float pad_x = 14.f;
		float pad_y = 9.f;
		float gap = 12.f;
		float sep_w = 1.f;

		float content_w = brand_ts.x;
		for (const auto& s : segs)
		{
			content_w += gap + sep_w + gap;
			content_w += ImGui::CalcTextSize(s.c_str()).x;
		}

		float total_w = pad_x * 2.f + content_w;
		float total_h = pad_y * 2.f + text_h;
		g_wm_size = ImVec2(total_w, total_h);

		float max_x = io.DisplaySize.x - total_w;
		float max_y = io.DisplaySize.y - total_h;
		if (max_x < 0.f) max_x = 0.f;
		if (max_y < 0.f) max_y = 0.f;
		if (gui.watermark_x < 0.f) gui.watermark_x = 0.f;
		if (gui.watermark_x > max_x) gui.watermark_x = max_x;
		if (gui.watermark_y < 0.f) gui.watermark_y = 0.f;
		if (gui.watermark_y > max_y) gui.watermark_y = max_y;

		{
			static ImVec2 grab{};

			ImGui::SetNextWindowPos(ImVec2(gui.watermark_x, gui.watermark_y), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(total_w, total_h), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);

			if (ImGui::Begin(
				"##jewsploit_watermark_drag",
				nullptr,
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoNav |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoBackground))
			{
				ImGui::InvisibleButton("##wm_drag", ImVec2(total_w, total_h));

				if (ImGui::IsItemActivated())
				{
					g_wm_dragging = true;
					grab = io.MousePos - ImVec2(gui.watermark_x, gui.watermark_y);
				}

				if (g_wm_dragging)
				{
					if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
					{
						gui.watermark_x = io.MousePos.x - grab.x;
						gui.watermark_y = io.MousePos.y - grab.y;
						if (gui.watermark_x < 0.f) gui.watermark_x = 0.f;
						if (gui.watermark_x > max_x) gui.watermark_x = max_x;
						if (gui.watermark_y < 0.f) gui.watermark_y = 0.f;
						if (gui.watermark_y > max_y) gui.watermark_y = max_y;
						ImGui::SetWindowPos(ImVec2(gui.watermark_x, gui.watermark_y), ImGuiCond_Always);
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
						ImGui::SetNextFrameWantCaptureMouse(true);
					}

					else
					{
						g_wm_dragging = false;
					}
				}

				else if (ImGui::IsItemHovered())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				}
			}
			ImGui::End();
			ImGui::PopStyleVar(3);
		}

		ImVec2 o(gui.watermark_x, gui.watermark_y);
		ImVec2 b(o.x + total_w, o.y + total_h);
		float rnd = 12.f;

		col::theme_t& th = col::live();
		ImU32 bg = ImGui::ColorConvertFloat4ToU32(
			ImVec4(th.window[0], th.window[1], th.window[2], th.window[3] * alpha)
		);
		// чуть плотнее окна чтоб читалось
		{
			ImVec4 v = ImGui::ColorConvertU32ToFloat4(bg);
			if (v.w < 0.55f) v.w = 0.55f * alpha;
			if (v.w > 0.92f) v.w = 0.92f * alpha;
			bg = ImGui::ColorConvertFloat4ToU32(v);
		}

		dl->AddRectFilled(o, b, bg, rnd);
		dl->AddRect(o, b, with_a(IM_COL32(255, 255, 255, 14), alpha), rnd, 0, 1.f);

		float ty = snap(o.y + (total_h - text_h) * 0.5f);
		float x = o.x + pad_x;

		dl->AddText(
			ImVec2(snap(x), ty),
			with_a(col::accent_u32(255), alpha),
			brand
		);
		x += brand_ts.x;

		ImU32 muted = with_a(IM_COL32(160, 166, 178, 255), alpha);
		ImU32 sep_c = with_a(IM_COL32(255, 255, 255, 28), alpha);

		for (const auto& s : segs)
		{
			x += gap;
			float sy0 = o.y + pad_y + 2.f;
			float sy1 = o.y + total_h - pad_y - 2.f;
			dl->AddLine(ImVec2(snap(x), sy0), ImVec2(snap(x), sy1), sep_c, 1.f);
			x += sep_w + gap;

			dl->AddText(ImVec2(snap(x), ty), muted, s.c_str());
			x += ImGui::CalcTextSize(s.c_str()).x;
		}

		ImGui::PopFont();
	}
}
