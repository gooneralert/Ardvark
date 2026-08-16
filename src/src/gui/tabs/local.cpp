#include "pch.h"
#include "local.h"
#include "helpers.h"

#include "imgui.h"
#include "widgets/widgets.h"
#include "app/Settings.h"
#include "features/local/CharMods.h"

#include <vector>

void ng_tabs::draw_local_tab()
{
	using namespace Cheat;
	using namespace ng_tabs;

	auto& m = g_Settings.misc;

	float left_w = 0.f, right_w = 0.f, h = 0.f;
	begin_columns(&left_w, &right_w, &h);

	static const std::vector<const char*> ws_modes = { "position", "humanoid" };
	static const std::vector<const char*> anim_pack_names(
		Features::CharMods::AnimPackNames(),
		Features::CharMods::AnimPackNames() + Features::CharMods::AnimPackCount());

	begin_panel("##local_move", left_w, h);
	{
		row_checkbox("walkspeed", &m.walkspeed);
		if (m.walkspeed)
		{
			row_keybind("##ws_kb", "ws key", &m.walkspeed_key, &m.walkspeed_key_mode);
			row_combo("ws mode", &m.walkspeed_mode, ws_modes);
			row_slider_f("ws value", &m.walkspeed_value, 1.f, 500.f, "%.0f");
		}

		row_checkbox("jump power", &m.jump);
		if (m.jump)
			row_slider_f("jump value", &m.jump_power, 50.f, 500.f, "%.0f");

		row_checkbox("fly", &m.fly);
		if (m.fly)
		{
			row_keybind("##fly_kb", "fly key", &m.fly_key, &m.fly_key_mode);
			row_checkbox("fly gravity", &m.fly_gravity);
			row_slider_f("fly speed", &m.fly_speed, 5.f, 1000.f, "%.0f");
		}

		row_checkbox("gravity", &m.gravity);
		if (m.gravity)
			row_slider_f("gravity value", &m.gravity_value, -500.f, 500.f, "%.1f");

		row_checkbox("tickrate", &m.tickrate);
		if (m.tickrate)
			row_slider_f("tickrate value", &m.tickrate_value, 1.f, 2000.f, "%.0f");

		row_checkbox("animation changer", &m.anim_changer);
		if (m.anim_changer)
			row_combo("anim pack", &m.anim_pack, anim_pack_names);

		row_checkbox("fake headless", &m.fake_headless);
		if (widgets::button("reset to show (headless)"))
			m.reset_fake_headless = true;

		row_checkbox("korblox", &m.korblox);
		if (widgets::button("reset to show (korblox)"))
			m.reset_korblox = true;

		row_checkbox("noclip", &m.noclip);
		row_checkbox("infinite jump", &m.inf_jump);
		row_checkbox("btools", &m.bTools);
		if (m.bTools)
		{
			static const std::vector<const char*> btools_tools = { "hammer", "grab", "clone" };
			row_combo("btool", &m.bToolsTool, btools_tools);
		}
	}
	end_panel();

	ImGui::SameLine(0.f, panel_gap);

	begin_panel("##local_cam", right_w, h);
	{
		row_keybind("##freecam", "freecam", &m.freecam_key, &m.freecam_mode);
		row_slider_f("freecam speed", &m.freecam_speed, 10.f, 300.f, "%.0f");
		row_slider_f("freecam sens", &m.freecam_sens, 0.05f, 1.f, "%.2f");

		row_checkbox("third person", &m.third_person);
		if (m.third_person)
		{
			row_keybind("##tp_kb", "tp key", &m.third_person_key, &m.third_person_mode);
			row_slider_f("camera back/up", &m.third_person_distance, 1.f, 120.f, "%.1f");
		}
	}
	end_panel();
}
