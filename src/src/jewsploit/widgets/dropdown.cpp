#include "dropdown.h"
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

	void make_preview(char* out, int out_n, bool* sel, const char* const items[], int count)
	{
		out[0] = 0;
		int n = 0;
		int used = 0;

		for (int i = 0; i < count; i++)
		{
			if (!sel[i])
			{
				continue;
			}

			int wrote = 0;
			if (n > 0)
			{
				wrote = snprintf(out + used, out_n - used, ", %s", items[i]);
			}

			else
			{
				wrote = snprintf(out + used, out_n - used, "%s", items[i]);
			}

			if (wrote < 0)
			{
				break;
			}

			used += wrote;
			if (used >= out_n - 1)
			{
				out[out_n - 1] = 0;
				return;
			}

			n++;
		}

		if (n == 0)
		{
			snprintf(out, out_n, "none");
		}
	}
}

bool ng::dropdown(const char* id, bool* sel, const char* const items[], int count, bool shown)
{
	if (!sel || !items || count <= 0)
	{
		return false;
	}

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

	if (e < 0.001f)
	{
		st->SetFloat(lid, 0.f);
		ImGui::PopID();
		return false;
	}

	float pad_r = 12.f;
	float avail = ImGui::GetContentRegionAvail().x;
	float box_w = avail - pad_r;
	if (box_w < 40.f)
	{
		box_w = 40.f;
	}

	float head_h = 30.f;
	float item_h = 28.f;
	float gap = 6.f;
	float rnd = 10.f;
	float top_pad = item_gap;

	float list = st->GetFloat(lid, 0.f);
	float list_e = anim::ease_out_cubic(list);
	float list_h = (float)count * item_h * list_e;
	float body = head_h + (list_h > 0.5f ? gap + list_h : 0.f);
	float total_h = (top_pad + body) * e;

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImGui::Dummy(ImVec2(box_w, total_h));

	float hx = pos.x;
	float hy = pos.y + top_pad * e;

	ImGui::SetCursorScreenPos(ImVec2(hx, hy));
	ImGui::InvisibleButton("hdr", ImVec2(box_w, head_h));
	bool head_hit = ImGui::IsItemClicked();
	bool head_hov = ImGui::IsItemHovered();

	if (head_hit && e > 0.55f)
	{
		want = want ? 0 : 1;
		st->SetInt(wid, want);
	}

	list = anim::approach(list, want ? 1.f : 0.f, 7.f, io.DeltaTime);
	st->SetFloat(lid, list);
	list_e = anim::ease_out_cubic(list);
	list_h = (float)count * item_h * list_e;

	char preview[128]{};
	make_preview(preview, sizeof(preview), sel, items, count);

	// смена выбора — текст заново выезжает (без ease_out, а то мигом полный)
	ImGuiID pa_id = ImGui::GetID("prev_a");
	ImGuiID ph_id = ImGui::GetID("prev_h");
	int ph = 0;
	for (int i = 0; i < count; i++)
	{
		ph = ph * 33 + (sel[i] ? (i + 1) : 0);
	}
	int prev_h = st->GetInt(ph_id, ph);
	float prev_a = st->GetFloat(pa_id, 1.f);
	if (prev_h != ph)
	{
		prev_a = 0.f;
		st->SetInt(ph_id, ph);
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

	ImU32 bg = with_a(col::checkbox_off_u32(), e);
	if (head_hov)
	{
		bg = with_a(IM_COL32(36, 40, 50, 220), e);
	}

	ImU32 br = with_a(IM_COL32(255, 255, 255, 28), e);

	dl->AddRectFilled(ImVec2(hx, hy), ImVec2(hx + box_w, hy + head_h), bg, rnd);
	dl->AddRect(ImVec2(hx, hy), ImVec2(hx + box_w, hy + head_h), br, rnd, 0, 1.f);

	ImVec2 pts = ImGui::CalcTextSize(preview);
	float ptx0 = hx + 12.f;
	float pty = hy + (head_h - pts.y) * 0.5f;
	float slide = (1.f - prev_a) * 10.f;
	ImU32 ptx = with_a(IM_COL32(220, 226, 236, 230), e * prev_a);

	dl->PushClipRect(ImVec2(hx + 8.f, hy), ImVec2(hx + box_w - 28.f, hy + head_h), true);
	dl->AddText(ImVec2(ptx0 - slide, pty), ptx, preview);
	dl->PopClipRect();

	float ax = hx + box_w - 16.f;
	float ay = hy + head_h * 0.5f;
	ImVec2 a = ImVec2(ax - 5.f, ay - 2.f + 4.f * list_e);
	ImVec2 b = ImVec2(ax + 5.f, ay - 2.f + 4.f * list_e);
	ImVec2 c = ImVec2(ax, ay + 3.f - 6.f * list_e);
	dl->AddTriangleFilled(a, b, c, with_a(IM_COL32(200, 208, 220, 200), e));

	bool ch = false;

	if (list_e > 0.001f)
	{
		float ly = hy + head_h + gap;
		float lh = list_h;
		float ir = 8.f;

		ImU32 lbg = with_a(IM_COL32(18, 20, 26, 235), e * list_e);
		dl->AddRectFilled(ImVec2(hx, ly), ImVec2(hx + box_w, ly + lh), lbg, ir);
		dl->AddRect(
			ImVec2(hx, ly),
			ImVec2(hx + box_w, ly + lh),
			with_a(IM_COL32(255, 255, 255, 20), e * list_e),
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
				sel[i] = !sel[i];
				ch = true;
			}

			if (ImGui::IsItemHovered())
			{
				dl->AddRectFilled(
					ImVec2(hx + 3.f, iy + 2.f),
					ImVec2(hx + box_w - 3.f, iy + ih - 2.f),
					with_a(IM_COL32(255, 255, 255, 12), e * list_e),
					6.f
				);
			}

			ImVec2 its = ImGui::CalcTextSize(items[i]);
			ImU32 col_txt = sel[i]
				? with_a(col::accent_u32(240), e * list_e)
				: with_a(IM_COL32(170, 176, 188, 220), e * list_e);

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
			ng::scroll_fade(
				dl,
				ImVec2(hx, ly),
				ImVec2(hx + box_w, ly + lh),
				0.f,
				0.9f * list_e,
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