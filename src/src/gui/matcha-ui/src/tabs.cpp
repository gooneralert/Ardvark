#include "pch.h"
#include "tabs.h"
#include "widgets.h"

#include "app/Settings.h"
#include "gui/gui.h"

// The matcha-ui tabs, wired to Ardvark's settings. The layout, panels and
// widgets are matcha's; only the values they read/write are ours.

using Settings = Cheat::Settings;
using Cheat::g_Settings;

namespace
{
    // Ardvark stores colours as float[4]; matcha's picker wants an ImVec4&.
    inline ImVec4& rgba(float (&c)[4])
    {
        return *reinterpret_cast<ImVec4*>(c);
    }

    // Triggerbot checks are bitflags; expose each one as its own toggle.
    void check_flag(int& mask, int bit, const char* label, const char* tip = nullptr)
    {
        bool on = (mask & bit) != 0;
        if (ui::checkbox(label, on))
        {
            if (on) mask |= bit;
            else    mask &= ~bit;
        }
        if (tip)
            ui::help(tip);
    }

    constexpr const char* aim_types[]        = { "Mouse", "Camera", "Silent" };
    constexpr const char* bind_modes[]       = { "Hold", "Toggle", "Always" };
    constexpr const char* hold_toggle[]      = { "Hold", "Toggle" };
    constexpr const char* silent_methods[]   = { "Viewport", "Mouse", "Raycast", "Magic Bullet", "Phantom" };
    constexpr const char* part_selects[]     = { "Cycle", "Random", "Closest" };
    constexpr const char* target_selects[]   = { "FOV Center", "Distance", "Lowest HP" };
    constexpr const char* curves[]           = { "Linear", "Exponential", "Spring", "Bezier" };
    constexpr const char* trigger_modes[]    = { "Hold", "Toggle", "Always", "Once" };
    constexpr const char* trigger_hitboxes[] = { "Whole Body", "Head", "Torso", "Arms", "Legs", "HRP" };

    constexpr const char* box_types[]        = { "2D", "Corner", "3D" };
    constexpr const char* name_types[]       = { "Name", "Display Name" };
    constexpr const char* sizing[]           = { "Bounding", "Dynamic" };
    constexpr const char* sides[]            = { "Top", "Bottom", "Left", "Right" };
    constexpr const char* origins[]          = { "Bottom", "Center", "Mouse", "Top" };
    constexpr const char* tracer_types[]     = { "Line", "Spider" };
    constexpr const char* skeleton_types[]   = { "Funny", "Anton", "Unfunny", "Egor" };
    constexpr const char* chams_modes[]      = { "Box", "Filled", "Clipper", "Shader", "Mesh" };
    constexpr const char* engine_chams[]     = { "Default", "Ghost", "Simple Wire", "Colored Frame", "Colored", "Smoke No Shadow", "Smoke" };

    constexpr const char* tonemappers[]      = { "Default", "Futuristic" };
    constexpr const char* skybox_modes[]     = { "Shader", "Roblox" };

    constexpr const char* hitbox_parts[]     = { "Head", "HRP", "Torso", "Arms", "Legs" };
    constexpr const char* bt_tools[]         = { "Hammer", "Grab", "Clone" };
    constexpr const char* anim_packs[]       = { "Default", "Zombie", "Ninja", "Levitate", "Astronaut" };

    constexpr const char* appearances[]      = { "Dark", "Light" };
    constexpr const char* densities[]        = { "Compact", "Default", "Spacious" };
    constexpr const char* presets[]          = { "Midnight", "Graphite", "Rose" };
}

