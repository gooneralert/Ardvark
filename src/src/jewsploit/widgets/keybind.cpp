#include "keybind.h"
#include "keys.h"
#include "../colors/colors.h"
#include "../animation/animation.h"

#include <stdio.h>
#include <windows.h>
#include <imgui.h>

namespace
{
	bool ignore_key[256]{};

	ImU32 with_a(ImU32 c, float a01)
	{
		if (a01 < 0.f) a01 = 0.f;
		if (a01 > 1.f) a01 = 1.f;
		ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
		v.w *= a01;
		return ImGui::ColorConvertFloat4ToU32(v);
	}

	const char* mode_name(int mode)
	{
		if (mode == 1) return "toggle";
		if (mode == 2) return "always";
		return "hold";
	}

	void arm_ignore()
	{
		for (int k = 0; k < 256; k++)
		{
			ignore_key[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
		}

		// клик по виджету всегда lmb — не брать его пока не отпустят и нажмут снова
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

bool ng::keybind(const char* id, int* vk, int* mode, bool shown)
{
	if (!vk || !mode)
	{
		return false;
	}

	if (*mode < 0) *mode = 0;
	if (*mode > 2) *mode = 2;

	ImGuiIO& io = ImGui::GetIO();
	ImGuiStorage* st = ImGui::GetStateStorage();

	ImGui::PushID(id);

	ImGuiID oid = ImGui::GetID("open");
	ImGuiID wid = ImGui::GetID("wait");

	float open = st->GetFloat(oid, shown ? 1.f : 0.f);
	open = anim::approach(open, shown ? 1.f : 0.f, 7.f, io.DeltaTime);
	st->SetFloat(oid, open);
	float e = anim::ease_out_cubic(open);

	ImGuiID skip_id = ImGui::GetID("skip");
	int waiting = st->GetInt(wid, 0);
	int skip_cap = st->GetInt(skip_id, 0);
	if (!shown)
	{
		if (waiting)
		{
			clear_ignore();
		}

		waiting = 0;
		skip_cap = 0;
		st->SetInt(wid, 0);
		st->SetInt(skip_id, 0);
	}

	if (e < 0.001f)
	{
		ImGui::PopID();
		return false;
	}

	ImVec2 row0 = ImGui::GetItemRectMin();
	ImVec2 row1 = ImGui::GetItemRectMax();
	float mid_y = (row0.y + row1.y) * 0.5f;

	const char* mc = mode_name(*mode);
	ImVec2 mts = ImGui::CalcTextSize(mc);

	float pad_r = 12.f;
	float gap = 6.f;
	float kb_w = 70.f;
	float kb_h = 24.f;
	float md_pad_x = 12.f;
	float md_w = mts.x + md_pad_x * 2.f;
	if (md_w < 56.f)
	{
		md_w = 56.f;
	}

	float md_h = 24.f;
	float rnd = 7.f;

	// dummy на всю строку -> row1.x; после чекбокса короткий -> content right
	float right = row1.x;
	float cr = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
	if (right < cr - 1.f)
	{
		right = cr;
	}

	float md_x = right - pad_r - md_w;
	float kb_x = md_x - gap - kb_w;

	float slide = (1.f - e) * 18.f;
	kb_x += slide;
	md_x += slide;

	float kb_y = mid_y - kb_h * 0.5f;
	float md_y = mid_y - md_h * 0.5f;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	bool ch = false;

	ImGui::SetCursorScreenPos(ImVec2(kb_x, kb_y));
	ImGui::InvisibleButton("kb", ImVec2(kb_w, kb_h));
	ImVec2 kb0 = ImGui::GetItemRectMin();
	ImVec2 kb1 = ImGui::GetItemRectMax();
	// HoveredRect+click — IsItemClicked врёт когда сверху/снизу чужой ActiveId
	bool kb_hov = ImGui::IsMouseHoveringRect(kb0, kb1);
	bool kb_hit = kb_hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	if (kb_hit)
	{
		if (waiting)
		{
			waiting = 0;
			st->SetInt(wid, 0);
			st->SetInt(skip_id, 0);
			clear_ignore();
		}

		else
		{
			waiting = 1;
			st->SetInt(wid, 1);
			st->SetInt(skip_id, 1);
			arm_ignore();
			skip_cap = 1;
		}
	}

	if (waiting && skip_cap)
	{
		// кадр клика / пока лкм ещё зажат — не жрем клавиши
		if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
		{
			skip_cap = 0;
			st->SetInt(skip_id, 0);
		}
	}

	else if (waiting)
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

	char txt[64]{};
	if (waiting)
	{
		snprintf(txt, sizeof(txt), "...");
	}

	else
	{
		keys::name(*vk, txt, sizeof(txt));
	}

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

	ImU32 bg = with_a(col::checkbox_off_u32(), e);
	if (kb_hov || waiting)
	{
		bg = with_a(IM_COL32(36, 40, 50, 220), e);
	}

	ImU32 br = with_a(IM_COL32(255, 255, 255, waiting ? 48 : 28), e);
	ImU32 tx = with_a(IM_COL32(220, 226, 236, 230), e * txt_e);

	dl->AddRectFilled(ImVec2(kb_x, kb_y), ImVec2(kb_x + kb_w, kb_y + kb_h), bg, rnd);
	dl->AddRect(ImVec2(kb_x, kb_y), ImVec2(kb_x + kb_w, kb_y + kb_h), br, rnd, 0, 1.f);

	ImVec2 ts = ImGui::CalcTextSize(txt);
	float ty = kb_y + (kb_h - ts.y) * 0.5f + (1.f - txt_e) * 4.f;
	dl->AddText(
		ImVec2(kb_x + (kb_w - ts.x) * 0.5f, ty),
		tx,
		txt
	);

	ImGui::SetCursorScreenPos(ImVec2(md_x, md_y));
	ImGui::InvisibleButton("md", ImVec2(md_w, md_h));
	ImVec2 md0 = ImGui::GetItemRectMin();
	ImVec2 md1 = ImGui::GetItemRectMax();
	bool md_hov = ImGui::IsMouseHoveringRect(md0, md1);
	bool md_hit = md_hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	if (md_hit)
	{
		*mode = *mode + 1;
		if (*mode > 2) *mode = 0;
		ch = true;
	}

	ImU32 mbg = with_a(col::checkbox_off_u32(), e);
	if (md_hov)
	{
		mbg = with_a(IM_COL32(36, 40, 50, 220), e);
	}

	dl->AddRectFilled(ImVec2(md_x, md_y), ImVec2(md_x + md_w, md_y + md_h), mbg, rnd);
	dl->AddRect(ImVec2(md_x, md_y), ImVec2(md_x + md_w, md_y + md_h), with_a(IM_COL32(255, 255, 255, 28), e), rnd, 0, 1.f);

	mts = ImGui::CalcTextSize(mc);
	ImGuiID ma_id = ImGui::GetID("md_a");
	ImGuiID mv_id = ImGui::GetID("md_v");
	int prev_md = st->GetInt(mv_id, *mode);
	float md_a = st->GetFloat(ma_id, 1.f);
	if (prev_md != *mode)
	{
		md_a = 0.f;
		st->SetInt(mv_id, *mode);
	}
	md_a = anim::approach(md_a, 1.f, 16.f, io.DeltaTime);
	st->SetFloat(ma_id, md_a);
	float md_e = anim::ease_out_cubic(md_a);
	ImU32 mtx = with_a(IM_COL32(220, 226, 236, 230), e * md_e);
	float mty = md_y + (md_h - mts.y) * 0.5f + (1.f - md_e) * 3.f;
	dl->AddText(
		ImVec2(md_x + (md_w - mts.x) * 0.5f, mty),
		mtx,
		mc
	);

	ImGui::SetCursorScreenPos(ImVec2(row0.x, row1.y));
	ImGui::Dummy(ImVec2(0.f, 0.01f));

	ImGui::PopID();
	return ch;
}