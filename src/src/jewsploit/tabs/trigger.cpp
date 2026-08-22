#include "trigger.h"
#include "helpers.h"
#include "../widgets/child.h"
#include "../widgets/checkbox.h"
#include "../widgets/keybind.h"
#include "../widgets/label_color.h"
#include "app/Settings.h"
#include "features/games/PhantomForces.h"
#include "features/aim/Triggerbot.h"

#include <imgui.h>

void ng_tabs::draw_trigger_tab()
{
	using namespace Cheat;

	Settings::TriggerbotConfig& cfg = g_Settings.triggerbot;

	float side_gap = 10.f;
	float avail_w = ImGui::GetContentRegionAvail().x;
	float avail_h = ImGui::GetContentRegionAvail().y;
	float cw = (avail_w - side_gap) * 0.5f;

	if (ng::child_begin("##trigger_child1", "trigger", cw, avail_h, 10.f, true))
	{
		row_keybind("##trigger_kb", "trigger key", &cfg.key, &cfg.key_mode);
		gap();

		pad();
		ng::checkbox("enabled", &cfg.enabled);
		gap();

		row_slider("##trigger_delay", "delay (ms)", &cfg.delay_ms, 1.0f, 300.0f);
		gap();

		row_slider("##trigger_click", "click duration", &cfg.click_duration_ms, 1.0f, 500.0f);
		gap();

		row_slider("##trigger_hc", "hit chance", &cfg.hitchance, 1.0f, 100.0f);
		gap();

		row_slider("##trigger_dist", "max distance", &cfg.max_distance, 1.0f, 2000.0f);
		gap();
	}
	ng::child_end();

	ImGui::SameLine(0.f, side_gap);

	if (ng::child_begin("##trigger_child2", "checks", cw, avail_h, 10.f, true))
	{
		static const char* k_hitboxes[] = { "whole body", "head", "torso", "arms", "legs", "hrp" };
		row_select("##hitboxes", "target part", &cfg.hitboxes, k_hitboxes, 6);
		gap();

		row_slider("##hitbox_mult", "hitbox size", &cfg.hitbox_multiplier, 1.0f, 4.0f);
		gap();

		bool c_team = (cfg.checks & Settings::TRIGGER_CHECK_TEAM) != 0;
		bool c_dead = (cfg.checks & Settings::TRIGGER_CHECK_HEALTH) != 0;
		bool c_ff = (cfg.checks & Settings::TRIGGER_CHECK_FF) != 0;
		bool c_invis = (cfg.checks & Settings::TRIGGER_CHECK_INVIS) != 0;
		bool c_vis = (cfg.checks & Settings::TRIGGER_CHECK_VISIBLE) != 0;

		pad();
		if (ng::checkbox("team check", &c_team))
			cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_TEAM) | (c_team ? Settings::TRIGGER_CHECK_TEAM : 0);
		gap();
		pad();
		if (ng::checkbox("dead check", &c_dead))
			cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_HEALTH) | (c_dead ? Settings::TRIGGER_CHECK_HEALTH : 0);
		gap();
		pad();
		if (ng::checkbox("forcefield check", &c_ff))
			cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_FF) | (c_ff ? Settings::TRIGGER_CHECK_FF : 0);
		gap();
		pad();
		if (ng::checkbox("invisible check", &c_invis))
			cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_INVIS) | (c_invis ? Settings::TRIGGER_CHECK_INVIS : 0);
		gap();
		pad();
		if (ng::checkbox("visible check", &c_vis))
			cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_VISIBLE) | (c_vis ? Settings::TRIGGER_CHECK_VISIBLE : 0);
		gap();

		if (Games::PhantomForces::IsActivePlace())
		{
			pad();
			ng::checkbox("pf parts", &cfg.pf_parts);
			gap();
		}

		pad();
		ng::checkbox("debug hud", &cfg.debug);
		gap();

		pad();
		ng::checkbox("prediction", &cfg.prediction_enabled);
		gap();

		if (cfg.prediction_enabled)
		{
			row_slider("##pred_xz", "pred x/z", &cfg.prediction_xz, 0.0f, 3.0f);
			gap();
			row_slider("##pred_y", "pred y", &cfg.prediction_y, 0.0f, 3.0f);
			gap();
			pad();
			ng::checkbox("draw prediction", &cfg.draw_prediction);
			gap();
		}

		lab(Features::Triggerbot::HasTarget() ? "Target under crosshair" : "No target");
	}
	ng::child_end();
}