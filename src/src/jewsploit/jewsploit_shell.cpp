#include "jewsploit_shell.h"
#include "features/games/PhantomForces.h"
#include "animation/animation.h"
#include "colors/colors.h"
#include "widgets/checkbox.h"
#include "widgets/colorpicker.h"
#include "widgets/label_keybind.h"
#include "widgets/label_color.h"
#include "widgets/select.h"
#include "widgets/slider.h"
#include "widgets/color_style.h"
#include "widgets/search_popup.h"
#include "widgets/spacing.h"
#include "widgets/island.h"
#include "tabs/aim.h"
#include "tabs/esp.h"
#include "tabs/misc.h"
#include "tabs/settings_tab.h"
#include "tabs/trigger.h"
#include "app/Settings.h"
#include "features/visuals/ESPPreview.h"
#include "core/globals/Globals.h"
#include "core/roblox/classes/Classes.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <windows.h>
#include <imgui.h>

bool menu::open = false;

static int menu_kb = VK_DELETE;

enum
{
	sf_esp_en = 1,
	sf_esp_preview,
	sf_aim_sticky,
	sf_lua,
	sf_explorer,
	sf_watermark,
	sf_teamcheck,
	sf_fly,
	sf_theme,
	sf_cp_style,
	sf_opacity,
	sf_accent,
	sf_menu_kb
};

static void search_draw_feature(int id)
{
	if (id == sf_esp_en)
	{
		ng::checkbox("esp enabled", &Cheat::g_Settings.esp.enabled);
	}

	else if (id == sf_esp_preview)
	{
		ng::checkbox("esp preview", &Cheat::g_Settings.esp.preview);
	}

	else if (id == sf_aim_sticky)
	{
		ng::checkbox("sticky target", &Cheat::g_Settings.aim.active().sticky);
	}

	else if (id == sf_lua)
	{
		ng::checkbox("lua executor", &Cheat::g_Settings.lua.executor);
	}

	else if (id == sf_explorer)
	{
		ng::checkbox("explorer", &Cheat::g_Settings.misc.explorer);
	}

	else if (id == sf_watermark)
	{
		ng::checkbox("watermark", &Cheat::g_Settings.gui.watermark);
	}

	else if (id == sf_teamcheck)
	{
		ng::checkbox("teamcheck", &Cheat::g_Settings.misc.teamcheck);
	}

	else if (id == sf_fly)
	{
		ng::checkbox("fly", &Cheat::g_Settings.misc.fly);
	}

	else if (id == sf_theme)
	{
		ImGui::Dummy(ImVec2(0.f, 2.f));
		int theme = col::preset_index();
		if (ng::select("##sf_theme", &theme, col::preset_names(), col::preset_count()))
		{
			col::set_preset(theme);
		}
	}

	else if (id == sf_cp_style)
	{
		ImGui::Dummy(ImVec2(0.f, 2.f));
		static const char* cp_items[] = { "simple", "presets" };
		int st = ng::cp_style();
		if (ng::select("##sf_cps", &st, cp_items, 2))
		{
			ng::set_cp_style(st);
		}
	}

	else if (id == sf_opacity)
	{
		ImGui::Dummy(ImVec2(0.f, 2.f));
		float op = col::live().window[3] * 100.f;
		ng::slider("##sf_op", &op, 0.f, 100.f, true);
		float a = op * 0.01f;
		if (a < 0.f) a = 0.f;
		if (a > 1.f) a = 1.f;
		col::live().window[3] = a;
		col::live().child[3] = a * 0.52f;
		float ca = a * 0.88f;
		if (ca > 1.f) ca = 1.f;
		col::live().ctrl[3] = ca;
	}

	else if (id == sf_accent)
	{
		ng::label_color("##sf_acc", "accent", col::live().accent);
	}

	else if (id == sf_menu_kb)
	{
		ng::label_keybind("##sf_mkb", "menu keybind", &menu_kb);
	}
}
static float snap(float v)
{
	return (float)(int)(v + 0.5f);
}

static void draw_text_center(ImDrawList* dl, ImVec2 box0, ImVec2 box1, const char* text, ImU32 col)
{
	ImVec2 ts = ImGui::CalcTextSize(text);
	float x = snap(box0.x + (box1.x - box0.x - ts.x) * 0.5f);
	float y = snap(box0.y + (box1.y - box0.y - ts.y) * 0.5f);
	dl->AddText(ImVec2(x, y), col, text);
}

