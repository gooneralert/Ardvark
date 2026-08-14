#include "explorer_ui.h"

#include "jewsploit_shell.h"
#include "animation/animation.h"
#include "colors/colors.h"
#include "widgets/float_panel.h"
#include "widgets/child.h"
#include "widgets/input.h"
#include "widgets/btn.h"

#include "app/Settings.h"
#include "app/Graphics.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "features/lua/vm/ScriptBytecode.h"
#include "gui/resources/fonts/fonts.h"
#include "gui/resources/icons/iconsdex.h"

#include <stb_image.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <Windows.h>

#undef GetClassName

namespace
{
#include "features/explorer/ExplorerDraw.h"
#include "features/explorer/ExplorerTree.h"
#include "features/explorer/ExplorerSearch.h"

	float snap(float v)
	{
		return (float)(int)(v + 0.5f);
	}

	void line(ImU32 col, const char* fmt, ...)
	{
		char buf[512];
		va_list args;
		va_start(args, fmt);
		std::vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);
		ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
		ImGui::TextColored(c, "%s", buf);
	}

	void kv(const char* k, const char* v, bool copy_click = false)
	{
		float x0 = ImGui::GetCursorPosX();
		ImGui::TextColored(ImVec4(0.52f, 0.55f, 0.62f, 1.f), "%s", k);
		ImGui::SameLine(x0 + 78.f, 0.f);
		float wrap = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
		ImGui::PushTextWrapPos(wrap);

		const char* txt = v ? v : "?";
		if (copy_click)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.82f, 0.92f, 1.f));
			ImGui::TextUnformatted(txt);
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsItemClicked())
				ImGui::SetClipboardText(txt);
		}

		else
		{
			ImGui::TextUnformatted(txt);
		}

		ImGui::PopTextWrapPos();
		ImGui::Dummy(ImVec2(0.f, 2.f));
	}

	void open_ctx_for(std::uint64_t addr, const std::string& name, const std::string& cls, const std::string& path)
	{
		g_ctx.address = addr;
		g_ctx.name = name;
		g_ctx.cls = cls;
		g_ctx.path = path;
		g_ctx.valid = true;
		g_open_ctx = true;
	}

	struct row_hit_t
	{
		bool lmb;
		bool rmb;
	};

	row_hit_t draw_row(const char* label, ID3D11ShaderResourceView* srv, float indent, bool arrow, bool open, float* hov)
	{
		float row_h = 24.f;
		float icon_sz = 14.f;
		ImVec2 p0 = ImGui::GetCursorScreenPos();

		ImVec4 clear(0.f, 0.f, 0.f, 0.f);
		ImGui::PushStyleColor(ImGuiCol_Header, clear);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, clear);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, clear);
		ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.f, row_h));
		ImGui::PopStyleColor(3);

		bool hov_now = ImGui::IsItemHovered();
		row_hit_t hit{};
		hit.lmb = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		hit.rmb = ImGui::IsItemClicked(ImGuiMouseButton_Right);
		*hov = anim::approach(*hov, hov_now ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		float side = 6.f;
		float rw = ImGui::GetWindowPos().x + ImGui::GetWindowSize().x - p0.x - side;
		if (rw < 40.f) rw = 40.f;

		if (*hov > 0.01f)
		{
			dl->AddRectFilled(
				ImVec2(p0.x + side, p0.y + 1.f),
				ImVec2(p0.x + rw, p0.y + row_h - 1.f),
				col::accent_u32((int)(36.f * (*hov))),
				8.f
			);
		}

		float x = p0.x + indent + 10.f;
		float cy = p0.y + row_h * 0.5f;

		if (arrow)
		{
			ImU32 ac = IM_COL32(160, 165, 175, 220);
			if (open)
			{
				dl->AddTriangleFilled(
					ImVec2(x, cy - 2.5f),
					ImVec2(x + 7.f, cy - 2.5f),
					ImVec2(x + 3.5f, cy + 3.f),
					ac
				);
			}

			else
			{
				dl->AddTriangleFilled(
					ImVec2(x + 1.f, cy - 4.f),
					ImVec2(x + 6.5f, cy),
					ImVec2(x + 1.f, cy + 4.f),
					ac
				);
			}
		}
		x += 14.f;

		if (srv)
		{
			ImVec2 a(x, cy - icon_sz * 0.5f);
			dl->AddImage((ImTextureID)(uintptr_t)srv, a, ImVec2(a.x + icon_sz, a.y + icon_sz));
		}
		x += icon_sz + 8.f;

		float fs = ImGui::GetFontSize();
		int ta = 200 + (int)(40.f * (*hov));
		if (ta > 255) ta = 255;
		dl->AddText(
			ImVec2(snap(x), snap(cy - fs * 0.5f)),
			IM_COL32(230, 235, 245, ta),
			label ? label : "?"
		);

		ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + row_h));
		return hit;
	}

	void draw_node(Node& n, int depth)
	{
		ImGui::PushID((void*)(uintptr_t)n.address);

		bool arrow = (!n.loaded) || (!n.children.empty());
		float indent = depth * 16.f + 4.f;
		float* ht = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hov"), 0.f);

		int iw = 16, ih = 16;
		ID3D11ShaderResourceView* srv = IconSrv(n.cls, iw, ih);
		const char* label = n.name.empty() ? "?" : n.name.c_str();
		row_hit_t hit = draw_row(label, srv, indent, arrow, n.open, ht);

		if (hit.lmb && arrow)
		{
			n.open = !n.open;
			if (n.open)
				LoadChildren(n);
		}

		if (hit.rmb)
		{
			open_ctx_for(n.address, n.name, n.cls, BuildPath(n.address));
		}

		if (n.open && n.loaded)
		{
			int shown = (int)n.children.size();
			if (shown > 800) shown = 800;
			for (int i = 0; i < shown; ++i)
				draw_node(n.children[i], depth + 1);

			if ((int)n.children.size() > shown)
			{
				ImGui::SetCursorPosX(indent + 28.f);
				line(IM_COL32(140, 145, 155, 255), "... %d more (use search)", (int)n.children.size() - shown);
			}
		}

		ImGui::PopID();
	}

	void draw_ctx()
	{
		if (g_open_ctx)
		{
			ImGui::OpenPopup("##exp_ctx");
			g_open_ctx = false;
		}

		col::theme_t& th = col::live();
		float wa = th.window[3];
		if (wa < 0.92f) wa = 0.94f;

		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(th.window[0], th.window[1], th.window[2], wa));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.10f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

		float menu_w = 176.f;
		float row_h = 28.f;
		float head_h = 52.f;
		bool script = IsScriptClass(g_ctx.cls);
		int n_items = script ? 6 : 5;
		ImGui::SetNextWindowSize(ImVec2(menu_w, head_h + n_items * row_h + 8.f));

		if (ImGui::BeginPopup("##exp_ctx", ImGuiWindowFlags_NoMove))
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 wp = ImGui::GetWindowPos();
			ImVec2 wsz = ImGui::GetWindowSize();

			dl->AddRect(
				wp,
				ImVec2(wp.x + wsz.x, wp.y + wsz.y),
				IM_COL32(255, 255, 255, 14),
				12.f, 0, 1.f
			);

			const char* nm = g_ctx.name.empty() ? "?" : g_ctx.name.c_str();
			dl->AddText(ImVec2(wp.x + 14.f, wp.y + 10.f), col::accent_u32(255), nm);
			dl->AddText(ImVec2(wp.x + 14.f, wp.y + 28.f), IM_COL32(140, 145, 155, 255), g_ctx.cls.c_str());
			dl->AddLine(
				ImVec2(wp.x + 12.f, wp.y + head_h - 1.f),
				ImVec2(wp.x + wsz.x - 12.f, wp.y + head_h - 1.f),
				IM_COL32(255, 255, 255, 18)
			);

			const char* labels[6];
			int n = 0;
			labels[n++] = "properties";
			if (script)
				labels[n++] = "view bytecode";
			labels[n++] = "copy name";
			labels[n++] = "copy class";
			labels[n++] = "copy path";
			labels[n++] = "copy address";

			for (int i = 0; i < n; ++i)
			{
				ImGui::SetCursorPos(ImVec2(8.f, head_h + i * row_h));
				ImGui::PushID(i);
				ImVec2 rp = ImGui::GetCursorScreenPos();
				ImGui::InvisibleButton("##ci", ImVec2(menu_w - 16.f, row_h));
				bool hov = ImGui::IsItemHovered();
				bool clk = ImGui::IsItemClicked();

				if (hov)
				{
					dl->AddRectFilled(
						rp,
						ImVec2(rp.x + menu_w - 16.f, rp.y + row_h - 2.f),
						col::accent_u32(45),
						8.f
					);
				}

				float fs = ImGui::GetFontSize();
				dl->AddText(
					ImVec2(rp.x + 10.f, rp.y + (row_h - fs) * 0.5f - 1.f),
					hov ? IM_COL32(245, 248, 255, 255) : IM_COL32(200, 205, 215, 230),
					labels[i]
				);

				if (clk)
				{
					if (std::strcmp(labels[i], "properties") == 0)
					{
						g_detail = g_ctx;
						g_detail_open = true;
						g_bc_for = 0;
						g_bc_bytes.clear();
						g_bc_status.clear();
						g_show_decompiled = false;
					}

					else if (std::strcmp(labels[i], "view bytecode") == 0)
					{
						g_detail = g_ctx;
						g_detail_open = true;
						DumpBytecode(g_detail);
						g_show_decompiled = false;
					}

					else if (std::strcmp(labels[i], "copy name") == 0)
						ImGui::SetClipboardText(g_ctx.name.c_str());

					else if (std::strcmp(labels[i], "copy class") == 0)
						ImGui::SetClipboardText(g_ctx.cls.c_str());

					else if (std::strcmp(labels[i], "copy path") == 0)
						ImGui::SetClipboardText(g_ctx.path.c_str());

					else if (std::strcmp(labels[i], "copy address") == 0)
					{
						char buf[32];
						std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)g_ctx.address);
						ImGui::SetClipboardText(buf);
					}

					ImGui::CloseCurrentPopup();
				}

				ImGui::PopID();
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor(2);
	}

	void draw_props(float alpha)
	{
		if (!g_detail.valid && !g_detail_open)
			return;

		bool open = menu::open && g_detail_open && g_detail.valid;
		bool script = IsScriptClass(g_detail.cls);
		bool hum = (g_detail.cls == "Humanoid");
		float info_h = hum ? 188.f : 148.f;
		// без actions/bytecode — окно по контенту, без дыры снизу
		float def_h = script ? 500.f : (34.f + 14.f + info_h + 16.f);
		float min_h = script ? 220.f : def_h;

		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
		bool began = ng::float_panel_begin(
			"##jewsploit_properties",
			"properties",
			&open,
			360.f, def_h,
			260.f, min_h
		);

		if (began)
		{
			ImVec2 wsz = ImGui::GetWindowSize();
			float head = ng::float_panel_head_h();
			float pad = 14.f;
			float cw = wsz.x - pad * 2.f;
			if (cw < 180.f) cw = 180.f;
			float y = head + 12.f;
			float gap = 10.f;

			static std::uint64_t last_addr = 0;
			if (g_detail.address != last_addr || ImGui::IsWindowAppearing())
			{
				last_addr = g_detail.address;
				if (!script)
				{
					float fit = head + 12.f + info_h + pad;
					ImGui::SetWindowSize(ImVec2(wsz.x, fit));
					wsz = ImGui::GetWindowSize();
				}
			}

			const char* nm = g_detail.name.empty() ? "?" : g_detail.name.c_str();

			ImGui::SetCursorPos(ImVec2(pad, y));
			if (ng::child_begin("##pr_info", "info", cw, info_h, 12.f, false))
			{
				line(col::accent_u32(255), "%s", nm);
				ImGui::Dummy(ImVec2(0.f, 6.f));

				kv("class", g_detail.cls.c_str());
				kv("path", g_detail.path.c_str(), true);

				char addr[40];
				std::snprintf(addr, sizeof(addr), "0x%llX", (unsigned long long)g_detail.address);
				kv("address", addr, true);

				Cheat::Instance inst(g_detail.address);
				char kids[32];
				std::snprintf(kids, sizeof(kids), "%d", (int)inst.GetChildren().size());
				kv("children", kids);

				if (hum)
				{
					Cheat::Humanoid hu(g_detail.address);
					char hp[48];
					std::snprintf(hp, sizeof(hp), "%.1f / %.1f", hu.GetHealth(), hu.GetMaxHealth());
					kv("health", hp);
					char move[56];
					std::snprintf(move, sizeof(move), "%.1f  jump %.1f", hu.GetWalkSpeed(), hu.GetJumpPower());
					kv("speed", move);
				}
			}
			ng::child_end();
			y += info_h + gap;

			if (script)
			{
				// хедер 34 + пад + кнопка 36 + пад снизу
				float act_h = 108.f;
				float btn_h = 36.f;
				ImGui::SetCursorPos(ImVec2(pad, y));
				if (ng::child_begin("##pr_act", "actions", cw, act_h, 12.f, false))
				{
					float avail = ImGui::GetContentRegionAvail().x;
					float gap_b = 8.f;
					float bw = (avail - gap_b * 3.f) / 4.f;
					if (bw < 58.f) bw = 58.f;

					if (ng::btn("##dump", "dump", bw, btn_h))
					{
						DumpBytecode(g_detail);
						g_show_decompiled = false;
					}
					ImGui::SameLine(0.f, gap_b);
					if (ng::btn("##save", "save", bw, btn_h))
						SaveBytecodeToFile(g_detail);
					ImGui::SameLine(0.f, gap_b);
					if (ng::btn("##hex", "copy hex", bw, btn_h))
					{
						std::string hex;
						hex.reserve(g_bc_bytes.size() * 2);
						static const char* d = "0123456789ABCDEF";
						for (unsigned char b : g_bc_bytes)
						{
							hex += d[b >> 4];
							hex += d[b & 0xF];
						}
						ImGui::SetClipboardText(hex.c_str());
					}
					ImGui::SameLine(0.f, gap_b);
					if (ng::btn("##dec", "decompile", bw, btn_h))
					{
						if (g_bc_bytes.empty() || g_bc_for != g_detail.address)
							DumpBytecode(g_detail);
						std::vector<std::uint8_t> raw(g_bc_bytes.begin(), g_bc_bytes.end());
						g_bc_status = "decompile...";
						g_decompiled = Cheat::Features::ScriptBytecode::Decompile(raw, g_detail.name.c_str());
						g_show_decompiled = true;
						if (g_decompiled.find("via Fission") != std::string::npos)
							g_bc_status = "fission ok";
						else if (g_decompiled.find("built-in lifter") != std::string::npos)
							g_bc_status = "lifter (no fission)";
						else if (g_decompiled.find("not Luau") != std::string::npos)
							g_bc_status = "no luau bytecode";
						else
							g_bc_status = "decompiled";
					}
				}
				ng::child_end();
				y += act_h + gap;

				float body_h = wsz.y - y - pad;
				if (body_h < 140.f) body_h = 140.f;
				const char* title = g_show_decompiled ? "decompiled" : "bytecode";

				ImGui::SetCursorPos(ImVec2(pad, y));
				if (ng::child_begin("##pr_bc", title, cw, body_h, 12.f, true))
				{
					if (!g_show_decompiled || g_decompiled.empty())
					{
						const char* st = "bytecode is compressed; not source";
						if (!g_bc_status.empty())
							st = g_bc_status.c_str();
						line(IM_COL32(140, 145, 155, 255), "%s", st);
						ImGui::Dummy(ImVec2(0.f, 8.f));
					}

					if (g_show_decompiled && !g_decompiled.empty())
					{
						// readonly multiline — выделить и ctrl+c как в браузере
						{
							const size_t n = g_decompiled.size();
							g_decompiled.resize(n + 1, '\0');
							g_decompiled.resize(n);
						}

						ImVec2 av = ImGui::GetContentRegionAvail();
						float box_h = av.y - 44.f;
						if (box_h < 80.f) box_h = 80.f;
						float box_w = av.x;
						if (box_w < 40.f) box_w = 40.f;

						ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
						ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
						ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.f));
						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 4.f));
						ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

						ImGui::InputTextMultiline(
							"##dec_src",
							g_decompiled.data(),
							g_decompiled.size() + 1,
							ImVec2(box_w, box_h),
							ImGuiInputTextFlags_ReadOnly
						);

						ImGui::PopStyleVar(2);
						ImGui::PopStyleColor(3);

						ImGui::Dummy(ImVec2(0.f, 8.f));
						if (ng::btn("##cpy", "copy source", 120.f, 32.f))
							ImGui::SetClipboardText(g_decompiled.c_str());
						ImGui::Dummy(ImVec2(0.f, 6.f));
					}

					else
					{
						size_t max_show = g_bc_bytes.size();
						if (max_show > 4096) max_show = 4096;
						char hexline[80];
						for (size_t i = 0; i < max_show; i += 16)
						{
							std::string row;
							for (size_t j = 0; j < 16 && i + j < max_show; ++j)
							{
								std::snprintf(hexline, sizeof(hexline), "%02X ", g_bc_bytes[i + j]);
								row += hexline;
							}
							ImGui::TextUnformatted(row.c_str());
						}
						if (g_bc_bytes.size() > max_show)
							line(IM_COL32(140, 145, 155, 255), "... (%zu more bytes)", g_bc_bytes.size() - max_show);
					}
				}
				ng::child_end();
			}

			ng::float_panel_end();
		}

		ImGui::PopStyleVar();

		if (!open && menu::open)
			g_detail_open = false;
	}
}

