#include "pch.h"
#include "settings_tab.h"
#include "helpers.h"

#include "imgui.h"
#include "widgets/widgets.h"
#include "app/Settings.h"
#include "core/config/Config.h"
#include "features/mcp/McpBridge.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

void ng_tabs::draw_settings_tab(int* menu_kb, bool* kb_skip)
{
	using namespace Cheat;
	using namespace ng_tabs;

	float left_w = 0.f, right_w = 0.f, h = 0.f;
	begin_columns(&left_w, &right_w, &h);

	static const std::vector<const char*> wm_items = {
		"build", "player", "place id", "game id", "time", "fps"
	};

	begin_panel("##set_menu", left_w, h);
	{
		if (menu_kb)
		{
			int before = *menu_kb;
			row_keybind_simple("##menu_kb", "menu key", menu_kb);
			if (kb_skip && *menu_kb != before)
				*kb_skip = true;
		}

		g_Settings.esp.preview = false;

		static const std::vector<const char*> gui_fonts = {
			"fredoka one", "tahoma bold", "proggy clean", "visitor", "verdana", "segoe ui", "imgui default"
		};
		row_combo("gui font", &g_Settings.gui.font, gui_fonts);

		row_checkbox("watermark", &g_Settings.gui.watermark);
		if (g_Settings.gui.watermark)
		{
			row_multicombo(
				"##wm_fields",
				"watermark info",
				g_Settings.gui.watermark_fields,
				wm_items
			);
		}

		row_checkbox("raycast engine", &g_Settings.misc.raycast_engine);

		{
			bool prev = g_Settings.misc.mcp;
			row_checkbox("mcp", &g_Settings.misc.mcp);
			if (g_Settings.misc.mcp != prev)
			{
				if (g_Settings.misc.mcp)
					Features::McpBridge::Start();
				else
					Features::McpBridge::Stop();
			}
		}

		row_checkbox("custom support", &g_Settings.misc.custom_support);
	}
	end_panel();

	ImGui::SameLine(0.f, panel_gap);

	begin_panel("##set_cfgs", right_w, h);
	{
		static char s_cfg_name[64] = "default";
		static int s_cfg_sel = -1;
		static char s_cfg_items[64][128]{};
		static int s_cfg_count = 0;
		static float s_cfg_refresh_at = 0.f;

		const float now = (float)ImGui::GetTime();
		if (now >= s_cfg_refresh_at)
		{
			std::vector<std::string> list = Config::List();
			s_cfg_count = 0;
			for (int i = 0; i < (int)list.size() && s_cfg_count < 64; i++)
			{
				snprintf(s_cfg_items[s_cfg_count], 128, "%s", list[i].c_str());
				s_cfg_count++;
			}
			s_cfg_refresh_at = now + 1.f;
			if (s_cfg_sel >= s_cfg_count)
				s_cfg_sel = s_cfg_count - 1;
		}

		pad();
		widgets::input_text("##cfg_name", s_cfg_name, (int)sizeof(s_cfg_name));

		pad();
		{
			float content_w = ImGui::CalcItemWidth();
			float bw = (content_w - content_spacing * 2.f) / 3.f;
			if (bw < 40.f)
				bw = 40.f;

			ImGui::PushItemWidth(bw);
			bool load = widgets::button("##load");
			ImGui::SameLine(0.f, content_spacing);
			bool del = widgets::button("##delete");
			ImGui::SameLine(0.f, content_spacing);
			bool save = widgets::button("##save");
			ImGui::PopItemWidth();

			const char* name = s_cfg_name;
			if (s_cfg_sel >= 0 && s_cfg_sel < s_cfg_count)
				name = s_cfg_items[s_cfg_sel];

			if (load && name && name[0] && Config::Load(name))
			{
				snprintf(s_cfg_name, sizeof(s_cfg_name), "%s", name);
			}
			else if (del && name && name[0] && Config::Remove(name))
			{
				std::vector<std::string> list = Config::List();
				s_cfg_count = 0;
				for (int i = 0; i < (int)list.size() && s_cfg_count < 64; i++)
				{
					snprintf(s_cfg_items[s_cfg_count], 128, "%s", list[i].c_str());
					s_cfg_count++;
				}
				s_cfg_sel = -1;
				s_cfg_refresh_at = now + 1.f;
			}
			else if (save && name && name[0] && Config::Save(name))
			{
				snprintf(s_cfg_name, sizeof(s_cfg_name), "%s", name);
				std::vector<std::string> list = Config::List();
				s_cfg_count = 0;
				for (int i = 0; i < (int)list.size() && s_cfg_count < 64; i++)
				{
					snprintf(s_cfg_items[s_cfg_count], 128, "%s", list[i].c_str());
					s_cfg_count++;
				}
				s_cfg_refresh_at = now + 1.f;
				s_cfg_sel = -1;
				for (int i = 0; i < s_cfg_count; i++)
				{
					if (strcmp(s_cfg_items[i], name) == 0)
					{
						s_cfg_sel = i;
						break;
					}
				}
			}
		}

		pad();
		if (ImGui::BeginListBox("##cfg_list", ImVec2(-1.f, ImGui::GetTextLineHeightWithSpacing() * 8.f)))
		{
			for (int i = 0; i < s_cfg_count; i++)
			{
				bool sel = (s_cfg_sel == i);
				if (ImGui::Selectable(s_cfg_items[i], sel))
				{
					s_cfg_sel = i;
					snprintf(s_cfg_name, sizeof(s_cfg_name), "%s", s_cfg_items[i]);
				}
			}
			ImGui::EndListBox();
		}
	}
	end_panel();
}
