#pragma once

#include "imgui.h"

namespace col
{
	struct theme_t
	{
		float accent[4];
		float child[4];
		float ctrl[4];
		float window[4];
	};

	theme_t& live();

	int preset_count();
	const char* const* preset_names();
	int preset_index();
	void set_preset(int idx);

	void get_accent(float out[4]);
	void set_accent(float in[4]);
	void get_child(float out[4]);
	void set_child(float in[4]);
	void get_ctrl(float out[4]);
	void set_ctrl(float in[4]);
	void get_window(float out[4]);
	void set_window(float in[4]);

	void push_to_imgui();

	inline ImU32 child_bg_u32()
	{
		theme_t& t = live();
		return IM_COL32(
			(int)(t.child[0] * 255.f + 0.5f),
			(int)(t.child[1] * 255.f + 0.5f),
			(int)(t.child[2] * 255.f + 0.5f),
			(int)(t.child[3] * 255.f + 0.5f)
		);
	}

	inline ImVec4 child_bg()
	{
		theme_t& t = live();
		return ImVec4(t.child[0], t.child[1], t.child[2], t.child[3]);
	}

	inline ImU32 nested_bg_u32()
	{
		theme_t& t = live();
		return IM_COL32(
			(int)(t.child[0] * 255.f + 0.5f),
			(int)(t.child[1] * 255.f + 0.5f),
			(int)(t.child[2] * 255.f + 0.5f),
			70
		);
	}

	inline ImU32 nested_br_u32()
	{
		return IM_COL32(255, 255, 255, 8);
	}

	inline ImU32 accent_u32(int a = 220)
	{
		theme_t& t = live();
		return IM_COL32(
			(int)(t.accent[0] * 255.f + 0.5f),
			(int)(t.accent[1] * 255.f + 0.5f),
			(int)(t.accent[2] * 255.f + 0.5f),
			a
		);
	}

	inline ImU32 checkbox_off_u32()
	{
		theme_t& t = live();
		return IM_COL32(
			(int)(t.ctrl[0] * 255.f + 0.5f),
			(int)(t.ctrl[1] * 255.f + 0.5f),
			(int)(t.ctrl[2] * 255.f + 0.5f),
			(int)(t.ctrl[3] * 255.f + 0.5f)
		);
	}

	inline ImU32 checkbox_on_u32()
	{
		return accent_u32(200);
	}

	inline ImU32 slider_track_u32()
	{
		return checkbox_off_u32();
	}

	inline ImU32 slider_fill_u32()
	{
		return checkbox_on_u32();
	}
}