void ng_explorer::init_icons()
{
	if (g_icons_ok)
		return;
	if (!Cheat::Core::g_Device)
		return;

	g_icons_ok = true;
	RegisterIcon("workspace", workspace, (int)sizeof(workspace));
	RegisterIcon("folder", folder, (int)sizeof(folder));
	RegisterIcon("camera", camera, (int)sizeof(camera));
	RegisterIcon("lightning", lightning, (int)sizeof(lightning));
	RegisterIcon("humanoid", humanoid, (int)sizeof(humanoid));
	RegisterIcon("part", part, (int)sizeof(part));
	RegisterIcon("players", players, (int)sizeof(players));
	RegisterIcon("meshpart", meshpart, (int)sizeof(meshpart));
	RegisterIcon("player", player, (int)sizeof(player));
	RegisterIcon("model", model, (int)sizeof(model));
	RegisterIcon("terrain", terrain, (int)sizeof(terrain));
	RegisterIcon("localscript", localscript, (int)sizeof(localscript));
	RegisterIcon("localscripts", localscripts, (int)sizeof(localscripts));
	RegisterIcon("playergui", playergui, (int)sizeof(playergui));
	RegisterIcon("stats", stats, (int)sizeof(stats));
	RegisterIcon("guiservice", guiservice, (int)sizeof(guiservice));
	RegisterIcon("videocapture", videocapture, (int)sizeof(videocapture));
	RegisterIcon("runservice", runservice, (int)sizeof(runservice));
	RegisterIcon("frame", frame, (int)sizeof(frame));
	RegisterIcon("csd", csd, (int)sizeof(csd));
	RegisterIcon("contentprovider", contentprovider, (int)sizeof(contentprovider));
	RegisterIcon("nonreplicated", nonreplicated, (int)sizeof(nonreplicated));
	RegisterIcon("startergear", startergear, (int)sizeof(startergear));
	RegisterIcon("timerdevice", timerdevice, (int)sizeof(timerdevice));
	RegisterIcon("backpack", backpack, (int)sizeof(backpack));
	RegisterIcon("marketplaceservice", marketplaceservice, (int)sizeof(marketplaceservice));
	RegisterIcon("soundservice", soundservice, (int)sizeof(soundservice));
	RegisterIcon("logservice", logservice, (int)sizeof(logservice));
	RegisterIcon("statsitem", statsitem, (int)sizeof(statsitem));
	RegisterIcon("boolvalue", boolvalue, (int)sizeof(boolvalue));
	RegisterIcon("intvalue", intvalue, (int)sizeof(intvalue));
	RegisterIcon("doubletype", doubletype, (int)sizeof(doubletype));
	RegisterIcon("typeshit", typeshit, (int)sizeof(typeshit));
}

