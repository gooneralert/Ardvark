#include "lua_ui.h"

#include "jewsploit_shell.h"
#include "widgets/float_panel.h"
#include "widgets/child.h"
#include "widgets/btn.h"
#include "colors/colors.h"

#include "features/lua/LuaExecutor.h"
#include "features/lua/State.h"
#include "features/lua/vm/LuaVM.h"
#include "features/lua/execute/Tabs.h"
#include "features/lua/execute/Scripts.h"
#include "features/lua/protocol/Log.h"
#include "features/lua/execute/Clipboard.h"
#include "app/Settings.h"
#include "gui/resources/fonts/fonts.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

using namespace Cheat::Features;
using namespace Cheat::Features::LuaDetail;

namespace
{
	int edit_cb(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			std::string* s = (std::string*)data->UserData;
			s->resize((size_t)data->BufSize);
			data->Buf = s->data();
		}

		return 0;
	}

	void draw_code_box(float w, float h)
	{
		std::string& src = CurText();
		if (src.capacity() < 4096)
			src.reserve(4096);

		// imgui хочет живой буфер с нулём
		if (src.empty())
		{
			src.push_back('\0');
			src.clear();
			src.reserve(4096);
		}

		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		float rnd = 10.f;

		ImU32 bg = col::checkbox_off_u32();
		dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), bg, rnd);
		dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(255, 255, 255, 28), rnd, 0, 1.f);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(220.f / 255.f, 226.f / 255.f, 236.f / 255.f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 10.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rnd);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

		ImGui::SetCursorScreenPos(pos);
		ImGuiInputTextFlags flags =
			ImGuiInputTextFlags_AllowTabInput |
			ImGuiInputTextFlags_CallbackResize;

		ImGui::InputTextMultiline(
			"##lua_code",
			src.data(),
			src.capacity() + 1,
			ImVec2(w, h),
			flags,
			edit_cb,
			&src
		);

		src.resize(std::strlen(src.c_str()));

		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor(4);
	}

	void draw_editor(float w, float h)
	{
		if (!ng::child_begin("##lua_ed", "editor", w, h, 12.f, true))
		{
			ng::child_end();
			return;
		}

		ImVec2 av = ImGui::GetContentRegionAvail();
		float eh = av.y - 10.f;
		if (eh < 80.f) eh = 80.f;
		float ew = av.x;
		if (ew < 40.f) ew = 40.f;

		draw_code_box(ew, eh);
		ng::child_end();
	}

	void draw_scripts(float w, float h)
	{
		if (!ng::child_begin("##lua_scr", "scripts", w, h, 12.f, true))
		{
			ng::child_end();
			return;
		}

		if (g_scripts.empty())
		{
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.f), "no scripts");
		}

		else
		{
			for (int i = 0; i < (int)g_scripts.size(); i++)
			{
				ImGui::PushID(i);
				bool selected = false;
				if (!g_tabs.empty() &&
				    g_tabs[g_tab].path == (ScriptsDir() / g_scripts[i]).string())
				{
					selected = true;
				}

				if (ImGui::Selectable(g_scripts[i].c_str(), selected))
					LoadFileIntoTab(ScriptsDir() / g_scripts[i]);

				ImGui::PopID();
			}
		}

		ImGui::Dummy(ImVec2(0.f, 10.f));
		ng::child_end();
	}

	std::string g_out_view;

	void rebuild_out_view()
	{
		std::vector<OutputLine> snap;
		{
			std::lock_guard<std::mutex> lock(g_output_mu);
			snap = g_output;
		}

		g_out_view.clear();
		if (snap.empty())
		{
			g_out_view = "empty - press execute";
			return;
		}

		g_out_view.reserve(snap.size() * 64);
		for (size_t i = 0; i < snap.size(); ++i)
		{
			const auto& line = snap[i];
			g_out_view += '[';
			g_out_view += LevelTag(line.level);
			g_out_view += "]  ";
			g_out_view += line.text;
			g_out_view += '\n';
		}
	}

	void draw_output(float w, float h)
	{
		if (!ng::child_begin("##lua_out", "output", w, h, 12.f, true))
		{
			ng::child_end();
			return;
		}

		static size_t cached_n = (size_t)-1;
		size_t cur_n = 0;
		{
			std::lock_guard<std::mutex> lock(g_output_mu);
			cur_n = g_output.size();
		}

		if (cached_n != cur_n || g_scroll_out)
		{
			rebuild_out_view();
			cached_n = cur_n;
		}

		{
			const size_t n = g_out_view.size();
			g_out_view.resize(n + 1, '\0');
			g_out_view.resize(n);
		}

		ImVec2 av = ImGui::GetContentRegionAvail();
		float box_h = av.y - 10.f;
		if (box_h < 40.f) box_h = 40.f;
		float box_w = av.x;
		if (box_w < 40.f) box_w = 40.f;

		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		float rnd = 10.f;
		dl->AddRectFilled(pos, ImVec2(pos.x + box_w, pos.y + box_h), col::checkbox_off_u32(), rnd);
		dl->AddRect(pos, ImVec2(pos.x + box_w, pos.y + box_h), IM_COL32(255, 255, 255, 28), rnd, 0, 1.f);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(220.f / 255.f, 226.f / 255.f, 236.f / 255.f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 10.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rnd);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

		ImGui::SetCursorScreenPos(pos);
		ImGuiID id = ImGui::GetID("##lua_out_ro");

		if (g_scroll_out)
		{
			if (ImGuiInputTextState* st = ImGui::GetInputTextState(id))
			{
				if (g_out_auto_scroll)
					st->ReloadUserBufAndMoveToEnd();

				else
					st->ReloadUserBufAndKeepSelection();
			}
		}

		const size_t buf_n = g_out_view.size();
		ImGui::InputTextMultiline(
			"##lua_out_ro",
			g_out_view.data(),
			buf_n + 1,
			ImVec2(box_w, box_h),
			ImGuiInputTextFlags_ReadOnly
		);

		if (g_scroll_out && g_out_auto_scroll)
		{
			if (ImGuiInputTextState* st = ImGui::GetInputTextState(id))
				st->Scroll.y = 1e9f;
			g_scroll_out = false;
		}

		else if (g_scroll_out)
		{
			g_scroll_out = false;
		}

		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor(4);

		ImGui::Dummy(ImVec2(0.f, 12.f));
		ng::child_end();
	}
}

