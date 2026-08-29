#pragma once

#include <vector>

namespace Cheat {

    struct Settings {
        enum ArrowInfo {
            ARROW_NAME = 0,
            ARROW_DISTANCE,
            ARROW_HEALTH,
            ARROW_TOOL,
            ARROW_INFO_COUNT
        };

        enum EspOutline {
            OUTLINE_SKELETON = 0,
            OUTLINE_BOX,
            ESP_OUTLINE_COUNT
        };

        enum LootFilter {
            LOOT_WEAPONS = 0,
            LOOT_MAGS,
            LOOT_AMMO,
            LOOT_ATTACHMENTS,
            LOOT_MEDICAL,
            LOOT_VALUABLES,
            LOOT_TOOLS,
            LOOT_ELECTRONICS,
            LOOT_HOUSEHOLDS,
            LOOT_DOCUMENTS,
            LOOT_OTHER,
            LOOT_FILTER_COUNT
        };

        enum BotEspElem {
            BOT_BOX = 0,
            BOT_NAME,
            BOT_SKELETON,
            BOT_CHAMS,
            BOT_HEALTHBAR,
            BOT_HEALTH_TEXT,
            BOT_DISTANCE,
            BOT_TOOL,
            BOT_FLAGS,
            BOT_ESP_COUNT
        };

        struct {
            bool enabled{ false };
            bool draw_local{ false };
            bool box{ false };
            bool box_fill{ false };
            int  box_fill_mode{ 0 };   // 0 color, 1 image
            int  box_fill_image{ 0 };  // BoxFill::ImageId 0..20
            float box_fill_image_alpha{ 0.5f };
            bool box_fill_remove_bg{ false };
            float box_fill_color[4]{ 1.f, 1.f, 1.f, 0.2f };
            bool name{ false };
            bool skeleton{ false };
            // 0 funny (линии) / 1 anton (SK) / 2 unfunny (US) / 3 egor (SE)
            int  skeleton_type{ 0 };
            bool chams{ false };

            // 0 box, 1 filled, 2 clipper, 3 shader, 4 mesh
            // engine — отдельный чекбокс (комбо с mesh/shader/…)
            int  chams_mode{ 0 };
            int  chams_shader{ 0 };
            bool engine_chams{ false };
            // mesh chams: 0 flat (ImGui / dx flat) / 1 dx shader
            int  mesh_chams_style{ 0 };
            // 0 flat 1 chrome 2 rainbow 3 pearl 4 glossy 5 holographic
            // 6 fade 7 wireframe 8 glass 9 ropes 10 liquid metal
            int  mesh_chams_dx_mode{ 4 };
            // world-depth occluded (отдельный чекбокс)
            bool  mesh_chams_occlusion{ false };
            // master "always visible" toggle — forces mesh + occlusion on
            bool  occluded_chams{ false };
            // тот же список mode, что mesh_chams_dx_mode
            int   mesh_chams_occluded_dx_mode{ 0 };
            // перезаписывают шейдер: fill + outline/fresnel
            float mesh_chams_occluded_color[4]{ 1.f, 0.f, 0.f, 1.f };       // red
            float mesh_chams_occluded_outline_color[4]{ 1.f, 0.f, 0.f, 1.f }; // red
            // контур-силуэт (faded glow) поверх mesh chams
            bool  mesh_chams_outline{ false };
            // 0 soft fade 1 pulse 2 flow 3 neon wave
            int   mesh_chams_outline_style{ 0 };
            float mesh_chams_outline_fade{ 1.8f }; // сила/ширина фейда
            float mesh_chams_outline_color[4]{ 1.f, 1.f, 1.f, 1.f };
            // 0 default 1 ghost 2 simple wire 3 colored frame 4 colored 5 smoke no shadow 6 smoke
            int  engine_chams_style{ 0 };
            // colored / colored frame — старая палитра Param
            // 0 red 1 green 2 orange 3 blue 4 pink 5 cyan 6 white
            int  engine_ghost_color_idx{ 6 };
            // ghost / simple wire / smoke / smoke no shadow — picker
            float engine_chams_color[4]{ 1.f, 1.f, 1.f, 1.f };
            bool preview{ false }; // окно превью рядом с меню, тумблер в settings

