#include "label_keybind.h"
#include "keys.h"
#include "../colors/colors.h"
#include "../animation/animation.h"

#include <stdio.h>
#include <windows.h>
#include <imgui.h>

namespace
{
	bool ignore_key[256]{};
	bool g_waiting = false;

	void arm_ignore()
	{
		for (int k = 0; k < 256; k++)
		{
			ignore_key[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
		}

		ignore_key[VK_LBUTTON] = true;
	}

	void clear_ignore()
	{
		for (int k = 0; k < 256; k++)
		{
			ignore_key[k] = false;
		}
	}
}

bool ng::label_keybind_take_waiting()
{
	bool w = g_waiting;
	g_waiting = false;
	return w;
}

bool ng::label_keybind(const char* id, const char* label, int* vk)
{
	if (!id || !label || !vk)
	{
		return false;
	}

	ImGuiStorage* st = ImGui::GetStateStorage();
	ImGui::PushID(id);

	ImGuiID wid = ImGui::GetID("wait");
	int waiting = st->GetInt(wid, 0);

	float pad_r = 12.f;
	float kb_w = 70.f;
	float kb_h = 24.f;
	float row_h = 28.f;
	float rnd = 7.f;

	ImVec2 pos = ImGui::GetCursorScreenPos();
	float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
	float kb_x = right - pad_r - kb_w;
	float kb_y = pos.y + (row_h - kb_h) * 0.5f;
	float mid_y = pos.y + row_h * 0.5f;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	bool ch = false;

	ImGui::SetCursorScreenPos(ImVec2(kb_x, kb_y));
	ImGui::InvisibleButton("kb", ImVec2(kb_w, kb_h));
	bool kb_hit = ImGui::IsItemClicked();
	bool kb_hov = ImGui::IsItemHovered();

	if (kb_hit)
	{
		if (waiting)
		{
			waiting = 0;
			st->SetInt(wid, 0);
			clear_ignore();
		}

		else
		{
			waiting = 1;
			st->SetInt(wid, 1);
			arm_ignore();
		}
	}

	if (waiting)
	{
		for (int k = 1; k < 256; k++)
		{
			if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU)
			{
				continue;
			}

			bool down = (GetAsyncKeyState(k) & 0x8000) != 0;

			if (ignore_key[k])
			{
				if (!down)
				{
					ignore_key[k] = false;
				}

				continue;
			}

			if (!down)
			{
				continue;
			}

			if (k == VK_ESCAPE)
			{
				*vk = 0;
				waiting = 0;
				st->SetInt(wid, 0);
				clear_ignore();
				ch = true;
				break;
			}

			*vk = k;
			waiting = 0;
			st->SetInt(wid, 0);
			clear_ignore();
			ch = true;
			break;
		}
	}

	g_waiting = waiting != 0;

	char txt[64]{};
	if (waiting)
	{
		snprintf(txt, sizeof(txt), "...");
	}

	else
	{
		keys::name(*vk, txt, sizeof(txt));
	}

	ImGuiIO& io = ImGui::GetIO();
	ImGuiID ta_id = ImGui::GetID("txt_a");
	ImGuiID tv_id = ImGui::GetID("txt_v");
	int sig = waiting ? -1 : *vk;
	int prev_sig = st->GetInt(tv_id, sig);
	float txt_a = st->GetFloat(ta_id, 1.f);
	if (prev_sig != sig)
	{
		txt_a = 0.f;
		st->SetInt(tv_id, sig);
	}
	txt_a = anim::approach(txt_a, 1.f, 16.f, io.DeltaTime);
	st->SetFloat(ta_id, txt_a);
	float txt_e = anim::ease_out_cubic(txt_a);

	ImU32 bg = col::checkbox_off_u32();
	if (kb_hov || waiting)
	{
		bg = IM_COL32(36, 40, 50, 220);
	}

	ImU32 br = IM_COL32(255, 255, 255, waiting ? 48 : 28);
	dl->AddRectFilled(ImVec2(kb_x, kb_y), ImVec2(kb_x + kb_w, kb_y + kb_h), bg, rnd);
	dl->AddRect(ImVec2(kb_x, kb_y), ImVec2(kb_x + kb_w, kb_y + kb_h), br, rnd, 0, 1.f);

	ImVec2 kts = ImGui::CalcTextSize(txt);
	int ta = (int)(230.f * txt_e);
	float kty = kb_y + (kb_h - kts.y) * 0.5f + (1.f - txt_e) * 4.f;
	dl->AddText(
		ImVec2(kb_x + (kb_w - kts.x) * 0.5f, kty),
		IM_COL32(220, 226, 236, ta),
		txt
	);

	ImVec2 lts = ImGui::CalcTextSize(label);
	dl->AddText(
		ImVec2(pos.x, mid_y - lts.y * 0.5f),
		IM_COL32(220, 226, 236, 230),
		label
	);

	ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + row_h));
	ImGui::Dummy(ImVec2(0.f, 0.01f));

	ImGui::PopID();
	return ch;
}
