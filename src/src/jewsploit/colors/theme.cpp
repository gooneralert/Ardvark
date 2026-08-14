#include "colors.h"

namespace
{
	col::theme_t g_live{};
	int g_preset = 0;

	const char* g_names[] = {
		"default",
		"neverlose",
		"skeet",
		"fatality",
		"gamesense",
		"onetap",
		"primordial",
		"aimware",
		"nixware",
		"custom"
	};

	void set4(float* d, float r, float g, float b, float a)
	{
		d[0] = r;
		d[1] = g;
		d[2] = b;
		d[3] = a;
	}

	void copy4(float* d, const float* s)
	{
		d[0] = s[0];
		d[1] = s[1];
		d[2] = s[2];
		d[3] = s[3];
	}

	col::theme_t make_preset(int idx)
	{
		col::theme_t t{};

		set4(t.window, 0.08f, 0.09f, 0.11f, 0.78f);
		set4(t.child, 8.f / 255.f, 10.f / 255.f, 14.f / 255.f, 95.f / 255.f);
		set4(t.ctrl, 28.f / 255.f, 32.f / 255.f, 40.f / 255.f, 200.f / 255.f);
		set4(t.accent, 210.f / 255.f, 215.f / 255.f, 230.f / 255.f, 1.f);

		if (idx == 1)
		{
			set4(t.window, 0.06f, 0.08f, 0.12f, 0.92f);
			set4(t.child, 0.05f, 0.07f, 0.11f, 0.55f);
			set4(t.ctrl, 0.10f, 0.13f, 0.20f, 0.90f);
			set4(t.accent, 0.25f, 0.55f, 1.f, 1.f);
		}

		else if (idx == 2)
		{
			set4(t.window, 0.07f, 0.07f, 0.07f, 0.95f);
			set4(t.child, 0.09f, 0.09f, 0.09f, 0.70f);
			set4(t.ctrl, 0.14f, 0.14f, 0.14f, 0.92f);
			set4(t.accent, 0.55f, 0.85f, 0.20f, 1.f);
		}

		else if (idx == 3)
		{
			set4(t.window, 0.08f, 0.06f, 0.10f, 0.92f);
			set4(t.child, 0.10f, 0.07f, 0.12f, 0.60f);
			set4(t.ctrl, 0.16f, 0.12f, 0.20f, 0.90f);
			set4(t.accent, 0.85f, 0.30f, 0.70f, 1.f);
		}

		else if (idx == 4)
		{
			set4(t.window, 0.08f, 0.09f, 0.08f, 0.94f);
			set4(t.child, 0.09f, 0.11f, 0.09f, 0.55f);
			set4(t.ctrl, 0.12f, 0.15f, 0.12f, 0.90f);
			set4(t.accent, 0.45f, 0.90f, 0.35f, 1.f);
		}

		else if (idx == 5)
		{
			set4(t.window, 0.07f, 0.08f, 0.11f, 0.93f);
			set4(t.child, 0.08f, 0.10f, 0.14f, 0.55f);
			set4(t.ctrl, 0.12f, 0.15f, 0.22f, 0.90f);
			set4(t.accent, 0.30f, 0.70f, 0.95f, 1.f);
		}

		else if (idx == 6)
		{
			set4(t.window, 0.07f, 0.06f, 0.10f, 0.94f);
			set4(t.child, 0.09f, 0.07f, 0.13f, 0.58f);
			set4(t.ctrl, 0.15f, 0.12f, 0.22f, 0.90f);
			set4(t.accent, 0.65f, 0.40f, 0.95f, 1.f);
		}

		else if (idx == 7)
		{
			set4(t.window, 0.09f, 0.07f, 0.07f, 0.93f);
			set4(t.child, 0.12f, 0.08f, 0.08f, 0.55f);
			set4(t.ctrl, 0.20f, 0.12f, 0.12f, 0.90f);
			set4(t.accent, 0.95f, 0.35f, 0.30f, 1.f);
		}

		else if (idx == 8)
		{
			set4(t.window, 0.06f, 0.09f, 0.10f, 0.93f);
			set4(t.child, 0.07f, 0.11f, 0.12f, 0.55f);
			set4(t.ctrl, 0.10f, 0.16f, 0.18f, 0.90f);
			set4(t.accent, 0.20f, 0.85f, 0.90f, 1.f);
		}

		return t;
	}
}

namespace col
{
	theme_t& live()
	{
		static bool inited = false;
		if (!inited)
		{
			g_live = make_preset(0);
			g_preset = 0;
			inited = true;
		}

		return g_live;
	}

	int preset_count()
	{
		return 10;
	}

	const char* const* preset_names()
	{
		return g_names;
	}

	int preset_index()
	{
		live();
		return g_preset;
	}

	void set_preset(int idx)
	{
		live();
		if (idx < 0) idx = 0;
		if (idx > 9) idx = 9;

		if (idx == 9)
		{
			g_preset = 9;
			return;
		}

		g_live = make_preset(idx);
		g_preset = idx;
	}

	void get_accent(float out[4]) { copy4(out, live().accent); }
	void get_child(float out[4]) { copy4(out, live().child); }
	void get_ctrl(float out[4]) { copy4(out, live().ctrl); }
	void get_window(float out[4]) { copy4(out, live().window); }

	void set_accent(float in[4])
	{
		copy4(live().accent, in);
	}

	void set_child(float in[4])
	{
		copy4(live().child, in);
	}

	void set_ctrl(float in[4])
	{
		copy4(live().ctrl, in);
	}

	void set_window(float in[4])
	{
		copy4(live().window, in);
	}

	void push_to_imgui()
	{
		theme_t& t = live();
		ImGuiStyle& st = ImGui::GetStyle();
		ImVec4* c = st.Colors;

		c[ImGuiCol_WindowBg] = ImVec4(t.window[0], t.window[1], t.window[2], t.window[3]);
		c[ImGuiCol_ChildBg] = child_bg();
		c[ImGuiCol_PopupBg] = ImVec4(t.window[0], t.window[1], t.window[2], 0.96f);
		c[ImGuiCol_CheckMark] = ImVec4(t.accent[0], t.accent[1], t.accent[2], 1.f);
		c[ImGuiCol_SliderGrab] = ImVec4(t.accent[0], t.accent[1], t.accent[2], 1.f);

		float ar = t.accent[0] * 1.05f;
		float ag = t.accent[1] * 1.05f;
		float ab = t.accent[2] * 1.05f;
		if (ar > 1.f) ar = 1.f;
		if (ag > 1.f) ag = 1.f;
		if (ab > 1.f) ab = 1.f;
		c[ImGuiCol_SliderGrabActive] = ImVec4(ar, ag, ab, 1.f);

		c[ImGuiCol_TitleBg] = c[ImGuiCol_WindowBg];
		c[ImGuiCol_TitleBgActive] = c[ImGuiCol_WindowBg];
		c[ImGuiCol_TitleBgCollapsed] = c[ImGuiCol_WindowBg];
	}
}