            bool healthbar{ false };
            bool health_text{ false };
            bool distance{ false };
            bool tool{ false };
            bool flags{ false };

            bool  tracer{ false };
            int   tracer_origin{ 0 }; // 0 bottom, 1 center, 2 mouse, 3 top
            int   tracer_type{ 0 };   // 0 line, 1 spider
            float tracer_color[4]{ 1.f, 1.f, 1.f, 0.85f };

            // china hat (layuh): 3d-конус над головой
            bool  china_hat{ false };
            bool  china_hat_target_only{ false };
            float china_hat_color[4]{ 1.f, 0.843f, 0.f, 1.f };

            // hit flash на clipper/shader/mesh
            bool  hit_chams{ false };
            float hit_chams_color[4]{ 1.f, 1.f, 1.f, 0.85f };
            float hit_chams_duration{ 0.55f };

            bool  offscreen_arrows{ false };
            float arrow_size{ 14.f };
            float arrow_radius{ 200.f };
            float arrow_color[4]{ 1.f, 1.f, 1.f, 0.95f };
            bool  arrow_info[ARROW_INFO_COUNT]{ true, false, false, false };

            // стороны элементов: 0 top 1 bottom 2 left 3 right
            // *_off слот на стороне, всегда без дыр
            int   name_side{ 0 };
            float name_off{ 0.f };
            int   distance_side{ 1 };
            float distance_off{ 0.f };
            int   tool_side{ 1 };
            float tool_off{ 0.f };
            int   flags_side{ 3 };
            float flags_off{ 0.f };
            int   health_text_side{ 2 };
            float health_text_off{ 0.f };
            int   healthbar_side{ 2 }; // только left/right

            int   font{ 0 }; // proxima soft bold
            float font_size{ 13.f };
            int   box_mode{ 0 };
            // 0 parts (тело) 1 mesh (все меши + аксы)
            int   bounding_type{ 0 };
            // outline: skeleton / box
            bool  esp_outline[ESP_OUTLINE_COUNT]{ true, true };
            float skeleton_thickness{ 1.f };
            float box_thickness{ 1.f };
            int   name_mode{ 0 };
            int   distance_unit{ 0 };
            bool  distance_check{ false };
            float max_distance{ 1000.f };

            bool  dead_check{ false };
            bool  body_corpse{ false };
            float corpse_color[4]{ 1.f, 1.f, 1.f, 0.90f };

            bool  bots{ true };
            float bot_max_distance{ 300.f }; // havoc bots, метры
            bool  apocalypse{ true }; // AR place: зомби/кастомный кэш
            bool  corpses{ false };
            bool  ground_loot{ false };
            bool  containers{ false };
            float ground_loot_color[4]{ 1.f, 1.f, 1.f, 0.95f };
            float containers_color[4]{ 1.f, 1.f, 1.f, 0.95f };

            bool  loot_chams{ false };
            bool  containers_chams{ false };
            int   loot_chams_shader{ 6 };
            int   containers_chams_shader{ 0 };

            bool  loot_filter[LOOT_FILTER_COUNT]{
                true, true, true, true, true, true, true,
                false, false, false, false
            };

            bool  bot_esp[BOT_ESP_COUNT]{
                // box name skel chams hpbar hptext dist tool flags
                true, true, true, false, true, false, true, true, false
            };
            int   bot_chams_mode{ 3 };   // shader
            int   bot_chams_shader{ 6 }; // gold
            float bot_box_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float bot_name_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float bot_skeleton_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float bot_chams_outline_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float bot_chams_fill_color[4]{ 1.f, 1.f, 1.f, 0.45f };
            float bot_distance_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float bot_tool_color[4]{ 1.f, 1.f, 1.f, 1.f };

            float box_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float name_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float skeleton_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float chams_outline_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float chams_fill_color[4]{ 1.f, 1.f, 1.f, 0.4f };
            float distance_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float tool_color[4]{ 1.f, 1.f, 1.f, 1.f };
        } esp;

        enum AimPart {
            AIM_HEAD = 0,
            AIM_UPPER_TORSO,
            AIM_LOWER_TORSO,
            AIM_HRP,
            AIM_LEFT_HAND,
            AIM_RIGHT_HAND,
            AIM_LEFT_FOOT,
            AIM_RIGHT_FOOT,
            AIM_PART_COUNT
        };