void tabs::combat()
{
    Settings& S = g_Settings;
    auto aim = [&]() -> Settings::AimbotConfig& { return S.aim.active(); };

    static int aim_tab, silent_tab;

    constexpr const char* aim_tabs[]    = { "Aimbot", "Prediction", "Smoothness", "FOV" };
    constexpr const char* silent_tabs[] = { "Silent Aim", "Force Magic", "Misc" };

    ui::column(0);
    ui::begin_panel("aimbot", aim_tabs, aim_tab);
    if (aim_tab == 0)
    {
        ui::combo("Aim Type", S.aim.type, aim_types);
        ui::keybind(S.aim.bind);
        ui::combo("Bind Mode", S.aim.bind_mode, bind_modes);
        ui::checkbox("Visible Check", aim().visible_only);
        ui::checkbox("Dead Check", aim().dead_check);
        ui::checkbox("Distance Check", aim().distance_check);
        ui::slider("Distance", aim().max_distance, 0, 10000);
        ui::checkbox("Sticky Aim", aim().sticky);
        ui::checkbox("Humanize", aim().humanize);
        ui::slider("Reaction (ms)", aim().reaction_ms, 0, 500);
        ui::slider("Sensitivity", aim().aim_sensitivity, 0, 5, "%.2f");
        ui::combo("Hit Part", aim().part_select, part_selects);
        ui::combo("Target", aim().target_select, target_selects);
        ui::checkbox("Target Bots", S.aim.target_bots);
    }
    else if (aim_tab == 1)
    {
        ui::checkbox("Enabled", aim().prediction);
        ui::slider("Bullet Speed", aim().bullet_speed, 0, 5000);
    }
    else if (aim_tab == 2)
    {
        ui::checkbox("Enabled", aim().smooth_enabled);
        ui::slider("Smooth X", aim().smooth_x, 0, 10, "%.2f");
        ui::slider("Smooth Y", aim().smooth_y, 0, 10, "%.2f");
        ui::combo("Curve", aim().aim_curve, curves);
    }
    else
    {
        ui::checkbox("Show FOV", aim().fov_enabled);
        ui::color(rgba(aim().fov_color));
        ui::color(rgba(aim().fov_outline_color));
        ui::slider("Radius", aim().fov_size, 10, 800);
        ui::checkbox("Aim Tracer", aim().tracer);
        ui::color(rgba(aim().tracer_color));
    }
    ui::end_panel();

    ui::begin_panel("Trigger Bot");
    ui::checkbox("Enabled", S.triggerbot.enabled);
    ui::keybind(S.triggerbot.key);
    ui::combo("Key Mode", S.triggerbot.key_mode, trigger_modes);
    ui::slider("Delay (ms)", S.triggerbot.delay_ms, 0, 500);
    ui::slider("Release (ms)", S.triggerbot.click_duration_ms, 0, 500);
    ui::slider("Distance", S.triggerbot.max_distance, 0, 10000);
    ui::slider("Hitbox Mul", S.triggerbot.hitbox_multiplier, 1, 5, "%.2f");
    ui::combo("Hitbox", S.triggerbot.hitboxes, trigger_hitboxes);
    check_flag(S.triggerbot.checks, Settings::TRIGGER_CHECK_TEAM,    "Team Check");
    check_flag(S.triggerbot.checks, Settings::TRIGGER_CHECK_HEALTH,  "Health Check");
    check_flag(S.triggerbot.checks, Settings::TRIGGER_CHECK_VISIBLE, "Visible Check");
    check_flag(S.triggerbot.checks, Settings::TRIGGER_CHECK_FF,      "Forcefield Check");
    check_flag(S.triggerbot.checks, Settings::TRIGGER_CHECK_INVIS,   "Invisible Check");
    ui::checkbox("Prediction", S.triggerbot.prediction_enabled);
    ui::checkbox("Debug HUD", S.triggerbot.debug);
    ui::end_panel();

    ui::column(1);
    ui::begin_panel("silent", silent_tabs, silent_tab);
    if (silent_tab == 0)
    {
        ui::keybind(S.aim.silent_bind);
        ui::combo("Bind Mode", S.aim.silent_bind_mode, bind_modes);
        ui::combo("Method", S.aim.silent_method, silent_methods);
        ui::checkbox("Enabled", S.aim.silent_enabled);
        ui::checkbox("Visible Check", S.aim.silent.visible_only);
        ui::checkbox("Dead Check", S.aim.silent.dead_check);
        ui::checkbox("Distance Check", S.aim.silent.distance_check);
        ui::slider("Distance", S.aim.silent.max_distance, 0, 10000);
        ui::slider("Sensitivity", S.aim.silent.aim_sensitivity, 0, 5, "%.2f");
        ui::combo("Hit Part", S.aim.silent.part_select, part_selects);
        ui::combo("Target", S.aim.silent.target_select, target_selects);
        ui::checkbox("Target Bots", S.aim.target_bots);
    }
    else if (silent_tab == 1)
    {
        ui::checkbox("Force Magic Bullet", S.aim.force_magic_bullet);
        ui::keybind(S.aim.force_magic_key);
        ui::combo("Bind Mode", S.aim.force_magic_mode, bind_modes);
        ui::checkbox("Raycast Debug", S.aim.raycast_debug);
    }
    else
    {
        ui::checkbox("Hybrid Mode", S.misc.hybrid_mode, col::yellow);
        ui::help("Master gate for the easily-detectable features");
        ui::checkbox("Raycast Engine", S.misc.raycast_engine);
        ui::checkbox("Arsenal Flick Fix", S.misc.arsenal_flick_fix);
    }
    ui::end_panel();
}

