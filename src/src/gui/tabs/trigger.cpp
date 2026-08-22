#include "pch.h"
#include "trigger.h"
#include "helpers.h"

#include "imgui.h"
#include "widgets/widgets.h"
#include "app/Settings.h"
#include "features/games/PhantomForces.h"

#include <vector>

void ng_tabs::draw_trigger_tab()
{
    using namespace Cheat;
    using namespace ng_tabs;

    Settings::TriggerbotConfig& cfg = g_Settings.triggerbot;

    float left_w = 0.f, right_w = 0.f, h = 0.f;
    begin_columns(&left_w, &right_w, &h);

    begin_panel("##trigger_child1", left_w, h);
    {
        row_keybind("##trigger_kb", "trigger key", &cfg.key, &cfg.key_mode);
        row_checkbox("enabled", &cfg.enabled);

        row_slider_f("delay (ms)", &cfg.delay_ms, 1.0f, 300.0f, "%.0f");
        row_slider_f("click duration", &cfg.click_duration_ms, 1.0f, 500.0f, "%.0f");
        row_slider_f("hit chance", &cfg.hitchance, 1.0f, 100.0f, "%.0f");
        row_slider_f("max distance", &cfg.max_distance, 1.0f, 2000.0f, "%.0f");
    }
    end_panel();

    ImGui::SameLine(0.f, panel_gap);

    begin_panel("##trigger_right", right_w, h);
    {
        // выбор части тела: 0 whole, 1 head, 2 torso, 3 arms, 4 legs, 5 hrp
        static const std::vector<const char*> k_hitboxes = {
            "whole body", "head", "torso", "arms", "legs", "hrp"
        };
        if (cfg.hitboxes < 0 || cfg.hitboxes >= (int)k_hitboxes.size())
            cfg.hitboxes = Settings::TRIGGER_HITBOX_WHOLE;
        row_combo("target part", &cfg.hitboxes, k_hitboxes);

        // расширяем область срабатывания (визуально, сами парты не трогаем)
        row_slider_f("hitbox size", &cfg.hitbox_multiplier, 1.0f, 4.0f, "%.2f");

        // bitflag checks — same bits as gelato triggerbot
        bool c_team = (cfg.checks & Settings::TRIGGER_CHECK_TEAM) != 0;
        bool c_dead = (cfg.checks & Settings::TRIGGER_CHECK_HEALTH) != 0;
        bool c_ff = (cfg.checks & Settings::TRIGGER_CHECK_FF) != 0;
        bool c_invis = (cfg.checks & Settings::TRIGGER_CHECK_INVIS) != 0;
        bool c_vis = (cfg.checks & Settings::TRIGGER_CHECK_VISIBLE) != 0;

        if (row_checkbox("team check", &c_team))
            cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_TEAM) | (c_team ? Settings::TRIGGER_CHECK_TEAM : 0);
        if (row_checkbox("dead check", &c_dead))
            cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_HEALTH) | (c_dead ? Settings::TRIGGER_CHECK_HEALTH : 0);
        if (row_checkbox("forcefield check", &c_ff))
            cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_FF) | (c_ff ? Settings::TRIGGER_CHECK_FF : 0);
        if (row_checkbox("invisible check", &c_invis))
            cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_INVIS) | (c_invis ? Settings::TRIGGER_CHECK_INVIS : 0);
        if (row_checkbox("visible check", &c_vis))
            cfg.checks = (cfg.checks & ~Settings::TRIGGER_CHECK_VISIBLE) | (c_vis ? Settings::TRIGGER_CHECK_VISIBLE : 0);

        if (Games::PhantomForces::IsActivePlace())
            row_checkbox("pf parts", &cfg.pf_parts);

        row_checkbox("debug hud", &cfg.debug);

        row_checkbox("prediction", &cfg.prediction_enabled);
        if (cfg.prediction_enabled)
        {
            row_slider_f("pred x/z", &cfg.prediction_xz, 0.0f, 3.0f, "%.2f");
            row_slider_f("pred y", &cfg.prediction_y, 0.0f, 3.0f, "%.2f");
            row_checkbox("draw prediction", &cfg.draw_prediction);
        }
    }
    end_panel();
}