        enum AimPartTier {
            PART_OFF = 0,
            PART_PRIMARY = 1,
            PART_SECONDARY = 2,
            PART_TERTIARY = 3
        };

        enum AimPartSelect {
            PART_SELECT_CYCLE = 0,
            PART_SELECT_RANDOM = 1,
            PART_SELECT_CLOSEST = 2
        };

        enum TargetSelect {
            TARGET_FOV_CENTER = 0, // ближайший к центру фова
            TARGET_DISTANCE = 1,   // ближайший по 3d
            TARGET_LOWEST_HP = 2
        };

        struct AimbotConfig {
            bool  fov_enabled{ false };
            float fov_size{ 120.0f };
            int   fov_position{ 0 };
            int   fov_style{ 0 };
            bool  fov_outline{ true };
            bool  distance_check{ false };
            float max_distance{ 1000.0f };
            bool  visible_only{ false };
            bool  dead_check{ false }; // мёртвых скип

            bool  parts[AIM_PART_COUNT]{ false, false, false, false,
                                         false, false, false, false };
            int   part_tier[AIM_PART_COUNT]{ 0, 0, 0, 0, 0, 0, 0, 0 };
            int   part_select{ PART_SELECT_CLOSEST };
            int   target_select{ TARGET_FOV_CENTER };
            float switch_time{ 0.40f };

            bool  hitchance_enabled{ false };
            float hitchance{ 100.0f };

            float smooth_x{ 1.0f };
            float smooth_y{ 1.0f };
            bool  smooth_enabled{ false }; // layuh smoothing toggle

            // кривая замедления (aim mouse): 0 linear, 1 exponential (default), 2 spring, 3 bezier
            int   aim_curve{ 1 };
            float aim_sensitivity{ 1.0f }; // множитель скорости довода

            bool  humanize{ false };
            float reaction_ms{ 60.0f };
            bool  sticky{ false };
            float sticky_fov_scale{ 1.5f };

            bool  prediction{ false };
            float bullet_speed{ 1200.f }; // studs/sec; 0 = без лида

            float fov_color[4]{ 1.f, 1.f, 1.f, 0.86f };
            float fov_outline_color[4]{ 1.f, 1.f, 1.f, 0.78f };

            // линия до залоченной парты
            bool  tracer{ true };
            float tracer_color[4]{ 1.f, 1.f, 1.f, 0.9f };

            void SyncPartsFromTiers()
            {
                for (int i = 0; i < AIM_PART_COUNT; ++i)
                    parts[i] = part_tier[i] != PART_OFF;
            }

            void SyncTiersFromParts()
            {
                for (int i = 0; i < AIM_PART_COUNT; ++i)
                {
                    if (parts[i] && part_tier[i] == PART_OFF)
                        part_tier[i] = PART_PRIMARY;

                    else if (!parts[i])
                        part_tier[i] = PART_OFF;
                }
            }
        };

        enum SilentMethod {
            SILENT_VIEWPORT = 0,
            SILENT_MOUSE,
            SILENT_RAYCAST,
            SILENT_MAGIC_BULLET,
            SILENT_PHANTOM, // custom PF silent (impl later)
            SILENT_METHOD_COUNT
        };

        // ---- triggerbot (ported from gelato) ----
        // какая часть тела является целью
        // 0 = всё тело (whole body), дальше конкретные группы
        enum TriggerHitbox {
            TRIGGER_HITBOX_WHOLE = 0,
            TRIGGER_HITBOX_HEAD,
            TRIGGER_HITBOX_TORSO,
            TRIGGER_HITBOX_ARMS,
            TRIGGER_HITBOX_LEGS,
            TRIGGER_HITBOX_HRP,
        };

        // check bitflags, совпадают с gelato triggerbot.settings.checks
        enum TriggerCheck {
            TRIGGER_CHECK_TEAM = 1,      // скип тиммейтов
            TRIGGER_CHECK_HEALTH = 2,    // скип мёртвых
            TRIGGER_CHECK_FF = 4,        // скип форсфилдов
            TRIGGER_CHECK_INVIS = 8,     // скип невидимок
            TRIGGER_CHECK_VISIBLE = 16,  // только видимые (wallcheck)
        };