void tabs::visuals()
{
    Settings& S = g_Settings;

    static int esp_tab, info_tab;

    constexpr const char* esp_tabs[]  = { "ESP", "Crosshair", "Misc", "Flags" };
    constexpr const char* info_tabs[] = { "Indicators", "OOF Arrow", "Radar" };

    ui::column(0);
    ui::begin_panel("esp", esp_tabs, esp_tab);
    if (esp_tab == 0)
    {
        ui::checkbox("Enabled", S.esp.enabled);
        ui::checkbox("Self ESP", S.esp.draw_local);
        ui::checkbox("Team Check", S.misc.teamcheck);
        ui::checkbox("Dead Check", S.esp.dead_check);
        ui::checkbox("Box", S.esp.box);
        ui::checkbox("Fill Box", S.esp.box_fill);
        ui::checkbox("Name", S.esp.name);
        ui::checkbox("Health Bar", S.esp.healthbar);
        ui::checkbox("Skeleton", S.esp.skeleton);
        ui::checkbox("Distance", S.esp.distance);
        ui::checkbox("Equipped Tool", S.esp.tool);
        ui::combo("Sizing Type", S.esp.bounding_type, sizing);
        ui::slider("Render Distance", S.esp.max_distance, 0, 30000);
    }
    else if (esp_tab == 1)
    {
        ui::checkbox("Enabled", S.crosshair.enabled);
        ui::color(rgba(S.crosshair.color));
        ui::checkbox("Center Dot", S.crosshair.dot);
        ui::slider("Size", S.crosshair.length, 2, 40, "%.1f");
        ui::slider("Gap", S.crosshair.gap, 0, 20, "%.1f");
        ui::slider("Thickness", S.crosshair.thickness, 1, 6, "%.2f");
        ui::checkbox("Outline", S.crosshair.outline);
        ui::color(rgba(S.crosshair.outline_color));
        ui::checkbox("Spin", S.crosshair.spin);
        ui::slider("Spin Speed", S.crosshair.spin_speed, 0, 720);
    }
    else if (esp_tab == 2)
    {
        ui::checkbox("Show Bots", S.esp.bots);
        ui::checkbox("Zombie / Custom Cache", S.esp.apocalypse);
        ui::checkbox("Corpses", S.esp.corpses);
        ui::checkbox("Ground Loot", S.esp.ground_loot);
        ui::checkbox("Containers", S.esp.containers);
        ui::checkbox("ESP Preview", S.misc.esp_preview);
    }
    else
    {
        ui::checkbox("Flags", S.esp.flags);
        ui::combo("Flags Side", S.esp.flags_side, sides);
        ui::slider("Offset", S.esp.flags_off, 0, 100);
    }
    ui::end_panel();

    ui::begin_panel("Box");
    ui::checkbox("Enabled", S.esp.box);
    ui::color(rgba(S.esp.box_fill_color));
    ui::color(rgba(S.esp.box_color));
    ui::checkbox("Fill Box", S.esp.box_fill);
    ui::combo("Box Type", S.esp.box_mode, box_types);
    ui::slider("Thickness", S.esp.box_thickness, 0.5f, 5, "%.1f");
    ui::end_panel();

    ui::begin_panel("Name");
    ui::checkbox("Enabled", S.esp.name);
    ui::color(rgba(S.esp.name_color));
    ui::combo("Type", S.esp.name_mode, name_types);
    ui::end_panel();

    ui::column(1);
    ui::begin_panel("info", info_tabs, info_tab);
    if (info_tab == 0)
    {
        ui::checkbox("Distance", S.esp.distance);
        ui::color(rgba(S.esp.distance_color));
        ui::checkbox("Equipped Item", S.esp.tool);
        ui::color(rgba(S.esp.tool_color));
        ui::checkbox("Skeleton", S.esp.skeleton);
        ui::color(rgba(S.esp.skeleton_color));
        ui::combo("Skeleton Type", S.esp.skeleton_type, skeleton_types);
        ui::checkbox("China Hat", S.esp.china_hat);
        ui::color(rgba(S.esp.china_hat_color));
    }
    else if (info_tab == 1)
    {
        ui::checkbox("Enabled", S.esp.offscreen_arrows);
        ui::color(rgba(S.esp.arrow_color));
        ui::slider("Radius", S.esp.arrow_radius, 50, 1000);
        ui::slider("Size", S.esp.arrow_size, 4, 64);
    }
    else
    {
        ui::checkbox("Hitmarker", S.hitmarker.enabled);
        ui::slider("Size", S.hitmarker.size, 0.2f, 3, "%.2f");
        ui::checkbox("Hit Data", S.hitdata.enabled);
        ui::slider("Duration", S.hitdata.duration, 0.2f, 5, "%.2f");
        ui::checkbox("Hit Sound", S.hitsound.enabled);
        ui::slider("Volume", S.hitsound.volume, 0, 100);
        ui::checkbox("Kill Effect", S.killfx.enabled);
        ui::slider("Effect", S.killfx.effect, 0, 8);
    }
    ui::end_panel();

    ui::begin_panel("Health");
    ui::checkbox("Health Bar", S.esp.healthbar);
    ui::checkbox("Health Text", S.esp.health_text);
    ui::combo("Text Pos", S.esp.health_text_side, sides);
    ui::combo("Bar Side", S.esp.healthbar_side, sides);
    ui::end_panel();

    ui::begin_panel("Chams");
    ui::combo("Mode", S.esp.chams_mode, chams_modes);
    ui::checkbox("Enabled", S.esp.chams);
    ui::color(rgba(S.esp.chams_outline_color));
    ui::color(rgba(S.esp.chams_fill_color));
    ui::checkbox("Engine Chams", S.esp.engine_chams);
    ui::combo("Engine Style", S.esp.engine_chams_style, engine_chams);
    ui::color(rgba(S.esp.engine_chams_color));
    ui::checkbox("Occluded Chams", S.esp.occluded_chams);
    ui::color(rgba(S.esp.mesh_chams_occluded_color));
    ui::color(rgba(S.esp.mesh_chams_occluded_outline_color));
    ui::checkbox("Hit Chams", S.esp.hit_chams);
    ui::color(rgba(S.esp.hit_chams_color));
    ui::end_panel();

    ui::begin_panel("Tracer");
    ui::checkbox("Enabled", S.esp.tracer);
    ui::color(rgba(S.esp.tracer_color));
    ui::combo("Origin", S.esp.tracer_origin, origins);
    ui::combo("Type", S.esp.tracer_type, tracer_types);
    ui::end_panel();
}