static void draw_soft_glow(ImVec2 a, ImVec2 b, float rnd)
{
	ImDrawList* back = ImGui::GetBackgroundDrawList();
	// край в window dl — foreground лез через lua/explorer/cp
	ImDrawList* dl = ImGui::GetWindowDrawList();
	float fade = ImGui::GetStyle().Alpha;

	const int layers = 6;
	const float reach = 8.f;

	for (int i = layers; i >= 1; i--)
	{
		float t = (float)i / (float)layers;
		float dist = reach * t;
		float a01 = 1.f - t;
		a01 = a01 * a01;
		int al = (int)(a01 * 16.f * fade);
		if (al < 1) continue;

		back->AddRect(
			ImVec2(a.x - dist, a.y - dist),
			ImVec2(b.x + dist, b.y + dist),
			IM_COL32(190, 205, 235, al),
			rnd + dist * 0.2f,
			0,
			1.1f + t * 1.1f
		);
	}

	int edge_a = (int)(22.f * fade);
	if (edge_a < 1) edge_a = 1;
	dl->AddRect(a, b, IM_COL32(220, 230, 245, edge_a), rnd, 0, 1.f);
}

void menu::init()
{
	ImGui::GetIO().ConfigWindowsResizeFromEdges = false;

	ImGuiStyle& s = ImGui::GetStyle();

	s.WindowRounding = 12.f;
	s.ChildRounding = 10.f;
	s.FrameRounding = 6.f;
	s.PopupRounding = 8.f;
	s.GrabRounding = 6.f;
	s.ScrollbarRounding = 8.f;
	s.TabRounding = 6.f;

	s.WindowBorderSize = 0.f;
	s.ChildBorderSize = 0.f;
	s.FrameBorderSize = 0.f;
	s.PopupBorderSize = 0.f;

	s.WindowPadding = ImVec2(0.f, 0.f);
	s.ItemSpacing = ImVec2(8.f, 6.f);
	s.ItemInnerSpacing = ImVec2(6.f, 4.f);

	ImVec4* c = s.Colors;

	// стекло
	c[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.11f, 0.78f);
	c[ImGuiCol_ChildBg] = col::child_bg(); // эталон child из esp preview
	c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.14f, 0.92f);
	c[ImGuiCol_Border] = ImVec4(1.f, 1.f, 1.f, 0.08f);
	c[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);

	c[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.96f, 1.f);
	c[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.58f, 0.64f, 1.f);

	c[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.13f, 0.16f, 0.70f);
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.17f, 0.21f, 0.80f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.19f, 0.24f, 0.90f);

	c[ImGuiCol_Button] = ImVec4(1.f, 1.f, 1.f, 0.06f);
	c[ImGuiCol_ButtonHovered] = ImVec4(1.f, 1.f, 1.f, 0.12f);
	c[ImGuiCol_ButtonActive] = ImVec4(1.f, 1.f, 1.f, 0.18f);

	// selectable leftover rects - alpha 0, tabs на своём draw
	c[ImGuiCol_Header] = ImVec4(0.f, 0.f, 0.f, 0.f);
	c[ImGuiCol_HeaderHovered] = ImVec4(0.f, 0.f, 0.f, 0.f);
	c[ImGuiCol_HeaderActive] = ImVec4(0.f, 0.f, 0.f, 0.f);

	c[ImGuiCol_CheckMark] = ImVec4(0.75f, 0.82f, 1.f, 1.f);
	c[ImGuiCol_SliderGrab] = ImVec4(0.75f, 0.82f, 1.f, 1.f);
	c[ImGuiCol_SliderGrabActive] = ImVec4(0.85f, 0.90f, 1.f, 1.f);

	c[ImGuiCol_Separator] = ImVec4(1.f, 1.f, 1.f, 0.08f);
	c[ImGuiCol_SeparatorHovered] = ImVec4(0.f, 0.f, 0.f, 0.f);
	c[ImGuiCol_SeparatorActive] = ImVec4(0.f, 0.f, 0.f, 0.f);

	c[ImGuiCol_TitleBg] = c[ImGuiCol_WindowBg];
	c[ImGuiCol_TitleBgActive] = c[ImGuiCol_WindowBg];
	c[ImGuiCol_TitleBgCollapsed] = c[ImGuiCol_WindowBg];

	c[ImGuiCol_ResizeGrip] = ImVec4(0.f, 0.f, 0.f, 0.f);
	c[ImGuiCol_ResizeGripHovered] = ImVec4(0.f, 0.f, 0.f, 0.f);
	c[ImGuiCol_ResizeGripActive] = ImVec4(0.f, 0.f, 0.f, 0.f);

	c[ImGuiCol_ScrollbarBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
	c[ImGuiCol_ScrollbarGrab] = ImVec4(0.f, 0.f, 0.f, 0.f);
	c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.f, 0.f, 0.f, 0.f);
	c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.f, 0.f, 0.f, 0.f);

	s.ScrollbarSize = 0.f;
}