        struct TriggerbotConfig {
            bool  enabled{ false };
            int   key{ 0x06 };       // XBUTTON2 как в gelato
            int   key_mode{ 0 };     // 0 hold, 1 toggle, 2 always, 3 once

            float delay_ms{ 5.0f };
            float click_duration_ms{ 50.0f };
            float hitchance{ 100.0f };

            float max_distance{ 1000.0f };

            bool  prediction_enabled{ false };
            float prediction_xz{ 1.0f };
            float prediction_y{ 1.01f };
            bool  draw_prediction{ false };

            int hitboxes{ TRIGGER_HITBOX_WHOLE };
            // визуальный множитель триггер-бокса (не трогает реальные парты)
            float hitbox_multiplier{ 1.10f };
            int checks{ 0 };
            // PF-specific: точные парты под курсором (как pf_part_under_cursor)
            bool  pf_parts{ true };

            // on-screen debug HUD (для отладки, показывает курсор/статусы)
            bool  debug{ false };
        };

        struct {
            int  bind{ 0 };
            int  bind_mode{ 0 };
            int  type{ 0 }; // 0 mouse, 1 camera, 2 off (без аимбота, silent отдельно)
            int  silent_method{ SILENT_RAYCAST };
            bool silent_enabled{ true }; // всегда on, рубит silent key
            int  silent_bind{ 0 }; // 0 = нет бинда — silent выключен
            int  silent_bind_mode{ 0 };

            bool force_magic_bullet{ false };
            int  force_magic_key{ 0 };
            int  force_magic_mode{ 0 };
            bool raycast_debug{ false }; // [raycast] stage-by-stage logs in the console
            bool target_bots{ true }; // havoc нпц из кэша

            AimbotConfig mouse;
            AimbotConfig camera;
            AimbotConfig silent;

            AimbotConfig& active()
            {
                if (type == 1)
                    return camera;

                else if (type == 2)
                    return silent;

                return mouse;
            }

            bool silent_raycast() const {
                return silent_method == SILENT_RAYCAST;
            }
            bool silent_magic() const {
                return silent_method == SILENT_MAGIC_BULLET;
            }

            bool silent_uses_raycast_hook() const {
                return silent_raycast() || silent_magic();
            }
        } aim;

        TriggerbotConfig triggerbot;

        struct {
            bool  no_shadow{ false };

            bool  fog{ false };
            float fog_start{ 0.f };
            float fog_end{ 2000.f };
            float fog_color[4]{ 1.f, 1.f, 1.f, 1.f };
            // clock+sun+moon+grad отдельно от amb/bri/light
            bool  time_changer{ false };
            float clock_time{ 14.f }; // 0..24

            bool  ambient{ false };
            float ambient_col[4]{ 1.f, 1.f, 1.f, 1.f };
            bool  outdoor{ false };
            float outdoor_col[4]{ 1.f, 1.f, 1.f, 1.f };
            bool  brightness{ false };
            float brightness_val{ 2.f };
            bool  exposure_on{ false };
            float exposure{ 0.f };
            bool  light{ false };
            float light_col[4]{ 1.f, 1.f, 1.f, 1.f };
            float light_dir[3]{ 0.f, -1.f, 0.f };
            bool  time_manual{ false }; // старый конфиг, не юзаем

            bool  env{ false };
            float env_diffuse{ 1.f };
            float env_specular{ 1.f };

            bool  color_shift{ false };
            float shift_top[4]{ 1.f, 1.f, 1.f, 1.f };
            float shift_bot[4]{ 1.f, 1.f, 1.f, 1.f };

            // 11-14 atmosphere
            bool  atmosphere{ false };
            float atmo_density{ 0.3f };
            float atmo_haze{ 0.f };
            float atmo_glare{ 0.f };
            float atmo_offset{ 0.25f };
            float atmo_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float atmo_decay[4]{ 1.f, 1.f, 1.f, 1.f };

            // sky цепочка (Lighting.Sky) — один тоггл
            bool  sky{ false };
            float sun_angular{ 21.f };
            float moon_angular{ 11.f };
            float sky_orient_xyz[3]{ 0.f, 0.f, 0.f };
            bool  skybox_changer{ false };
            int   skybox_mode{ 1 }; // 0 shader 1 roblox
            int   skybox_shader{ 0 }; // заглушка, ещё не реализовано
            int   skybox_preset{ 0 };