void tabs::world()
{
    Settings& S = g_Settings;

    ui::column(0);
    ui::begin_panel("Camera Modification");
    ui::checkbox("Camera Field Of View", S.misc.fov);
    ui::help("Overrides the camera's field of view");
    ui::slider("Amount", S.misc.fov_value, 1, 120);
    ui::checkbox("Unlock FPS", S.misc.fps_unlock);
    ui::slider("FPS Cap", S.misc.fps_cap, 30, 1000);
    ui::end_panel();

    ui::begin_panel("Freecam");
    ui::keybind(S.misc.freecam_key);
    ui::combo("Mode", S.misc.freecam_mode, hold_toggle);
    ui::slider("Speed", S.misc.freecam_speed, 0, 500);
    ui::slider("Sensitivity", S.misc.freecam_sens, 0, 2, "%.2f");
    ui::checkbox("Third Person", S.misc.third_person);
    ui::keybind(S.misc.third_person_key);
    ui::combo("TP Mode", S.misc.third_person_mode, hold_toggle);
    ui::slider("TP Distance", S.misc.third_person_distance, 0, 40, "%.1f");
    ui::end_panel();

    ui::column(1);
    ui::begin_panel("World Lighting");
    ui::checkbox("Ambience", S.world.ambient, col::yellow);
    ui::color(rgba(S.world.ambient_col));
    ui::checkbox("Outdoor Ambient", S.world.outdoor);
    ui::color(rgba(S.world.outdoor_col));
    ui::checkbox("Custom Fog", S.world.fog, col::red);
    ui::color(rgba(S.world.fog_color));
    ui::slider("Fog Start", S.world.fog_start, 0, 10000);
    ui::slider("Fog End", S.world.fog_end, 0, 10000);
    ui::checkbox("Custom Exposure", S.world.exposure_on, col::green);
    ui::slider("Exposure Compensation", S.world.exposure, -3, 3, "%.2f");
    ui::checkbox("Custom Brightness", S.world.brightness, col::yellow);
    ui::slider("Brightness", S.world.brightness_val, 0, 10, "%.2f");
    ui::checkbox("Custom Time", S.world.time_changer, col::green);
    ui::slider("Clock Time", S.world.clock_time, 0, 24, "%.2f");
    ui::end_panel();

    ui::begin_panel("Post FX");
    ui::checkbox("Bloom", S.world.bloom);
    ui::slider("Intensity", S.world.bloom_intensity, 0, 5, "%.2f");
    ui::slider("Size", S.world.bloom_size, 0, 100);
    ui::checkbox("Color Correction", S.world.color_corr);
    ui::slider("Brightness", S.world.cc_bri, -1, 1, "%.2f");
    ui::slider("Contrast", S.world.cc_con, -1, 1, "%.2f");
    ui::color(rgba(S.world.cc_tint));
    ui::checkbox("Color Grade", S.world.color_grade);
    ui::combo("Tonemapper", S.world.tonemapper, tonemappers);
    ui::checkbox("Depth Of Field", S.world.dof);
    ui::slider("Focus", S.world.dof_focus, 0, 500);
    ui::slider("Radius", S.world.dof_radius, 0, 200);
    ui::end_panel();

    ui::begin_panel("Sky & Atmosphere");
    ui::checkbox("Sky", S.world.sky);
    ui::slider("Sun Angular", S.world.sun_angular, 0, 60);
    ui::slider("Moon Angular", S.world.moon_angular, 0, 60);
    ui::checkbox("Skybox Changer", S.world.skybox_changer);
    ui::combo("Mode", S.world.skybox_mode, skybox_modes);
    ui::slider("Preset", S.world.skybox_preset, 0, 32);
    ui::checkbox("Atmosphere", S.world.atmosphere);
    ui::slider("Density", S.world.atmo_density, 0, 1, "%.3f");
    ui::slider("Haze", S.world.atmo_haze, 0, 5, "%.2f");
    ui::slider("Glare", S.world.atmo_glare, 0, 5, "%.2f");
    ui::color(rgba(S.world.atmo_color));
    ui::checkbox("Terrain", S.world.terrain);
    ui::slider("Grass Length", S.world.grass_len, 0, 1, "%.2f");
    ui::color(rgba(S.world.grass_col));
    ui::color(rgba(S.world.water_col));
    ui::end_panel();
}