void ng_lua::draw(float alpha)
{
	if (alpha <= 0.001f)
		return;

	// want = меню + тумблер; закрытие анимирует float_panel (флаг не жрём при hide меню)
	bool open = menu::open && Cheat::g_Settings.lua.executor;

	LuaExecutor::Initialize();

	float now = (float)ImGui::GetTime();
	if (now >= g_refresh_at)
	{
		RefreshScripts();
		g_refresh_at = now + 1.5f;
	}

	ImGui::PushFont(fonts::ui(), fonts::ui_size());
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

	bool began = ng::float_panel_begin(
		"##jewsploit_lua_executor",
		"lua executor",
		&open,
		900.f, 600.f,
		640.f, 420.f
	);

	if (began)
	{
		ImVec2 wsz = ImGui::GetWindowSize();
		float head = ng::float_panel_head_h();
		float pad = 16.f;
		float gap = 12.f;
		float btn_h = 34.f;

		float avail_w = wsz.x - pad * 2.f;
		float avail_h = wsz.y - head - pad - 12.f;
		if (avail_w < 360.f) avail_w = 360.f;
		if (avail_h < 260.f) avail_h = 260.f;

		float body_h = avail_h - btn_h - gap;
		if (body_h < 180.f) body_h = 180.f;

		float side_w = 200.f;
		float left_w = avail_w - side_w - gap;
		if (left_w < 240.f) left_w = 240.f;

		// больше editor, output компактнее
		float ed_h = body_h * 0.72f;
		float out_h = body_h - ed_h - gap;
		if (ed_h < 160.f) ed_h = 160.f;
		if (out_h < 80.f)
		{
			out_h = 80.f;
			ed_h = body_h - out_h - gap;
		}

		float x = pad;
		float y = head + 12.f;

		ImGui::SetCursorPos(ImVec2(x, y));
		draw_editor(left_w, ed_h);

		ImGui::SetCursorPos(ImVec2(x + left_w + gap, y));
		draw_scripts(side_w, body_h);

		ImGui::SetCursorPos(ImVec2(x, y + ed_h + gap));
		draw_output(left_w, out_h);

		float by = y + ed_h + gap + out_h + gap;
		float bw = (avail_w - gap * 3.f) / 4.f;
		if (bw < 80.f) bw = 80.f;

		ImGui::SetCursorPos(ImVec2(x, by));
		if (ng::btn("##exec", "execute", bw, btn_h))
		{
			LuaExecutor::Log(LogLevel::Info, "running...");
			LuaVM::Execute(CurText(), Cur().name.c_str());
		}
		ImGui::SameLine(0.f, gap);
		if (ng::btn("##clr", "clear", bw, btn_h))
		{
			ClearCurrent();
			LuaExecutor::Log(LogLevel::Info, "editor cleared");
		}
		ImGui::SameLine(0.f, gap);
		if (ng::btn("##copy_out", "copy out", bw, btn_h))
		{
			if (g_out_view.empty())
				SetClipboardText("");

			else
				SetClipboardText(g_out_view);
		}
		ImGui::SameLine(0.f, gap);
		if (ng::btn("##clr_out", "clear output", bw, btn_h))
		{
			{
				std::lock_guard<std::mutex> lock(g_output_mu);
				g_output.clear();
			}
			g_out_sel = -1;
			g_out_sel_end = -1;
		}

		ng::float_panel_end();
	}

	// крест — гасим тумблер; закрытие меню флаг не трогает
	if (!open && menu::open)
		Cheat::g_Settings.lua.executor = false;

	ImGui::PopStyleVar();
	ImGui::PopFont();
}