            // 20-24 postfx
            bool  bloom{ false };
            float bloom_intensity{ 1.f };
            float bloom_size{ 24.f };
            float bloom_threshold{ 0.95f };

            bool  color_corr{ false };
            float cc_bri{ 0.f };
            float cc_con{ 0.f };
            float cc_tint[4]{ 1.f, 1.f, 1.f, 1.f };

            bool  color_grade{ false };
            int   tonemapper{ 0 }; // 0 Default 1 Futuristic

            bool  dof{ false };
            float dof_far{ 0.1f };
            float dof_near{ 0.1f };
            float dof_focus{ 20.f };
            float dof_radius{ 50.f };

            // terrain цепочка (Workspace.Terrain) — один тоггл
            bool  terrain{ false };
            float grass_len{ 0.2f };
            float grass_col[4]{ 1.f, 1.f, 1.f, 1.f };
            float water_col[4]{ 1.f, 1.f, 1.f, 1.f };
            float water_refl{ 1.f };
            float water_trans{ 0.3f };
        } world;

        struct {
            bool enabled{ false };
            int  effect{ 0 };
        } killfx;

        struct {
            bool  enabled{ false }; // всегда на мыши, вместо виндового курсора
            float length{ 8.f };
            float gap{ 4.f };
            float thickness{ 1.5f };
            bool  spin{ false };
            float spin_speed{ 90.f }; // deg/sec
            bool  outline{ true };
            bool  dot{ false };
            float dot_size{ 1.5f };
            float color[4]{ 1.f, 1.f, 1.f, 1.f };
            float outline_color[4]{ 1.f, 1.f, 1.f, 0.85f };
        } crosshair;

        struct {
            bool  enabled{ false };
            float size{ 1.0f };
            float duration{ 0.55f };
        } hitmarker;

        struct {
            bool  enabled{ false };
            int   index{ 0 };
            float volume{ 100.0f };
        } hitsound;

        enum HitDataMode {
            HITDATA_TYPE = 0,
            HITDATA_DAMAGE,
            HITDATA_HEALTH,
            HITDATA_DISTANCE,
            HITDATA_PART,
            HITDATA_MODE_COUNT
        };
        struct {
            bool  enabled{ false };
            bool  modes[HITDATA_MODE_COUNT]{ true, false, false, false, false };
            float duration{ 1.35f };
            float size{ 15.0f };
        } hitdata;

        enum HitboxPart {
            HB_HEAD = 0,
            HB_HRP,
            HB_TORSO,
            HB_ARMS,
            HB_LEGS,
            HB_PART_COUNT
        };
        struct {
            bool  enabled{ false };
            bool  visualize{ false };
            int   viz_mode{ 0 }; // 0 2d, 1 3d, 2 3d filled
            float viz_color[4]{ 1.f, 1.f, 1.f, 0.55f };
            int   part{ HB_HEAD }; // какой парт раздуваем
            float scale{ 2.f };
        } hitbox;

        struct {
            bool  fps_unlock{ false }; int   fps_cap{ 240 };
            bool  fov{ false };        float fov_value{ 70.f };
            bool  walkspeed{ false };  int   walkspeed_mode{ 0 };
            float walkspeed_value{ 32.f };
            int   walkspeed_key{ 0 };
            int   walkspeed_key_mode{ 0 };
            bool  jump{ false };       float jump_power{ 50.f };
            bool  fly{ false };
            float fly_speed{ 60.f };
            int   fly_key{ 0 };
            int   fly_key_mode{ 0 };
            bool  fly_gravity{ true };  // fly zeroes workspace gravity while engaged

            bool  gravity{ false };
            float gravity_value{ 196.2f };

            bool  tickrate{ false };
            float tickrate_value{ 60.f };

            bool  anim_changer{ false };
            int   anim_pack{ 0 };
            bool  fake_headless{ false };
            bool  reset_fake_headless{ false };
            bool  korblox{ false };
            bool  reset_korblox{ false };