void tabs::character()
{
    Settings& S = g_Settings;

    ui::column(0);
    ui::begin_panel("Hitbox Extender");
    ui::checkbox("Enabled", S.hitbox.enabled, col::red);
    ui::checkbox("Visualize Hitbox", S.hitbox.visualize);
    ui::color(rgba(S.hitbox.viz_color));
    ui::combo("Part", S.hitbox.part, hitbox_parts);
    ui::slider("Hitbox Size", S.hitbox.scale, 1, 50, "%.2f");
    ui::end_panel();

    ui::begin_panel("Desync");
    ui::checkbox("Enabled", S.misc.desync);
    ui::checkbox("Raycast Engine", S.misc.raycast_engine);
    ui::end_panel();

    ui::begin_panel("Misc");
    ui::checkbox("Team Check", S.misc.teamcheck);
    ui::checkbox("Arsenal Flick Fix", S.misc.arsenal_flick_fix);
    ui::checkbox("BTools", S.misc.bTools);
    ui::combo("Tool", S.misc.bToolsTool, bt_tools);
    ui::end_panel();

    ui::column(1);
    ui::begin_panel("Movement");
    ui::checkbox("No-Clip", S.misc.noclip);
    ui::checkbox("Inf Jump", S.misc.inf_jump);
    ui::help("Lets you jump again while in the air");
    ui::checkbox("Speed", S.misc.walkspeed);
    ui::keybind(S.misc.walkspeed_key);
    ui::combo("Key Mode", S.misc.walkspeed_key_mode, bind_modes);
    ui::slider("Speed Amount", S.misc.walkspeed_value, 16, 500);
    ui::checkbox("Flight", S.misc.fly);
    ui::keybind(S.misc.fly_key);
    ui::combo("Fly Mode", S.misc.fly_key_mode, bind_modes);
    ui::slider("Fly Speed", S.misc.fly_speed, 0, 500);
    ui::checkbox("Zero Gravity", S.misc.fly_gravity);
    ui::checkbox("Jump", S.misc.jump, col::yellow);
    ui::slider("Jump Power", S.misc.jump_power, 0, 1000);
    ui::checkbox("Gravity", S.misc.gravity);
    ui::slider("Gravity Value", S.misc.gravity_value, 0, 500, "%.1f");
    ui::end_panel();

    ui::begin_panel("Exploit");
    ui::checkbox("Tickrate", S.misc.tickrate);
    ui::slider("Ticks", S.misc.tickrate_value, 1, 1000);
    ui::checkbox("Animation Changer", S.misc.anim_changer);
    ui::combo("Anim Pack", S.misc.anim_pack, anim_packs);
    ui::checkbox("Fake Headless", S.misc.fake_headless);
    ui::checkbox("Korblox", S.misc.korblox);
    ui::end_panel();
}

