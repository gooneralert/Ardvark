#include "scroll_fade.h"

namespace
{
	ImU32 with_a(ImU32 c, int a)
	{
		if (a < 0) a = 0;
		if (a > 255) a = 255;
		return (c & 0x00FFFFFFu) | ((ImU32)a << 24);
	}
}

void ng::scroll_fade(ImDrawList* dl, ImVec2 a, ImVec2 b, float top_k, float bot_k, ImU32 bg, float fade_h)
{
	if (!dl)
	{
		return;
	}

	if (fade_h < 4.f) fade_h = 4.f;

	float h = b.y - a.y;
	if (h < fade_h * 2.f)
	{
		fade_h = h * 0.45f;
	}

	if (top_k < 0.f) top_k = 0.f;
	if (top_k > 1.f) top_k = 1.f;
	if (bot_k < 0.f) bot_k = 0.f;
	if (bot_k > 1.f) bot_k = 1.f;

	int base_a = (int)((bg >> 24) & 0xff);
	if (base_a < 1) base_a = 220;

	if (top_k > 0.01f)
	{
		float fh = fade_h;
		int a0 = (int)((float)base_a * top_k);
		ImU32 c0 = with_a(bg, a0);
		ImU32 c1 = with_a(bg, 0);
		dl->AddRectFilledMultiColor(
			ImVec2(a.x, a.y),
			ImVec2(b.x, a.y + fh),
			c0, c0, c1, c1
		);
	}

	if (bot_k > 0.01f)
	{
		float fh = fade_h;
		int a0 = (int)((float)base_a * bot_k);
		ImU32 c0 = with_a(bg, 0);
		ImU32 c1 = with_a(bg, a0);
		dl->AddRectFilledMultiColor(
			ImVec2(a.x, b.y - fh),
			ImVec2(b.x, b.y),
			c0, c0, c1, c1
		);
	}
}

void ng::scroll_fade_child(ImU32 bg, float fade_h)
{
	float sy = ImGui::GetScrollY();
	float smax = ImGui::GetScrollMaxY();
	if (smax < 1.f)
	{
		return;
	}

	float top_k = sy / 22.f;
	if (top_k > 1.f) top_k = 1.f;

	float bot_k = (smax - sy) / 22.f;
	if (bot_k > 1.f) bot_k = 1.f;

	ImVec2 a = ImGui::GetWindowPos();
	ImVec2 sz = ImGui::GetWindowSize();
	ImVec2 b = ImVec2(a.x + sz.x, a.y + sz.y);

	scroll_fade(ImGui::GetWindowDrawList(), a, b, top_k, bot_k, bg, fade_h);
}
