#include "select.h"
#include "spacing.h"
#include "scroll_fade.h"
#include "../colors/colors.h"
#include "../animation/animation.h"

#include <stdio.h>
#include <math.h>
#include <imgui.h>

namespace
{
	ImU32 with_a(ImU32 c, float a01)
	{
		if (a01 < 0.f) a01 = 0.f;
		if (a01 > 1.f) a01 = 1.f;
		ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
		v.w *= a01;
		return ImGui::ColorConvertFloat4ToU32(v);
	}
}

bool ng::select(const char* id, int* cur, const char* const items[], int count, bool shown, bool close_on_pick)
{
	if (!cur || !items || count <= 0)
	{
		return false;
	}

	if (*cur < 0) *cur = 0;
	if (*cur >= count) *cur = count - 1;

	ImGuiIO& io = ImGui::GetIO();
	ImGuiStorage* st = ImGui::GetStateStorage();

	ImGui::PushID(id);

	ImGuiID oid = ImGui::GetID("open");
	ImGuiID lid = ImGui::GetID("list");
	ImGuiID wid = ImGui::GetID("want");

	float open = st->GetFloat(oid, shown ? 1.f : 0.f);
	open = anim::approach(open, shown ? 1.f : 0.f, 7.f, io.DeltaTime);
	st->SetFloat(oid, open);
	float e = anim::ease_out_cubic(open);

	int want = st->GetInt(wid, 0);
	if (!shown)
	{
		want = 0;
		st->SetInt(wid, 0);
	}

	float list = st->GetFloat(lid, 0.f);

	if (e < 0.001f)
	{
		st->SetFloat(lid, 0.f);
		ImGui::PopID();
		return false;
	}

	float pad_r = 12.f;
	float avail = ImGui::GetContentRegionAvail().x;
	float box_w = avail - pad_r;
	if (box_w < 40.f) box_w = 40.f;

	float head_h = 30.f * e;
	float item_h = 28.f;
	float gap = 6.f;
	float rnd = 10.f;
	float top_pad = item_gap * e;

	float list_e = anim::ease_out_cubic(list) * e;
	float list_h = (float)count * item_h * list_e;
	float body = head_h + (list_h > 0.5f ? gap + list_h : 0.f);
	float total_h = top_pad + body;

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImGui::Dummy(ImVec2(box_w, total_h));

	float hx = pos.x;
	float hy = pos.y + top_pad;

	ImGui::SetCursorScreenPos(ImVec2(hx, hy));
	ImGui::InvisibleButton("hdr", ImVec2(box_w, head_h));
	bool head_hit = ImGui::IsItemClicked();
	bool head_hov = ImGui::IsItemHovered();

	if (head_hit)
	{
		want = want ? 0 : 1;
		st->SetInt(wid, want);
	}

	list = anim::approach(list, want ? 1.f : 0.f, 7.f, io.DeltaTime);
	st->SetFloat(lid, list);
	list_e = anim::ease_out_cubic(list) * e;
	list_h = (float)count * item_h * list_e;

	const char* preview = items[*cur];

	ImGuiID pa_id = ImGui::GetID("prev_a");
	ImGuiID pc_id = ImGui::GetID("prev_c");
	int prev_c = st->GetInt(pc_id, *cur);
	float prev_a = st->GetFloat(pa_id, 1.f);
	if (prev_c != *cur)
	{
		prev_a = 0.f;
		st->SetInt(pc_id, *cur);
	}

	{
		float d = 1.f - prev_a;
		if (d < 0.001f)
		{
			prev_a = 1.f;
		}

		else
		{
			float k = 1.f - expf(-9.f * io.DeltaTime);
			prev_a = prev_a + d * k;
		}
	}
	st->SetFloat(pa_id, prev_a);

	ImU32 bg = col::checkbox_off_u32();
	if (head_hov)
	{
		bg = IM_COL32(36, 40, 50, 220);
	}

	ImU32 br = IM_COL32(255, 255, 255, 28);

	dl->AddRectFilled(ImVec2(hx, hy), ImVec2(hx + box_w, hy + head_h), bg, rnd);
	dl->AddRect(ImVec2(hx, hy), ImVec2(hx + box_w, hy + head_h), br, rnd, 0, 1.f);

	ImVec2 pts = ImGui::CalcTextSize(preview);
	int pa = (int)(230.f * prev_a);
	ImU32 ptx = IM_COL32(220, 226, 236, pa);
	float slide = (1.f - prev_a) * 10.f;
	float pty = hy + (head_h - pts.y) * 0.5f;
	dl->PushClipRect(ImVec2(hx + 8.f, hy), ImVec2(hx + box_w - 28.f, hy + head_h), true);
	dl->AddText(ImVec2(hx + 12.f - slide, pty), ptx, preview);
	dl->PopClipRect();

	float ax = hx + box_w - 16.f;
	float ay = hy + head_h * 0.5f;
	ImVec2 a = ImVec2(ax - 5.f, ay - 2.f + 4.f * list_e);
	ImVec2 b = ImVec2(ax + 5.f, ay - 2.f + 4.f * list_e);
	ImVec2 c = ImVec2(ax, ay + 3.f - 6.f * list_e);
	dl->AddTriangleFilled(a, b, c, IM_COL32(200, 208, 220, 200));

	bool ch = false;

	if (list_e > 0.001f)
	{
		float ly = hy + head_h + gap;
		float lh = list_h;
		float ir = 8.f;

		ImU32 lbg = with_a(IM_COL32(18, 20, 26, 235), list_e);
		dl->AddRectFilled(ImVec2(hx, ly), ImVec2(hx + box_w, ly + lh), lbg, ir);
		dl->AddRect(
			ImVec2(hx, ly),
			ImVec2(hx + box_w, ly + lh),
			with_a(IM_COL32(255, 255, 255, 20), list_e),
			ir,
			0,
			1.f
		);

		dl->PushClipRect(ImVec2(hx, ly), ImVec2(hx + box_w, ly + lh), true);

		for (int i = 0; i < count; i++)
		{
			float iy = ly + (float)i * item_h;
			float ih = item_h;
			if (iy + 2.f > ly + lh)
			{
				break;
			}

			ImGui::SetCursorScreenPos(ImVec2(hx, iy));
			char iid[24]{};
			snprintf(iid, sizeof(iid), "it%d", i);
			ImGui::InvisibleButton(iid, ImVec2(box_w, ih));

			if (ImGui::IsItemClicked() && list_e > 0.55f)
			{
				*cur = i;
				ch = true;
				if (close_on_pick)
				{
					want = 0;
					st->SetInt(wid, 0);
				}
			}

			if (ImGui::IsItemHovered())
			{
				dl->AddRectFilled(
					ImVec2(hx + 3.f, iy + 2.f),
					ImVec2(hx + box_w - 3.f, iy + ih - 2.f),
					with_a(IM_COL32(255, 255, 255, 12), list_e),
					6.f
				);
			}

			ImVec2 its = ImGui::CalcTextSize(items[i]);
			ImU32 col_txt = (*cur == i)
				? with_a(col::accent_u32(240), list_e)
				: with_a(IM_COL32(170, 176, 188, 220), list_e);

			dl->AddText(
				ImVec2(hx + 12.f, iy + (ih - its.y) * 0.5f),
				col_txt,
				items[i]
			);
		}

		dl->PopClipRect();

		float full_h = (float)count * item_h;
		if (full_h > lh + 0.5f)
		{
			float bot_k = 0.9f * list_e;
			float top_k = 0.f;
			ng::scroll_fade(
				dl,
				ImVec2(hx, ly),
				ImVec2(hx + box_w, ly + lh),
				top_k,
				bot_k,
				IM_COL32(18, 20, 26, 240),
				16.f
			);
		}
	}

	ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + total_h));
	ImGui::Dummy(ImVec2(0.f, 0.01f));

	ImGui::PopID();
	return ch;
}