void tabs::options()
{
    Settings& S = g_Settings;

    ui::column(0);
    ui::begin_panel("Theme");
    ui::row("Menu Key");
    {
        const int before = *gui::menu_key_ptr();
        ui::keybind(*gui::menu_key_ptr());
        if (*gui::menu_key_ptr() != before)
            *gui::menu_key_skip_ptr() = true; // don't let the new key toggle us
    }
    ui::row("Accent Color");
    ui::color(rgba(S.gui.accent));
    ui::combo("Preset", S.gui.theme, presets);
    ui::end_panel();

    ui::begin_panel("General");
    ui::checkbox("Vertical Sync", S.gui.vsync);
    ui::help("Caps the menu to your refresh rate");
    ui::checkbox("Hybrid Mode", S.misc.hybrid_mode, col::yellow);
    ui::help("Splits work between threads");
    ui::checkbox("Raycast Engine", S.misc.raycast_engine);
    ui::help("Uses raycasts for visibility checks");
    ui::checkbox("MCP Bridge", S.misc.mcp);
    ui::checkbox("Team Check", S.misc.teamcheck);
    ui::slider("FPS Cap", S.gui.fps_cap, 0, 2000);
    ui::end_panel();

    ui::begin_panel("User Interface");
    ui::combo("Appearance", S.gui.appearance, appearances);
    ui::combo("Density", S.gui.density, densities);
    ui::checkbox("Glass", S.gui.glass_on);
    ui::checkbox("Dim Backdrop", S.gui.dim_backdrop);
    ui::checkbox("Watermark", S.gui.watermark);
    ui::slider("Frost", S.gui.frost, 0, 1, "%.2f");
    ui::slider("Blur", S.gui.blur, 0, 100);
    ui::slider("Blur Refresh (Hz)", S.gui.blur_hz, 1, 240);
    ui::end_panel();

    ui::column(1);
    ui::begin_panel("Session");
    ui::center("user", col::bright);
    ui::badge("Pro");
    ui::spacing(6);
    ui::checkbox("Lua Executor", S.lua.executor);
    ui::checkbox("Players", S.misc.players);
    ui::checkbox("Explorer", S.misc.explorer);
    ui::checkbox("Servers", S.misc.servers);
    ui::checkbox("ESP Preview", S.misc.esp_preview);
    ui::checkbox("Music Player", S.misc.music);
    ui::checkbox("Internal Print", S.lua.internal_print);
    ui::help("Routes print() through the injected Roblox print (banable)");
    ui::slider("Lua Tick (ms)", S.lua.ticks_ms, 1, 15);
    ui::end_panel();

    ui::begin_panel("UI Elements");
    ui::checkbox("Watermark Build", S.gui.watermark_fields[Settings::WM_BUILD]);
    ui::checkbox("Watermark Player", S.gui.watermark_fields[Settings::WM_PLAYER]);
    ui::checkbox("Watermark Place", S.gui.watermark_fields[Settings::WM_PLACE_ID]);
    ui::checkbox("Watermark Game", S.gui.watermark_fields[Settings::WM_GAME_ID]);
    ui::checkbox("Watermark Time", S.gui.watermark_fields[Settings::WM_TIME]);
    ui::checkbox("Watermark FPS", S.gui.watermark_fields[Settings::WM_FPS]);
    ui::checkbox("Custom Support", S.misc.custom_support);
    ui::checkbox("Kill Effect", S.killfx.enabled);
    ui::end_panel();
}

