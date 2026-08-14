#include "settings_tab.h"
#include "helpers.h"

#include "../colors/colors.h"
#include "../widgets/btn_row.h"
#include "../widgets/checkbox.h"
#include "../widgets/child.h"
#include "../widgets/color_style.h"
#include "../widgets/input.h"
#include "../widgets/label_color.h"
#include "../widgets/label_keybind.h"
#include "../widgets/select.h"
#include "../widgets/slider.h"
#include "../widgets/spacing.h"
#include "../widgets/strlist.h"

#include "app/Settings.h"
#include "core/config/Config.h"
#include "features/mcp/McpBridge.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

void ng_tabs::draw_settings_tab(int* menu_kb, bool* kb_skip)
{
	if (!menu_kb || !kb_skip)
	{
		return;
	}

	float col_gap = 10.f;
	float avail_w = ImGui::GetContentRegionAvail().x;
	float avail_h = ImGui::GetContentRegionAvail().y;
	float cw = (avail_w - col_gap) * 0.5f;

	ng::child_begin("##set_child1", "menu", cw, avail_h, 10.f, true);

	ng_tabs::pad();
	ImGui::Dummy(ImVec2(0.f, 2.f));

	ng_tabs::pad();
	if (ng::label_keybind("##menu_kb", "menu key", menu_kb))
	{
		*kb_skip = true;
	}

	ng_tabs::gap();
	ng_tabs::pad();
	ng::checkbox("esp preview", &Cheat::g_Settings.esp.preview);

	ng_tabs::gap();
	ng_tabs::pad();
	ng::checkbox("watermark", &Cheat::g_Settings.gui.watermark);

	if (Cheat::g_Settings.gui.watermark)
	{
		static const char* wm_items[] = {
			"build", "player", "place id", "game id", "time", "fps"
		};
		ng_tabs::row_dropdown(
			"##wm_fields",
			"watermark info",
			Cheat::g_Settings.gui.watermark_fields,
			wm_items,
			Cheat::Settings::WM_FIELD_COUNT,
			true
		);
	}

	ng_tabs::gap();
	ng_tabs::pad();
	ng::checkbox("raycast engine", &Cheat::g_Settings.misc.raycast_engine);

	ng_tabs::gap();
	ng_tabs::pad();
	{
		bool prev = Cheat::g_Settings.misc.mcp;
		ng::checkbox("mcp", &Cheat::g_Settings.misc.mcp);
		if (Cheat::g_Settings.misc.mcp != prev)
		{
			if (Cheat::g_Settings.misc.mcp)
			{
				Cheat::Features::McpBridge::Start();
			}

			else
			{
				Cheat::Features::McpBridge::Stop();
			}
		}
	}

	ng_tabs::gap();
	ng_tabs::pad();
	ng::checkbox("custom support", &Cheat::g_Settings.misc.custom_support);

	ng_tabs::gap();
	{
		static const char* font_items[] = {
			"fredoka one", "tahoma bold", "proggy clean", "visitor", "verdana", "imgui"
		};
		ng_tabs::row_select("##gui_font", "font", &Cheat::g_Settings.gui.font, font_items, 6);
	}

	ng_tabs::gap();
	{
		int theme = col::preset_index();
		if (ng_tabs::row_select("##theme", "theme", &theme, col::preset_names(), col::preset_count()))
		{
			col::set_preset(theme);
		}
	}

	ng_tabs::gap();
	{
		static const char* cp_items[] = { "simple", "presets" };
		int st = ng::cp_style();
		if (ng_tabs::row_select("##cp_style", "colorpicker", &st, cp_items, 2))
		{
			ng::set_cp_style(st);
		}
	}

	ng_tabs::gap();
	{
		float op = col::live().window[3] * 100.f;
		ng_tabs::row_slider("##gui_op", "opacity", &op, 0.f, 100.f, true);
		float a = op * 0.01f;
		if (a < 0.f) a = 0.f;
		if (a > 1.f) a = 1.f;
		col::live().window[3] = a;
		// чайлды стекляннее окна а то бетон
		col::live().child[3] = a * 0.52f;
		float ca = a * 0.88f;
		if (ca > 1.f) ca = 1.f;
		col::live().ctrl[3] = ca;
	}

	ng_tabs::gap();
	ng_tabs::pad();
	ng::label_color("##acc", "accent", col::live().accent);

	ng_tabs::gap();
	ng_tabs::pad();
	ng::label_color("##childc", "child", col::live().child);

	ng_tabs::gap();
	ng_tabs::pad();
	ng::label_color("##ctrl", "controls", col::live().ctrl);

	ng_tabs::gap();
	ng_tabs::pad();
	ng::label_color("##win", "window", col::live().window);

	ng::child_end();

	ImGui::SameLine(0.f, col_gap);

	ng::child_begin("##set_child2", "configs", cw, avail_h, 10.f, true);

	static char s_cfg_name[64] = "default";
	static int s_cfg_sel = -1;
	static char s_cfg_items[64][128]{};
	static int s_cfg_count = 0;
	static float s_cfg_refresh_at = 0.f;

	const float now = (float)ImGui::GetTime();
	if (now >= s_cfg_refresh_at)
	{
		std::vector<std::string> list = Cheat::Config::List();
		s_cfg_count = 0;
		for (int i = 0; i < (int)list.size() && s_cfg_count < 64; i++)
		{
			snprintf(s_cfg_items[s_cfg_count], 128, "%s", list[i].c_str());
			s_cfg_count++;
		}
		s_cfg_refresh_at = now + 1.0f;
		if (s_cfg_sel >= s_cfg_count)
		{
			s_cfg_sel = s_cfg_count - 1;
		}
	}

	ng_tabs::pad();
	ImGui::Dummy(ImVec2(0.f, 2.f));

	ng_tabs::pad();
	ng::input("##cfg_name", s_cfg_name, (int)sizeof(s_cfg_name));

	ng_tabs::gap();
	ng_tabs::pad();

	static const char* cfg_btns[] = { "load", "delete", "save" };
	int cfg_hit = ng::btn_row("##cfg_btns", cfg_btns, 3);

	if (cfg_hit == 0)
	{
		const char* name = s_cfg_name;
		if (s_cfg_sel >= 0 && s_cfg_sel < s_cfg_count)
		{
			name = s_cfg_items[s_cfg_sel];
		}

		if (name && name[0] && Cheat::Config::Load(name))
		{
			snprintf(s_cfg_name, sizeof(s_cfg_name), "%s", name);
			col::push_to_imgui();
		}
	}

	else if (cfg_hit == 1)
	{
		const char* name = s_cfg_name;
		if (s_cfg_sel >= 0 && s_cfg_sel < s_cfg_count)
		{
			name = s_cfg_items[s_cfg_sel];
		}

		if (name && name[0] && Cheat::Config::Remove(name))
		{
			std::vector<std::string> list = Cheat::Config::List();
			s_cfg_count = 0;
			for (int i = 0; i < (int)list.size() && s_cfg_count < 64; i++)
			{
				snprintf(s_cfg_items[s_cfg_count], 128, "%s", list[i].c_str());
				s_cfg_count++;
			}
			s_cfg_sel = -1;
			s_cfg_refresh_at = now + 1.0f;
		}
	}

	else if (cfg_hit == 2)
	{
		const char* name = s_cfg_name;
		if (s_cfg_sel >= 0 && s_cfg_sel < s_cfg_count)
		{
			name = s_cfg_items[s_cfg_sel];
		}

		if (name && name[0] && Cheat::Config::Save(name))
		{
			snprintf(s_cfg_name, sizeof(s_cfg_name), "%s", name);
			std::vector<std::string> list = Cheat::Config::List();
			s_cfg_count = 0;
			for (int i = 0; i < (int)list.size() && s_cfg_count < 64; i++)
			{
				snprintf(s_cfg_items[s_cfg_count], 128, "%s", list[i].c_str());
				s_cfg_count++;
			}
			s_cfg_refresh_at = now + 1.0f;
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

	ng_tabs::gap();
	ng_tabs::pad();

	float pad_r = 12.f;
	float list_w = ImGui::GetContentRegionAvail().x - pad_r;
	if (list_w < 40.f)
	{
		list_w = 40.f;
	}

	if (ng::strlist("##cfg_list", "saved", s_cfg_items, s_cfg_count, &s_cfg_sel, list_w, 5))
	{
		if (s_cfg_sel >= 0 && s_cfg_sel < s_cfg_count)
		{
			snprintf(s_cfg_name, sizeof(s_cfg_name), "%s", s_cfg_items[s_cfg_sel]);
		}
	}

	ng::child_end();
}