            bool  explorer{ false };
            bool  esp_preview{ true };   // ESP preview is on by default
            bool  players{ false }; // island -> float players_ui
            bool  servers{ false }; // navbar -> server explorer window
bool  music{ false };   // music player window
            bool  mcp{ false }; // localhost bridge для cursor mcp
            bool  custom_support{ false };
            // hybrid mode: master gate for the easily-detectable features
            // (raycast / magic bullet silent, force magic bullet, internal
            // print, lua call gate). hidden + unusable while off.
            bool  hybrid_mode{ false };

            int   freecam_key{ 0 };
            int   freecam_mode{ 0 };
            float freecam_speed{ 60.f };
            float freecam_sens{ 0.30f };

            bool  third_person{ false };
            int   third_person_key{ 0 };
            int   third_person_mode{ 0 }; // 0 hold, 1 toggle
            float third_person_distance{ 12.f };

            bool  noclip{ false };
            bool  inf_jump{ false };

            bool  bTools{ false };
            int   bToolsTool{ 0 }; // 0 Hammer, 1 Grab, 2 Clone

            bool  teamcheck{ false };
            bool  raycast_engine{ false };

            bool  arsenal_flick_fix{ false }; // layuh anti-flick (Arsenal)
            bool  desync{ false };            // layuh firewall desync
        } misc;

        struct {
            bool executor{ false };   // отдельное окно lua
            bool internal_print{ false }; // "internal print - BANABLE": route print() to the
                                         // shellcode-injected Roblox print instead of the log window
            float ticks_ms{ 15.f };   // lua coroutine ticker interval (1..15 ms, 60fps default)
        } lua;

        enum {
            WM_BUILD = 0,
            WM_PLAYER,
            WM_PLACE_ID,
            WM_GAME_ID,
            WM_TIME,
            WM_FPS,
            WM_FIELD_COUNT
        };

        struct {
            int   theme{ 0 };
            int   font{ 0 }; // proxima soft bold
            float frost{ 0.10f };  // glass milkiness 0..1 (tint + white wash, 0 = clear)
            float blur{ 10.f };   // glass blur strength 0..100 (default 10, like matcha)
            bool  watermark{ false };
            float watermark_x{ 10.f };
            float watermark_y{ 10.f };
            // build, player, place, game, time, fps
            bool  watermark_fields[WM_FIELD_COUNT]{ true, false, true, false, true, true };
            float accent[4]{ 51.f / 255.f, 122.f / 255.f, 231.f / 255.f, 1.f };
            float text_active[4]{ 1.f, 1.f, 1.f, 1.f };
            float text_inactive[4]{ 136.f / 255.f, 136.f / 255.f, 136.f / 255.f, 1.f };
            float outer_border[4]{ 0.f, 0.f, 0.f, 1.f };
            float inner_border[4]{ 31.f / 255.f, 30.f / 255.f, 31.f / 255.f, 1.f };
            float panel_fill[4]{ 17.f / 255.f, 17.f / 255.f, 16.f / 255.f, 1.f };
            float content_outer[4]{ 31.f / 255.f, 30.f / 255.f, 31.f / 255.f, 1.f };
            float content_inner[4]{ 0.f, 0.f, 0.f, 1.f };
            float content_fill[4]{ 21.f / 255.f, 21.f / 255.f, 20.f / 255.f, 1.f };
            float child_fill[4]{ 15.f / 255.f, 14.f / 255.f, 14.f / 255.f, 1.f };
        } gui;
    };

    inline Settings g_Settings;

    struct CustomVisuals {
        bool  box{ false };       float box_color[4]{ 1.f, 1.f, 1.f, 1.f };
        bool  filled{ false };    float fill_color[4]{ 1.f, 1.f, 1.f, 0.25f };
        bool  name{ false };      float name_color[4]{ 1.f, 1.f, 1.f, 1.f };
        bool  distance{ false };  float distance_color[4]{ 1.f, 1.f, 1.f, 1.f };
        bool  tracer{ false };    float tracer_color[4]{ 1.f, 1.f, 1.f, 1.f };
    };

    struct CustomTarget {
        char label[64]{ "" };
        int  kind{ 0 };
        int  resolve{ 0 };
        char query[128]{ "" };
        bool enabled{ false };
        CustomVisuals vis;
    };
    inline std::vector<CustomTarget> g_CustomTargets;

}