void tabs::configs()
{
    static int script = -1, config = -1;
    static char name[32];
    constexpr const char* scripts[] = { "untitled" };
    constexpr const char* saved[] = { "default", "legit" };

    ui::column(0);
    ui::begin_panel("Scripts", 838);
    ui::button("Open Folder##scripts");
    ui::label("Script List");
    ui::list("##scripts", scripts, script, 290);
    ui::button("Load Script");
    ui::button("Set Auto Execute");
    ui::end_panel();

    ui::column(1);
    ui::begin_panel("Config", 838);
    ui::button("Open Folder##configs");
    ui::label("Config List");
    ui::list("##configs", saved, config, 272);
    ui::label("Config Name");
    ui::input("##name", name, sizeof(name));
    ui::buttons({ "Load", "Save" });
    ui::end_panel();
}

void tabs::npc()
{
    static int tab, npc = -1, config = -1;
    static char name[32];
    constexpr const char* npc_tabs[] = { "General", "Visuals" };

    ui::column(0);
    ui::begin_panel("NPC List", ui::fill_height);
    ui::buttons({ "+ Model", "+ Directory" });
    ui::list("##npcs", {}, npc, -14);
    ui::end_panel();

    ui::column(1);
    ui::begin_panel("npc", npc_tabs, tab);
    ui::spacing(27);
    ui::center("Select an NPC to edit");
    ui::spacing(25);
    ui::end_panel(13);

    ui::begin_panel("Config##npc");
    ui::input("##npc_config", name, sizeof(name));
    ui::buttons({ "Load", "Save", "Folder" });
    ui::list("##npc_configs", {}, config, 95);
    ui::end_panel();
}

void tabs::teams()
{
    Settings& S = g_Settings;

    static char search[32];

    ui::column(0);
    ui::begin_panel("Teams [0]", ui::fill_height);
    ui::input("##search", search, sizeof(search), "Search team...");
    ui::begin_box("##teams", -14);
    ui::checkbox("Use Custom Teams", S.misc.teamcheck);
    ui::checkbox("Auto Detect Allied Team", S.esp.apocalypse);
    ui::label("Team", col::dim);
    ui::value("Status", col::dim);
    ui::separator();
    ui::spacing(14);
    ui::center("No teams in this game");
    ui::end_box();
    ui::end_panel();

    ui::column(1);
    ui::begin_panel("Editor");
    ui::spacing(26);
    ui::center("Select a team to edit");
    ui::spacing(26);
    ui::end_panel(13);

    ui::begin_panel("Quick Actions");
    ui::buttons({ "All Enemy", "All Ally" });
    ui::button("Reset to Default");
    ui::end_panel();
}