void menu::shutdown()
{
}

void menu::draw()
{
	static bool kb_prev = false;
	static bool kb_skip = false;

	bool kb_wait = ng::label_keybind_take_waiting();
	bool kb_down = menu_kb > 0 && (GetAsyncKeyState(menu_kb) & 0x8000) != 0;

	if (kb_skip || kb_wait)
	{
		if (!kb_down)
		{
			kb_skip = false;
		}
	}

	else if (kb_down && !kb_prev)
	{
		open = !open;
	}

	kb_prev = kb_down;

	ImGuiIO& io = ImGui::GetIO();

	static float vis = 1.f;
	static ImVec2 hold_sz{};
	static ImVec2 hold_pos{};
	static bool search_open = false;

	// закрытие быстрее — ease_out на закрытии тянул слишком долго
	float vis_spd = open ? 13.f : 26.f;
	vis = anim::approach(vis, open ? 1.f : 0.f, vis_spd, io.DeltaTime);
	float e = open ? anim::ease_out_cubic(vis) : vis;

	const bool draw_shell = vis >= 0.001f;
	if (draw_shell)
	{

	float s = 0.94f + 0.06f * e;
	bool ui_live = open && vis > 0.995f && !search_open;

	// дефолт под quad hd формат со скрина (~1/3 ширины, ~половина высоты)
	float w = io.DisplaySize.x * 0.33f;
	float h = io.DisplaySize.y * 0.48f;
	if (w < 640.f) w = 640.f;
	if (h < 420.f) h = 420.f;
	if (w > 920.f) w = 920.f;
	if (h > 740.f) h = 740.f;

	const float prev_w = w * 0.42f; // чуть шире чем раньше
	const float prev_h = h * 0.68f;
	const float dock_gap = 14.f;
	const bool show_prev = Cheat::g_Settings.esp.preview;
	float total_w = w;
	if (show_prev)
	{
		total_w = w + dock_gap + prev_w;
	}

	ImGui::SetNextWindowPos(
		ImVec2((io.DisplaySize.x - total_w) * 0.5f, (io.DisplaySize.y - h) * 0.5f),
		ImGuiCond_FirstUseEver
	);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(480.f, 300.f), ImVec2(FLT_MAX, FLT_MAX));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize;

	if (!ui_live)
	{
		flags |= ImGuiWindowFlags_NoInputs;
	}

	col::push_to_imgui();

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
	ImGui::Begin("##jewsploit_shell", nullptr, flags);

	const float top_h = 52.f;
	const float min_w = 480.f;
	const float min_h = 300.f;
	ImVec2 wsz = ImGui::GetWindowSize();
	ImVec2 wp = ImGui::GetWindowPos();

	static ImVec2 cur{};
	static ImVec2 tgt{};
	static bool inited = false;
	static bool drag_hold = false;
	static int rs = 0;
	static ImVec2 rs_mouse{};
	static ImVec2 rs_pos{};
	static ImVec2 rs_sz{};
	static int tab = 0;

	if (!inited)
	{
		cur = tgt = wp;
		inited = true;
	}

	ImDrawList* dl = ImGui::GetWindowDrawList();
	float rnd = ImGui::GetStyle().WindowRounding;

	const char* tab_names[] = { "aim", "trigger", "esp", "misc", "settings" };
	const int tab_n = 5;
	const float tab_gap = 28.f;
	const float hit_pad_x = 6.f;
	const float hit_h = 28.f;

	static float tab_anim[5]{};

	if (tab < 0) tab = 0;
	if (tab >= tab_n) tab = tab_n - 1;

		// лого слева
		{
			std::string logo_str = "Ardvark";
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
					logo_str += " | " + title;
			}
			const char* logo = logo_str.c_str();
			ImVec2 ts = ImGui::CalcTextSize(logo);
			float lx = wp.x + 18.f;
			float ly = mid_y - ts.y * 0.5f;
			ImGui::SetCursorScreenPos(ImVec2(lx - hit_pad_x, mid_y - hit_h * 0.5f));
			ImGui::InvisibleButton("##logo", ImVec2(ts.x + hit_pad_x * 2.f, hit_h));

			int a = ImGui::IsItemHovered() ? 230 : 170;
			dl->AddText(ImVec2(lx, ly), IM_COL32(245, 248, 255, a), logo);
		}

		// табы по центру
		float widths[5]{};
		float row_w = 0.f;
		for (int i = 0; i < tab_n; i++)
		{
			widths[i] = ImGui::CalcTextSize(tab_names[i]).x;
			row_w += widths[i] + hit_pad_x * 2.f;
			if (i + 1 < tab_n) row_w += tab_gap;
		}

		float row_x = wp.x + (wsz.x - row_w) * 0.5f;
		float x = row_x;

		for (int i = 0; i < tab_n; i++)
		{
			float bw = widths[i] + hit_pad_x * 2.f;
			ImGui::SetCursorScreenPos(ImVec2(x, mid_y - hit_h * 0.5f));

			char id[24]{};
			snprintf(id, sizeof(id), "##tab%d", i);

			if (ImGui::InvisibleButton(id, ImVec2(bw, hit_h)))
			{
				tab = i;
			}

			bool hov = ImGui::IsItemHovered();
			bool sel = (tab == i);

			float want = sel ? 1.f : 0.f;
			tab_anim[i] = anim::approach(tab_anim[i], want, 14.f, io.DeltaTime);
			float t = tab_anim[i];

			int a = 110;
			if (hov) a = 190;
			a = a + (int)((245 - a) * t);

			ImVec2 ts = ImGui::CalcTextSize(tab_names[i]);
			float tx = x + hit_pad_x;
			float ty = mid_y - ts.y * 0.5f - t * 3.f;

			if (t > 0.01f)
			{
				int ga = (int)(36.f * t);
				ImU32 gc = IM_COL32(245, 248, 255, ga);
				float g = 1.15f + t * 0.6f;
				dl->AddText(ImVec2(tx - g, ty), gc, tab_names[i]);
				dl->AddText(ImVec2(tx + g, ty), gc, tab_names[i]);
				dl->AddText(ImVec2(tx, ty - g), gc, tab_names[i]);
				dl->AddText(ImVec2(tx, ty + g), gc, tab_names[i]);

				int ga2 = (int)(16.f * t);
				ImU32 gc2 = IM_COL32(245, 248, 255, ga2);
				float g2 = 2.4f;
				dl->AddText(ImVec2(tx - g2, ty), gc2, tab_names[i]);
				dl->AddText(ImVec2(tx + g2, ty), gc2, tab_names[i]);
				dl->AddText(ImVec2(tx, ty - g2), gc2, tab_names[i]);
				dl->AddText(ImVec2(tx, ty + g2), gc2, tab_names[i]);
			}

			dl->AddText(ImVec2(tx, ty), IM_COL32(245, 248, 255, a), tab_names[i]);

			x += bw + tab_gap;
		}

		// лупа — поиск фич
		{
			float cx = wp.x + wsz.x - 22.f;
			float cy = mid_y;
			ImGui::SetCursorScreenPos(ImVec2(cx - 14.f, cy - 14.f));
			if (ImGui::InvisibleButton("##search", ImVec2(28.f, 28.f)))
			{
				search_open = !search_open;
			}

			bool hov = ImGui::IsItemHovered();
			int a = hov || search_open ? 235 : 140;
			ImU32 col = IM_COL32(245, 248, 255, a);
			float th = hov || search_open ? 1.7f : 1.45f;
			float r = 4.6f;
			ImVec2 cc = ImVec2(cx - 1.2f, cy - 1.2f);
			dl->AddCircle(cc, r, col, 0, th);
			dl->AddLine(
				ImVec2(cc.x + r * 0.72f, cc.y + r * 0.72f),
				ImVec2(cx + 5.2f, cy + 5.2f),
				col,
				th + 0.25f
			);
		}

		dl->AddLine(
			ImVec2(wp.x + 8.f, wp.y + top_h),
			ImVec2(wp.x + wsz.x - 8.f, wp.y + top_h),
			IM_COL32(255, 255, 255, 18),
			1.f
		);
	}

	bool on_nav = ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();
	// cp попап поверх краёв — иначе ресайз/драг ловит клик насквозь
	bool cp_open = ng::colorpicker_any_open();

	ImVec2 m = io.MousePos;
	bool over_top =
		m.x >= wp.x && m.x <= wp.x + wsz.x &&
		m.y >= wp.y && m.y <= wp.y + top_h;

	if (!ui_live || cp_open)
	{
		drag_hold = false;
		rs = 0;
	}

	if (!io.MouseDown[0])
	{
		drag_hold = false;
	}

	else if (ui_live && !cp_open && io.MouseClicked[0] && over_top && !on_nav && rs == 0)
	{
		drag_hold = true;
		tgt = cur = wp;
	}

	bool drag = ui_live && !cp_open && drag_hold && ImGui::IsMouseDragging(ImGuiMouseButton_Left);

	if (drag && rs == 0)
	{
		tgt.x += io.MouseDelta.x;
		tgt.y += io.MouseDelta.y;
	}

	// ресайз без верха
	{
		const float hit = 6.f;
		const float corner = 14.f;
		float x0 = wp.x;
		float y0 = wp.y;
		float x1 = wp.x + wsz.x;
		float y1 = wp.y + wsz.y;

		if (ui_live && !cp_open && rs == 0 && !drag && io.MouseClicked[0])
		{
			bool bl = m.x >= x0 - hit && m.x <= x0 + corner && m.y >= y1 - corner && m.y <= y1 + hit;
			bool br = m.x >= x1 - corner && m.x <= x1 + hit && m.y >= y1 - corner && m.y <= y1 + hit;
			bool l = !bl && m.x >= x0 - hit && m.x <= x0 + hit && m.y > y0 + top_h && m.y <= y1;
			bool r = !br && m.x >= x1 - hit && m.x <= x1 + hit && m.y > y0 + top_h && m.y <= y1;
			bool b = !bl && !br && m.y >= y1 - hit && m.y <= y1 + hit && m.x >= x0 && m.x <= x1;

			if (bl) rs = 4;
			else if (br) rs = 5;
			else if (l) rs = 1;
			else if (r) rs = 2;
			else if (b) rs = 3;

			if (rs)
			{
				rs_mouse = m;
				rs_pos = wp;
				rs_sz = wsz;
			}
		}

		if (rs && io.MouseDown[0])
		{
			ImVec2 d = ImVec2(m.x - rs_mouse.x, m.y - rs_mouse.y);
			ImVec2 np = rs_pos;
			ImVec2 ns = rs_sz;

			if (rs == 1 || rs == 4)
			{
				ns.x = rs_sz.x - d.x;
				np.x = rs_pos.x + d.x;
			}

			if (rs == 2 || rs == 5)
			{
				ns.x = rs_sz.x + d.x;
			}

			if (rs == 3 || rs == 4 || rs == 5)
			{
				ns.y = rs_sz.y + d.y;
			}

			if (ns.x < min_w)
			{
				if (rs == 1 || rs == 4)
				{
					np.x = rs_pos.x + rs_sz.x - min_w;
				}
				ns.x = min_w;
			}

			if (ns.y < min_h)
			{
				ns.y = min_h;
			}

			ImGui::SetWindowSize(ns);
			wsz = ns;
			cur = tgt = np;
			ImGui::SetWindowPos(np);
			wp = np;
		}

		if (rs && !io.MouseDown[0])
		{
			rs = 0;
		}
	}

	if (ui_live && rs == 0)
	{
		cur = anim::approach(cur, tgt, 18.f, io.DeltaTime);
		ImGui::SetWindowPos(cur);
		wp = ImGui::GetWindowPos();
	}

	if (ui_live)
	{
		hold_sz = wsz;
		hold_pos = wp;
	}

	else if (hold_sz.x > 1.f)
	{
		ImVec2 sz = ImVec2(hold_sz.x * s, hold_sz.y * s);
		ImVec2 pos = ImVec2(
			hold_pos.x + (hold_sz.x - sz.x) * 0.5f,
			hold_pos.y + (hold_sz.y - sz.y) * 0.5f
		);
		ImGui::SetWindowSize(sz);
		ImGui::SetWindowPos(pos);
		wsz = sz;
		wp = pos;
		cur = tgt = hold_pos;
	}

	draw_soft_glow(wp, ImVec2(wp.x + wsz.x, wp.y + wsz.y), rnd);

	// контент вкладок — пока пусто
	{
		const float pad = 14.f;
		ImGui::SetCursorPos(ImVec2(pad, top_h + pad));

		char cid[32]{};
		snprintf(cid, sizeof(cid), "##page_%s", tab_names[tab]);

		// page без bg — иначе child темнеет дважды
		ImGui::BeginChild(
			cid,
			ImVec2(wsz.x - pad * 2.f, wsz.y - top_h - pad * 2.f),
			false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
		);

		if (tab == 0)
		{
			ng_tabs::draw_aim_tab();
		}

		else if (tab == 1)
		{
			ng_tabs::draw_trigger_tab();
		}

		else if (tab == 2)
		{
			ng_tabs::draw_esp_tab();
		}

		else if (tab == 3)
		{
			ng_tabs::draw_misc_tab();
		}

		else
		{
			ng_tabs::draw_settings_tab(&menu_kb, &kb_skip);
		}

		ImGui::EndChild();
	}

	ImVec2 main_pos = wp;
	ImVec2 main_sz = wsz;

	ImGui::End();

	// превью рядом с меню, только если тумблер в settings
	if (show_prev)
	{
		float pw = prev_w * s;
		float ph = prev_h * s;
		ImVec2 pp = ImVec2(main_pos.x + main_sz.x + dock_gap, main_pos.y);

		// если справа не влезает — слева
		if (pp.x + pw > io.DisplaySize.x - 8.f)
		{
			pp.x = main_pos.x - dock_gap - pw;
		}

		ImGui::SetNextWindowPos(pp, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(pw, ph), ImGuiCond_Always);

		ImGuiWindowFlags pflags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoFocusOnAppearing;

		if (!ui_live)
		{
			pflags |= ImGuiWindowFlags_NoInputs;
		}

		// тот же WindowBg / Alpha что у основного shell (col::push уже был)
		if (ImGui::Begin("##esp_preview", nullptr, pflags))
		{
			ImVec2 pwp = ImGui::GetWindowPos();
			ImVec2 pwz = ImGui::GetWindowSize();
			float pr = ImGui::GetStyle().WindowRounding;

			draw_soft_glow(pwp, ImVec2(pwp.x + pwz.x, pwp.y + pwz.y), pr);

			ImDrawList* pdl = ImGui::GetWindowDrawList();
			pdl->AddRect(
				pwp,
				ImVec2(pwp.x + pwz.x, pwp.y + pwz.y),
				IM_COL32(255, 255, 255, 12),
				pr,
				0,
				1.f
			);

			const float head_h = 34.f;
			const float line_pad = 12.f;

			draw_text_center(
				pdl,
				pwp,
				ImVec2(pwp.x + pwz.x, pwp.y + head_h),
				"esp preview",
				IM_COL32(230, 235, 245, 200)
			);

			float ly = snap(pwp.y + head_h);
			pdl->AddLine(
				ImVec2(snap(pwp.x + line_pad), ly),
				ImVec2(snap(pwp.x + pwz.x - line_pad), ly),
				IM_COL32(255, 255, 255, 18),
				1.f
			);

			ImGui::SetCursorPos(ImVec2(0.f, head_h));
			ImGui::BeginChild(
				"##esp_prev_body",
				ImVec2(pwz.x, pwz.y - head_h),
				false,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
			);
			Cheat::Visuals::ESPPreview::Render();
			ImGui::EndChild();
		}

		ImGui::End();
	}

	ImGui::PopStyleVar();

	{
		static const ng::search_entry_t search_items[] = {
			{ "esp enabled", sf_esp_en },
			{ "esp preview", sf_esp_preview },
			{ "sticky target", sf_aim_sticky },
			{ "lua executor", sf_lua },
			{ "explorer", sf_explorer },
			{ "watermark", sf_watermark },
			{ "teamcheck", sf_teamcheck },
			{ "fly", sf_fly },
			{ "theme", sf_theme },
			{ "colorpicker", sf_cp_style },
			{ "opacity", sf_opacity },
			{ "accent", sf_accent },
			{ "menu keybind", sf_menu_kb },
		};

		ng::search_popup(
			&search_open,
			search_items,
			(int)(sizeof(search_items) / sizeof(search_items[0])),
			search_draw_feature
		);
	}

	} // draw_shell

	// островок только пока меню открыто
	if (open)
	{
		ng::island();
	}
}