void ng_explorer::shutdown()
{
	StopSearch();
	for (auto& kv : g_icons)
	{
		if (kv.second.srv)
			kv.second.srv->Release();
	}
	g_icons.clear();
	g_icons_ok = false;
}

void ng_explorer::draw(float alpha)
{
	if (alpha <= 0.001f)
		return;

	bool open = menu::open && Cheat::g_Settings.misc.explorer;

	ng_explorer::init_icons();
	EnsureRoot();

	ImGui::PushFont(fonts::ui(), fonts::ui_size());
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

	bool began = ng::float_panel_begin(
		"##jewsploit_explorer",
		"explorer",
		&open,
		400.f, 540.f,
		280.f, 260.f
	);

	if (began)
	{
		ImVec2 wsz = ImGui::GetWindowSize();
		float head = ng::float_panel_head_h();
		float pad = 16.f;
		float cw = wsz.x - pad * 2.f;
		if (cw < 140.f) cw = 140.f;

		static char query[128] = "";

		ImGui::SetCursorPos(ImVec2(pad, head + 10.f));
		ImGui::PushItemWidth(cw);
		bool changed = ng::input("##exp_q", query, IM_ARRAYSIZE(query));
		ImGui::PopItemWidth();

		if (ImGui::IsItemDeactivatedAfterEdit() && query[0])
			StartSearch(query);
		if (changed && query[0] == '\0' && g_search_on)
			StopSearch();
		if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter, false) && query[0])
			StartSearch(query);

		float search_h = 34.f;
		float gap = 8.f;
		float tree_h = wsz.y - head - 10.f - search_h - gap - pad;
		if (tree_h < 120.f) tree_h = 120.f;

		ImGui::SetCursorPos(ImVec2(pad, head + 10.f + search_h + gap));
		if (ng::child_begin("##exp_dm", "datamodel", cw, tree_h, 12.f, true))
		{
			if (g_search_on)
			{
				std::lock_guard<std::mutex> lk(g_search_mtx);
				if (g_searching.load())
					line(IM_COL32(140, 145, 155, 255), "searching...");

				if (g_search_results.empty() && !g_searching.load())
					line(IM_COL32(140, 145, 155, 255), "no matches");

				ImGuiListClipper clipper;
				clipper.Begin((int)g_search_results.size(), 24.f);
				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
					{
						const SearchResult& r = g_search_results[i];
						ImGui::PushID(i);
						float* ht = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hov"), 0.f);
						int iw, ih;
						ID3D11ShaderResourceView* srv = IconSrv(r.cls, iw, ih);
						const char* label = r.name.empty() ? "?" : r.name.c_str();
						row_hit_t hit = draw_row(label, srv, 0.f, false, false, ht);
						if (hit.lmb || hit.rmb)
							open_ctx_for(r.address, r.name, r.cls, r.path);
						ImGui::PopID();
					}
				}
				clipper.End();
			}

			else if (g_root.address)
			{
				if (!g_root.loaded)
					LoadChildren(g_root);
				for (auto& c : g_root.children)
					draw_node(c, 0);
			}

			else
			{
				line(IM_COL32(140, 145, 155, 255), "not attached / no datamodel");
			}

			draw_ctx();
		}
		ng::child_end();

		ng::float_panel_end();
	}

	ImGui::PopStyleVar();

	if (!open && menu::open)
		Cheat::g_Settings.misc.explorer = false;

	draw_props(alpha);
	ImGui::PopFont();
}
