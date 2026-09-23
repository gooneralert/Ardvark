#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "stubs.h"

// Global manual UI scaling — base 2560x1440, UIScale = ScreenWidth / 2560
// Height is not used for scale (prevents stretch). Safety: 0/blank → 1.0f
inline float UIScale{ 1.0f };
constexpr int BASE_RES_WIDTH  = 2560;
constexpr int BASE_RES_HEIGHT = 1440;

namespace settings
{

	namespace aimbot
	{
		inline bool enabled{ false };
		inline int keybind{ 0 };
		inline int activation_mode{ 1 };

		inline int mode{ 0 };

		inline int target_part{ 1 };
		inline bool air_part_enabled{ false };
		inline int air_part{ 1 };

		inline float fov{ 100.f };
		inline bool use_fov{ false };
		inline bool draw_fov{ false };
		inline float fov_circle_colour[4]{ 1.f, 1.f, 1.f, 1.f };
		inline float fov_outline_colour[4]{ 0.f, 0.f, 0.f, 1.f };
		inline bool fov_circle_rainbow{ false };
		inline float fov_circle_rainbow_speed{ 1.0f };
		inline bool fov_glow{ false };
		inline bool fov_filled{ false };
		inline int fov_style{ 0 };

		// --- Standard smoothing (mouse movement interpolation) ---
		inline bool smoothing{ true };
		inline float smoothingx{ 7.f };
		inline float smoothingy{ 8.f };
		inline int smoothing_style{ 3 };
		inline float roblox_sensitivity{ 0.5f };

		// --- Humanized aimbot (legacy — kept for binary compat, no longer shown in UI) ---
		inline bool humanize{ false };
		inline float humanize_deadzone{ 3.5f };
		inline float humanize_speed{ 5.5f };
		inline float humanize_smoothx{ 7.f };
		inline float humanize_smoothy{ 8.f };

		// --- Gelato-style humanize params (active when smoothing_style == 13) ---
		inline float humanize_reaction_ms{ 80.f };   // reaction ramp time (ms) before aim accelerates
		inline float humanize_fatigue{ 0.4f };        // 0-1: how much aim weakens over hold duration
		inline float humanize_strength{ 1.0f };       // 0-3: per-frame scale+noise jitter magnitude

		inline bool enable_prediction{ false };
		inline float prediction_x{ 10.f };
		inline float prediction_y{ 10.f };

		inline bool air_prediction_enabled{ false };
		inline float air_prediction_x{ 10.f };
		inline float air_prediction_y{ 10.f };

		// --- Closet / legit features ---
		inline bool teamcheck{ false };
		inline bool knock_check{ false };
		inline bool sticky_aim{ false };

		inline bool health_check_enabled{ false };
		inline float min_health{ 0.0f };

		inline bool offset_enabled{ false };
		inline float offset_x{ 0.0f };
		inline float offset_y{ 0.0f };

		// Max world-distance cutoff (studs). 0 = disabled.
		inline bool max_range_enabled{ false };
		inline float max_range{ 500.f };

		// Only aim at visible (unobstructed) targets — uses wallcheck
		inline bool visibility_check{ false };

		// Minimum time (ms) the bot must stay on a target before it can switch to another
		inline float target_switch_delay_ms{ 0.f };

		// Smooth recoil compensation (counters recoil while holding the trigger)
		inline bool recoil_compensation{ false };
		inline float recoil_comp_x{ 0.f };   // pixels/s upward push
		inline float recoil_comp_y{ 5.f };

		inline bool rage_method{ false };
		inline int  type{ 1 }; // 0=Mouse 1=Camera Teleport
		inline bool resolver{ false };
	}

	namespace rage
	{
		inline bool hitsounds{ false };
		inline int hitsound_type{ 0 };
		inline int hitsound_method{ 0 };
		inline bool rapidfire{ false };
		inline bool noclip{ false };
		inline bool hit_tracers{ false };
		inline float hit_tracers_color[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
		inline float hit_tracers_duration{ 1.0f };

		namespace hipheight
		{
			inline bool enabled{ false };
			inline float height{ 2.0f };
		}

		namespace hitbox_expander
		{
			inline bool enabled{ false };
			// 0 = HumanoidRootPart, 1 = Head, 2 = Torso, 3 = All body parts, 4 = Mixed (rotates randomly)
			inline int target_part{ 0 };
			inline float size_x{ 2.2f };
			inline float size_y{ 2.2f };
			inline float size_z{ 1.2f };
			inline bool knock_check{ false };
			inline int mixed_interval_ms{ 350 };  // how often Mixed rotates the expanded part
		}

		namespace spin360
		{
			inline bool enabled{ false };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
			inline float speed{ 180.0f };  // degrees per second, 1-720
		}

		// ── New rage features ────────────────────────────────────────────────
		namespace fake_lag
		{
			inline bool enabled{ false };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
			inline int packets{ 8 };        // how many ticks to skip (1-20)
		}

		namespace ghost_mode
		{
			inline bool enabled{ false };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
			// Makes the local player intangible (CanCollide=false on all parts)
		}

		namespace infinite_stamina
		{
			inline bool enabled{ false };
		}

		namespace super_punch
		{
			inline bool enabled{ false };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
			inline float force{ 500.0f };   // studs/sec fling force
		}

		namespace auto_sprint
		{
			inline bool enabled{ false };
		}

		namespace low_gravity_jump
		{
			// Combines gravity reduction + high jump for big air
			inline bool enabled{ false };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
			inline float gravity_multiplier{ 0.3f };   // fraction of normal gravity (0.05-1.0)
			inline float jump_boost{ 80.0f };           // extra jump power (50-200)
		}

		namespace fling
		{
			inline bool enabled{ false };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
			inline float force{ 1000.0f };
			// 0=All, 1=Nearest, 2=Selected
			inline int target_mode{ 1 };
		}

		namespace rapid_click
		{
			// Simulates click spam at configurable CPS
			inline bool enabled{ false };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
			inline float cps{ 20.0f };     // clicks per second (1-60)
		}

		namespace playerlist
		{
			inline bool enabled{ false };
			inline int selected_index{ -1 };
			inline std::uint64_t selected_address{ 0 };
			inline std::string selected_name{};

			inline bool is_spectating{ false };
			inline std::string spectate_target_name{};
			inline std::uint64_t original_camera_subject{ 0 };
			inline std::int32_t original_camera_type{ 0 };
			inline std::uint64_t target_address{ 0 };

			inline math::vector3 saved_position{};
			inline bool has_saved_position{ false };

			inline std::unordered_set<std::string> whitelist{};
		}

		inline bool is_whitelisted(const std::string& name)
		{
			return playerlist::whitelist.count(name) > 0;
		}
	}

	namespace custom_entities
	{
		struct custom_entity_t
		{
			rbx::instance_t instance{};
			std::string name;
			std::string container_path;
			float distance = 0.f;
			bool enabled = true;
		};

		struct custom_container_t
		{
			std::string path;
			std::string name;
			bool enabled = true;
			std::vector<custom_entity_t> entities;
		};

		inline std::vector<custom_container_t> containers;
		inline std::string current_input = "Workspace.Bots";
		inline bool show_custom_entities = false;
		inline bool auto_refresh = false;
		inline float refresh_rate = 0.005f;
	}

	namespace silentaim
	{
		inline bool enabled{ false };
		inline int keybind{ 0 };
		inline int activation_mode{ 1 };

		inline int target_part{ 1 };

		inline float fov{ 100.f };
		inline bool use_fov{ false };
		inline bool draw_fov{ false };
		inline bool lerp_fov{ false };
		inline bool attach_fov_to_target{ false };
		inline float fov_circle_colour[4]{ 1.f, 1.f, 1.f, 1.f };
		inline float fov_outline_colour[4]{ 0.f, 0.f, 0.f, 1.f };
		inline bool fov_circle_rainbow{ false };
		inline float fov_circle_rainbow_speed{ 1.0f };
		inline bool fov_glow{ false };
		inline bool fov_filled{ false };
		inline int fov_style{ 0 };

		inline bool enable_prediction{ false };
		inline float prediction_x{ 10.f };
		inline float prediction_y{ 10.f };

		inline bool sticky_aim{ false };
		inline bool auto_switch{ false };
		inline bool spoof_mouse{ true };
		inline bool use_aimbot_target{ false };

		inline bool teamcheck{ false };
		inline bool guncheck{ false };
		inline bool knock_check{ false };

		inline int priorities{ 0 };
		inline bool health_check_enabled{ false };
		inline float min_health{ 0.0f };

		inline bool draw_target_dot{ false };
		inline float target_dot_color[4]{ 1.f, 0.f, 0.f, 1.f };
		inline float target_dot_size{ 4.0f };

		inline bool draw_snap_line{ false };
		inline float snap_line_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool use_raycast{ false };
		inline bool wallbang{ false };

		inline bool visibility_check{ false };
		inline float max_range{ 500.f };
		inline int method{ 0 }; // 0=Experimental

	}

	namespace visuals
	{
		inline bool radar_enabled{ false };
		inline float radar_size{ 0.f };

		inline bool enable_enemies{ false };
		inline bool enable_client{ false };

		// Gelato-style filters (applied per entity before drawing)
		inline bool filter_dead{ false };       // skip entities with health <= 0
		inline bool filter_invisible{ false };  // skip entities whose Head transparency >= 0.95

		inline bool box{ false };
		inline int box_type{ 0 };
		inline float box_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline bool box_fill{ false };
		inline float box_fill_color[4]{ 0.2f, 0.2f, 0.2f, 0.3f };
		// Gradient box fill
		inline bool  box_fill_gradient{ false };
		inline int   box_fill_type{ 1 };       // 0=horizontal, 1=vertical, 2=diagonal
		inline float box_fill_color_top[4]{ 0.2f, 0.2f, 0.2f, 0.3f };
		inline float box_fill_color_bottom[4]{ 0.f, 0.f, 0.f, 0.f };
		inline bool  box_fill_gradient_rotate{ false };
		inline float box_fill_speed{ 1.0f };

		// Bot (AI/NPC) override colors â€” used instead of enemy colors when entity.is_bot
		inline float bot_box_color[4]{ 1.f, 0.5f, 0.f, 1.f };       // orange
		inline float bot_box_fill_color[4]{ 1.f, 0.5f, 0.f, 0.15f };
		inline float bot_name_color[4]{ 1.f, 0.5f, 0.f, 1.f };

		inline bool name{ false };
		inline int name_type{ 0 };
		inline int name_display_type{ 0 };
		inline float name_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline float name_color_blend_start[4]{ 1.f, 1.f, 1.f, 1.f };
		inline float name_color_blend_end[4]{ 0.f, 0.f, 1.f, 1.f };
		inline bool blend{ false };
		inline bool avatar{ false };

		inline bool healthbar{ false };
		inline float healthbar_color[4]{ 0.f, 1.f, 0.f, 1.f };
		inline bool health_based_healthbar{ false };
		inline bool gradient_healthbar{ false };
		inline float gradient_healthbar_color_start[4]{ 1.f, 1.f, 1.f, 1.f };
		inline float gradient_healthbar_color_end[4]{ 0.f, 1.f, 0.f, 1.f };
		// Gelato health bar style: 0=static_color, 1=dynamic(ramps r/y/g), 2=gradient
		inline int  healthbar_style{ 2 };
		inline float healthbar_color_top[4]   { 30.f/255.f, 1.f, 60.f/255.f, 1.f };
		inline float healthbar_color_middle[4]{ 1.f, 140.f/255.f, 0.f,       1.f };
		inline float healthbar_color_bottom[4]{ 1.f, 25.f/255.f,  25.f/255.f, 1.f };
		inline bool health_percent{ false };
		inline float health_percent_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool armorbar{ false };
		inline float armorbar_color[4]{ 0.275f, 0.627f, 1.f, 1.f };

		inline bool distance{ false };
		inline int distance_measurement{ 0 };
		inline float distance_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool tool{ false };
		inline float tool_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline int esp_font{ 0 };
		inline bool local_player{ false };

		// Engine chams (FCE memory-write — jewsploit-style, 8 visual styles)
		// styles: 0=XRay, 1=Ghost, 2=Wireframe, 3=ColoredFrame, 4=Colored,
		//         5=SmokeNoShadow, 6=Smoke, 7=Invisible
		inline bool  engine_chams_enabled{ false };
		inline int   engine_chams_style{ 0 };
		// Color picker for Ghost/Wireframe/Smoke styles (RGBA, 0-1 range)
		inline float engine_chams_color[4]{ 1.f, 0.f, 0.f, 1.f };
		// Dropdown palette index for ColoredFrame/Colored styles
		// 0=red, 1=green, 2=orange, 3=blue, 4=pink, 5=cyan, 6=white
		inline int   engine_chams_dropdown_idx{ 3 };

		// World Render — the exact same per-part FastClusterEntity technique
		// as Engine/Mesh chams, but exposed as its own feature in the World
		// tab so the world's "textures" can be restyled independently.
		// Styles mirror chams_style in engine_chams.h
		// 0=XRay, 1=Ghost, 2=Wireframe, 3=ColoredFrame, 4=Colored,
		// 5=SmokeNoShadow, 6=Smoke, 7=Invisible
		inline bool  world_render_enabled{ false };
		inline int   world_render_style{ 2 }; // Wireframe by default
		inline float world_render_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline int   world_render_dropdown_idx{ 3 };

		// Skybox shader — replaces Roblox's sky pass pixel shader with a
		// procedural animated sky (D3D11 remote hook). Styles:
		// 0 = Kaleidoscope, 1 = Nebula (liquid plasma), 2 = Aurora curtains
		// (3 = video sky from the original source; needs the SkyVideo decoder,
		// which isn't in this kit, so it is intentionally not selectable)
		inline bool  skybox_enabled{ false };
		inline int   skybox_style{ 0 };
		inline bool  skybox_tint_enabled{ false };
		inline float skybox_tint[4]{ 1.f, 1.f, 1.f, 1.f };

		// Overlay GPU shader chams (MeshDxShader — flat/chrome/rainbow/etc.)
		inline bool shader_chams_enabled{ false };
		inline int  shader_chams_mode{ 0 }; // 0=flat … 23=magma, see MeshDxShader::ModeNames()

		// Mesh chams (memory_mesh_chams — gelato)
		inline bool mesh_chams_enabled{ false };
		// In-engine renderer placeholder (UI only in this kit).
		inline bool mesh_chams_internal{ false };
		inline int  mesh_chams_gpu_shader{ 1 };
		inline int  mesh_chams_mode{ 4 };
		inline bool mesh_chams_occlusion{ false };
		inline int  mesh_chams_occluded_mode{ 0 };
		inline bool mesh_chams_outline_enabled{ true };
		inline int  mesh_chams_outline_style{ 0 };
		inline int  mesh_chams_parsing_type{ 1 };
		inline float mesh_chams_fill_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline float mesh_chams_fresnel_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline float mesh_chams_occluded_color[4]{ 0.5f, 0.5f, 0.5f, 1.f };
		inline float mesh_chams_occluded_fresnel[4]{ 0.5f, 0.5f, 0.5f, 1.f };
		inline float mesh_chams_outline_color[4]{ 0.f, 0.f, 0.f, 1.f };
		inline float mesh_chams_fresnel_power{ 2.5f };
		inline float mesh_chams_outline_fade{ 1.8f };
		inline float mesh_chams_glow_strength{ 1.f };
		inline float mesh_chams_head_scale{ 1.f };
		inline float mesh_chams_client_head_scale{ 1.f };
		inline int  mesh_chams_dropdown{ 0 }; // new dropdown placeholder

		// Native Render chams — UI placeholder only.
		inline bool  native_chams_enabled{ false };
		inline int   native_chams_mode{ 1 }; // 0=X-Ray 1=Wireframe 2=Glow 3=Flat
		inline float native_chams_color[4]{ 0.f, 1.f, 1.f, 1.f }; // cyan
		// Animated — like skybox: each part cycles through the sky palette over time
		inline bool  native_chams_animated{ false };
		inline int   native_chams_anim_style{ 1 }; // 0=Kaleidoscope 1=Nebula 2=Aurora 3=Black Hole
		inline float native_chams_anim_speed{ 1.f }; // 0.1 .. 3.0

		// Weapon chams â€” neon outline on each enemy's held weapon Handle
		inline bool  weapon_chams{ false };
		inline float weapon_chams_color[4]{ 0.f, 0.8f, 1.f, 1.f };
		inline float weapon_chams_glow_size{ 6.0f };

		inline bool target_warning_icon{ false };
		inline float target_warning_icon_size{ 24.0f };

		inline bool flags{ false };
		inline float flags_state_colour[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool client_box{ false };
		inline float client_box_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline bool client_box_fill{ false };
		inline float client_box_fill_color[4]{ 0.2f, 0.2f, 0.2f, 0.3f };

		inline bool client_name{ false };
		inline float client_name_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline bool client_avatar{ false };

		inline bool client_healthbar{ false };
		inline float client_healthbar_color[4]{ 0.f, 1.f, 0.f, 1.f };
		inline bool client_health_percent{ false };
		inline float client_health_percent_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool client_armorbar{ false };
		inline float client_armorbar_color[4]{ 0.275f, 0.627f, 1.f, 1.f };

		inline bool client_distance{ false };
		inline float client_distance_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool client_tool{ false };
		inline float client_tool_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool client_chams{ false };
		inline float client_chams_fill_color[4]{ 1.f, 0.f, 0.f, 0.5f };
		inline float client_chams_outline_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool client_flags{ false };
		inline float client_flags_state_colour[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool client_headless{ false };
		inline bool client_korblox{ false };

		inline bool esp_preview_auto_rotate{ true };
		inline bool esp_preview_enabled{ false };

		inline bool debug_wallcheck{ false };

		inline bool view_hitbox{ false };
		inline float view_hitbox_color[4]{ 1.f, 0.f, 0.f, 1.f };

		inline float fade_in_speed{ 5.0f };
		inline float fade_out_speed{ 5.0f };

		inline bool knock_check{ false };
		inline bool teamcheck{ false };
		inline bool team_based_color{ false };
		inline bool text_background{ false };
		inline float text_background_color[4]{ 0.f, 0.f, 0.f, 1.f };
		inline bool esp_glow{ false };
		inline bool head_dot_glow{ false };
		inline int health_text_pos{ 0 };
		inline float visible_color[4]{ 0.55f, 0.75f, 1.f, 1.f };
		inline float occluded_color[4]{ 1.f, 0.25f, 0.25f, 1.f };
		inline bool ignore_whitelisted{ false };

		inline bool max_distance_enabled{ false };
		inline float max_distance{ 500.f };
		inline bool visibility_check{ false };

		inline bool hit_tracers_enabled{ false };
		inline int hit_tracers_method{ 0 };
		inline int hit_tracers_type{ 0 };
		inline float hit_tracers_color[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
		inline float hit_tracers_duration{ 1.0f };

		// --- ESP quality improvements ---
		// Head dot: draw a circle on the head position
		inline bool  head_dot{ false };
		inline float head_dot_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline float head_dot_size{ 3.5f };

		// Snap line
		inline bool  snap_line{ false };
		inline int   snap_line_origin{ 0 };  // 0=bottom-center, 1=top-center, 2=cursor
		inline float snap_line_color[4]{ 1.f, 1.f, 1.f, 0.6f };
		inline float snap_line_thickness{ 1.f };

		// Healthbar gradient (override solid color with green->red)
		inline bool  healthbar_gradient{ false };

		// Box corner style rounding
		inline bool  box_rounded_corners{ false };

		// Outline on text elements
		inline bool  text_shadow{ true };

		// Skeleton
		inline bool  skeleton{ false };
		inline float skeleton_color[4]{ 1.f, 1.f, 1.f, 1.f };
	}

	namespace movement
	{
		namespace speedhack
		{
			inline bool enabled{ false };
			inline int mode{ 0 };
			inline float speed{ 50.0f };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
		}

		namespace jumphack
		{
			inline bool enabled{ false };
			inline float value{ 50.0f };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
		}

		namespace flyhack
		{
			inline bool enabled{ false };
			inline int mode{ 0 };
			inline float speed{ 50.0f };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
		}

		namespace tickrate
		{
			inline bool enabled{ false };
			inline float value{ 240.0f };
		}

		namespace orbit
		{
			inline bool enabled{ false };
			inline int orbit_type{ 0 };
			inline float speed{ 30.0f };
			inline float radius{ 10.0f };
			inline float height_offset{ 10.0f };
			inline bool spectate_target{ false };
			inline bool randomize{ false };
			inline float randomize_x{ 5.0f };
			inline float randomize_y{ 5.0f };
		}

		namespace gravity
		{
			inline bool enabled{ false };
			inline float value{ 196.2f };
		}

		namespace desync
		{
			inline bool enabled{ false };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
		}

		namespace bhop
		{
			inline bool enabled{ false };
			inline int  keybind{ VK_SPACE };
			inline int  activation_mode{ 1 };
			inline float slide_multiplier{ 1.5f };
			inline int   slide_duration_ms{ 300 };
		}

		namespace thirdperson
		{
			inline bool  enabled{ false };
			inline float fov{ 90.0f };
		}
	}

	namespace crosshair
	{
		inline bool  enabled{ false };
		inline float radius{ 12.0f };
		inline float dot_size{ 2.5f };
		inline int   dot_count{ 8 };
		inline float speed{ 2.0f };
		inline float color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline bool  rainbow{ false };
		inline float rainbow_speed{ 1.0f };
		inline float thickness{ 2.0f };     // line thickness (for line/static style)
		inline int   style{ 0 };            // 0 = dots, 1 = lines (rotating), 2 = static 4-bar

		// Static 4-bar crosshair (style == 2)
		inline float static_gap{ 4.0f };    // gap from center to bar start
		inline float static_length{ 8.0f }; // length of each bar
		inline bool  static_dot{ false };   // center dot
		inline bool  static_outline{ true }; // black outline for contrast
	}

	namespace ui
	{
		inline bool watermark{ true };
		inline bool keybinds{ true };
	}

	namespace watermark
	{
		// Elements toggle
		inline bool show_cheat_name{ true };
		inline bool show_display_name{ false };
		inline bool show_username{ false };
		inline bool show_fps{ true };
		inline bool show_ping{ false };
		inline bool show_server_ip{ false };

		// Separator
		inline int separator_type{ 0 }; // 0= " | ", 1= " - ", 2= " / ", 3= " :: ", 4= "  "

		// Color
		inline float text_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline bool rainbow{ false };
		inline float rainbow_speed{ 1.0f };

		// Drag position (saved)
		inline float pos_x{ 20.0f };
		inline float pos_y{ 100.0f };
		inline bool pos_initialized{ false };
	}

	// ---------------------------------------------------------------------
	// Manual UI Scaling — user-defined resolution (fixes broken auto-scale)
	// Base resolution is 2560x1440. UIScale = ScreenWidth / 2560.0f
	// Height is stored but not used for scale (prevents stretched menu).
	// Safety: if ScreenWidth <= 0 or blank → UIScale = 1.0f
	// ---------------------------------------------------------------------
	inline float UIScale{ 1.0f };
	namespace ui_scale
	{
		constexpr int BASE_WIDTH  = 2560;
		constexpr int BASE_HEIGHT = 1440;
		inline float GetUIScale(int userWidth)
		{
			if (userWidth <= 0) return 1.0f;
			return static_cast<float>(userWidth) / static_cast<float>(BASE_WIDTH);
		}
	}

	namespace launcher
	{
		// Launcher attach method — shown before loading screen
		// 0 = Kernel, 1 = Usermode (Recommended)
		inline int attach_method{ 1 };
		inline bool show_launcher{ true };
	}

	namespace menu
	{
		inline int menu_keybind{ VK_INSERT };
		inline bool watermark{ false };
		inline bool streamproof{ false };
		inline bool vsync{ false };
		inline bool hide_console{ true };
		inline bool performance_mode{ false };

		// Manual resolution inputs — user types width/height (e.g. 1920 / 1080)
		// Base resolution is 2560x1440. UIScale = width / 2560.0f
		inline int screen_width{ 2560 };
		inline int screen_height{ 1440 };
		// Mirror of global ::UIScale for convenience inside settings::menu
		inline float menu_uiscale{ 1.0f };
		inline void UpdateUIScale()
		{
			if (screen_width <= 0) { menu_uiscale = 1.0f; ::UIScale = 1.0f; ::settings::UIScale = 1.0f; }
			else { menu_uiscale = static_cast<float>(screen_width) / 2560.0f; ::UIScale = menu_uiscale; ::settings::UIScale = menu_uiscale; }
		}
		inline float GetMenuUIScale()
		{
			if (screen_width <= 0) return 1.0f;
			return static_cast<float>(screen_width) / 2560.0f;
		}

		// Menu accent color -- defaults to soft off-white
		inline float accent_color[4]{ 93/255.f, 157/255.f, 246/255.f, 1.f };

		// Theme customization panel settings
		inline float bg_opacity{ 0.90f };
		inline float border_thickness{ 1.0f };
		inline bool  rain_effect{ true };
		inline float rain_opacity{ 0.72f };
		inline int   rain_count{ 110 };
		inline float menu_scale{ 1.0f };
		inline float font_scale{ 1.0f };
		inline float panel_rounding{ 16.0f };
		inline bool  topbar_show_version{ true };
		inline bool  topbar_show_active{ true };
		inline float topbar_text_color[4]{ 0.55f, 0.55f, 0.58f, 1.f };
		inline float sidebar_opacity{ 1.0f };

		// Per-panel background colors (RGBA) — charcoal card from the reference
		inline float color_sidebar[4]     { 26/255.f, 27/255.f, 38/255.f, 0.98f };
		inline float color_header[4]      { 23/255.f, 23/255.f, 31/255.f, 0.98f };
		inline float color_content[4]     { 26/255.f, 27/255.f, 38/255.f, 0.98f };
		inline float color_topbar[4]      { 23/255.f, 23/255.f, 31/255.f, 0.98f };
		inline float color_border[4]      { 41/255.f,  46/255.f,  66/255.f,  0.45f };
		inline float color_child[4]       { 32/255.f,  34/255.f,  46/255.f,  1.f };
		inline float color_text[4]        { 122/255.f, 131/255.f, 165/255.f, 1.f };
		inline float color_text_muted[4]  { 64/255.f, 70/255.f, 102/255.f, 1.f };
	}

	namespace lighting
	{
		namespace fog
		{
			inline bool enabled{ false };
			inline float fog_start{ 0.0f };
			inline float fog_end{ 500.0f };
			inline float fog_r{ 0.75f };
			inline float fog_g{ 0.75f };
			inline float fog_b{ 0.75f };
		}

		namespace shadows
		{
			inline bool disable{ false };
		}

		namespace clocktime
		{
			inline bool enabled{ false };
			inline float clock_time{ 12.0f };
		}

		namespace exposure
		{
			inline bool enabled{ false };
			inline float exposure{ 0.f };
		}

		namespace atmosphere
		{
			inline bool  enabled{ false };
			inline float density{ 0.536f };
			inline float offset { 0.250f };
			inline float haze   { 2.21f  };
			inline float glare  { 1.000f };
		}

		// Gelato-ported world effects
		namespace bloom
		{
			inline bool  enabled{ false };
			inline float intensity{ 0.4f };
			inline float size{ 24.0f };
			inline float threshold{ 0.95f };
		}

		namespace blur
		{
			inline bool  enabled{ false };
			inline float size{ 8.0f };
		}

		namespace color_correction
		{
			inline bool  enabled{ false };
			inline float brightness{ 0.0f };
			inline float contrast{ 0.0f };
			inline float tint[4]{ 1.f, 1.f, 1.f, 1.f };
		}

		namespace depth_of_field
		{
			inline bool  enabled{ false };
			inline float far_intensity{ 0.1f };
			inline float focus_distance{ 50.0f };
			inline float in_focus_radius{ 50.0f };
			inline float near_intensity{ 0.1f };
		}

		namespace ambient
		{
			inline bool  enabled{ false };
			inline float ambient_color[4]{ 0.5f, 0.5f, 0.5f, 1.f };
			inline float outdoor_ambient_color[4]{ 0.5f, 0.5f, 0.5f, 1.f };
		}

	}

	namespace exploits
	{
		namespace antiafk
		{
			inline bool enabled{ false };
		}

		namespace displayfps
		{
			inline bool enabled{ false };
		}

		namespace freezeplayer
		{
			inline bool enabled{ false };
			inline int keybind{ 0 };
			inline int activation_mode{ 1 };
		}
	}

	namespace triggerbot
	{
		inline bool enabled{ false };
		inline int keybind{ 0 };
		inline int activation_mode{ 1 };  // 0=toggle 1=hold 2=always

		// Which hitbox to check cursor-over
		// 0=Any, 1=Head, 2=Torso/Root, 3=LeftArm, 4=RightArm, 5=LeftLeg, 6=RightLeg
		inline int target_part{ 0 };

		// Delay before firing (ms)
		inline float delay_ms{ 0.0f };

		// Random jitter added to delay (ms). Actual delay = delay_ms Â± jitter_ms
		inline float jitter_ms{ 0.0f };

		// How long to hold the click down (ms)
		inline float hold_ms{ 50.0f };

		// Cooldown between shots (ms)
		inline float cooldown_ms{ 100.0f };

		// Pixel-radius FOV around cursor â€” only triggers if target part is within this distance
		inline float fov{ 15.0f };
		inline bool use_fov{ true };

		// FOV circle draw
		inline bool draw_fov{ false };
		inline float fov_circle_colour[4]{ 1.f, 1.f, 1.f, 1.f };
		inline float fov_outline_colour[4]{ 0.f, 0.f, 0.f, 1.f };
		inline bool fov_circle_rainbow{ false };
		inline float fov_circle_rainbow_speed{ 1.0f };

		// Filters
		inline bool teamcheck{ false };
		inline bool knock_check{ false };
		inline bool require_los{ false };  // only fire when not wall-checked

		// Which mouse button to simulate
		// 0 = Left  1 = Right  2 = Middle
		inline int fire_button{ 0 };

		// Visual feedback â€” draw a small indicator when hovering a target
		inline bool draw_crosshair_indicator{ true };
		inline float indicator_color[4]{ 1.0f, 0.3f, 0.3f, 1.0f };
	}

	namespace colorbot
	{
		inline bool enabled{ false };
		inline int keybind{ 0 };
		inline int activation_mode{ 1 };  // 0=toggle 1=hold 2=always

		// --- Primary target color (RGBA, 0-1 range) ---
		inline float target_color[4]{ 0.90f, 0.20f, 0.20f, 1.f };

		// --- Multi-color support (up to 4 additional colors) ---
		inline bool  multi_color_enabled{ false };
		inline int   multi_color_count{ 0 };           // 0-3 active secondary colors
		inline float multi_colors[3][4]{
			{ 0.90f, 0.50f, 0.10f, 1.f },
			{ 0.80f, 0.80f, 0.10f, 1.f },
			{ 0.10f, 0.80f, 0.20f, 1.f },
		};

		// --- Tolerance mode ---
		// 0 = RGB (classic), 1 = HSV (hue-based, better across lighting)
		inline int   tolerance_mode{ 0 };
		inline float tolerance{ 30.f };   // RGB per-channel OR HSV composite
		inline float hue_tolerance{ 15.f };    // HSV mode: hue range (degrees)
		inline float sat_tolerance{ 0.35f };   // HSV mode: saturation range
		inline float val_tolerance{ 0.35f };   // HSV mode: value range

		// --- FOV ---
		inline float fov{ 80.f };
		inline bool  draw_fov{ false };
		inline float fov_color[4]{ 1.f, 1.f, 1.f, 0.6f };

		// --- Smoothing ---
		inline float smoothing{ 8.f };

		// --- Aim mode: 0=aim only, 1=aim+click ---
		inline int aim_mode{ 0 };

		// --- Flick mode: flick quickly then hold briefly on target ---
		inline bool  flick_mode{ false };
		inline float flick_speed{ 2.5f };   // multiplier for flick phase speed
		inline float flick_hold_ms{ 80.f }; // how long to hold after flicking onto target

		// --- Target lock: stay locked on the first match cluster found ---
		inline bool  target_lock{ false };
		inline float lock_tolerance_mult{ 2.5f };  // tolerance multiplier while locked

		// --- Recoil compensation ---
		inline bool  recoil_comp{ false };
		inline float recoil_x{ 0.f };
		inline float recoil_y{ 3.5f };   // pixels per tick to compensate (negative = up)

		// --- Screen color picker state ---
		inline bool  picking_active{ false };
		inline bool  picking_done{ false };
		inline float picked_r{ 0.f };
		inline float picked_g{ 0.f };
		inline float picked_b{ 0.f };

		// --- Click threshold (px) ---
		inline float click_threshold{ 6.f };

		// --- Statistics (read-only, updated by colorbot thread) ---
		inline float last_confidence{ 0.f };  // 0-1, how strong the last detection was
		inline int   matches_per_frame{ 0 };  // pixel match count last frame
	}

	namespace cilent
	{
		namespace fpscaps
		{
			inline bool enabled{ false };
		}
	}

	namespace globals
	{
		inline bool is_game_active{ true };
		inline bool version_matched{ false };
		inline std::string offset_validation_result{};
	}

	namespace bladeball
	{
		inline bool autoparry{ false };
		inline bool clash_mode{ false };
		inline bool test_mode{ false };
		inline bool debug_output{ false };
		inline float parry_distance{ 20.0f };
		inline bool esp_lines{ true };
		inline float esp_lines_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline bool draw_parry_dist{ false };
		inline float parry_dist_color[4]{ 1.f, 1.f, 1.f, 1.f };
	}

	namespace havoc
	{
		inline bool  ai_chams_enabled{ false };
		inline float ai_chams_fill_color[4]{ 1.f, 0.f, 0.f, 0.85f };
		inline float ai_chams_outline_color[4]{ 0.6f, 0.f, 0.f, 1.f };
		inline float ai_max_distance{ 150.f };

		// Neon weapon chams (dropped tools in workspace)
		inline bool  weapon_chams_enabled{ false };
		inline float weapon_chams_color[4]{ 0.f, 0.8f, 1.f, 1.f };
		inline float weapon_chams_glow_size{ 6.0f };
		inline float weapon_max_distance{ 150.f };

		// Neon glow on the local player's held weapon (equipped Tool)
		inline bool  local_weapon_glow_enabled{ false };
		inline float local_weapon_glow_color[4]{ 0.f, 1.f, 0.8f, 1.f };  // teal neon default
		inline float local_weapon_glow_size{ 8.0f };
	}

	namespace techy
	{
		inline bool skin_changer{ false };

		// -1 = no skin selected (keep current)
		inline int gun_skin_index{ -1 };
		inline int knife_skin_index{ -1 };

		// Debug status â€” set by techy::run() each tick
		inline std::string debug_status{};

		struct skin_entry_t
		{
			const char* name;
			const char* color_map;
		};

		// â”€â”€ Gun skins (DesertEagle, R8, USP-S) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline constexpr skin_entry_t gun_skins[] = {
			// DesertEagle
			{ "DE | Damascus (FN)",          "rbxassetid://138742397116112" },
			{ "DE | Engraved (FN)",          "rbxassetid://87449264885059"  },
			{ "DE | Oxide (FN)",             "rbxassetid://107016845882306" },
			{ "DE | Fractured Web (FN)",     "rbxassetid://91663410536964"  },
			{ "DE | Consumed (FN)",          "rbxassetid://83512116644691"  },
			{ "DE | Modern Art (FN)",        "rbxassetid://82677159073278"  },
			{ "DE | Cybernetic (FN)",        "rbxassetid://78767973765446"  },
			{ "DE | Nacre Lattice (SE)",     "rbxassetid://111907579480057" },
			{ "DE | Printstream (FN)",       "rbxassetid://105074247423814" },
			{ "DE | Case Hardened 916 (FN)", "rbxassetid://121618027411944" },
			{ "DE | Case Hardened 998 (FN)", "rbxassetid://112390771094976" },
			{ "DE | Case Hardened 408 (FN)", "rbxassetid://75858482052000"  },
			{ "DE | Case Hardened 104 (FN)", "rbxassetid://79779009988947"  },
			{ "DE | Case Hardened 44 (FN)",  "rbxassetid://138424045421695" },
			{ "DE | Case Hardened 951 (FN)", "rbxassetid://90381321114645"  },
			{ "DE | Case Hardened 307 (FN)", "rbxassetid://110487536904857" },
			{ "DE | Case Hardened 361 (FN)", "rbxassetid://121601546787868" },
			{ "DE | Case Hardened 578 (FN)", "rbxassetid://110616699583170" },
			{ "DE | Case Hardened 781 (FN)", "rbxassetid://125238624184220" },
			// R8 Revolver
			{ "R8 | Fade Full (FN)",         "rbxassetid://123945467374041" },
			{ "R8 | Fade F87 (FN)",          "rbxassetid://135438673691961" },
			{ "R8 | Fade F72 (FN)",          "rbxassetid://82013592745920"  },
			{ "R8 | Fade F58 (FN)",          "rbxassetid://140147362561207" },
			{ "R8 | Fade F43 (FN)",          "rbxassetid://80691741784688"  },
			{ "R8 | Vanilla",                "rbxassetid://85443679266923"  },
			{ "R8 | Blaze (FN)",             "rbxassetid://136402474285406" },
			{ "R8 | Crazy8 (FN)",            "rbxassetid://123616573352680" },
			{ "R8 | Laminate (FN)",          "rbxassetid://100505976539345" },
			// USP-S
			{ "USP-S | Hush Puppy (FN)",     "rbxassetid://75926657118100"  },
			{ "USP-S | Vanilla",             "rbxassetid://84198270112714"  },
			{ "USP-S | True Jester (FN)",    "rbxassetid://113438333935485" },
		};
		inline constexpr int gun_skin_count = static_cast<int>(std::size(gun_skins));

		// â”€â”€ Knife skins (Butterfly, M9, Nomad, Survivor, Bayonet) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline constexpr skin_entry_t knife_skins[] = {
			// Butterfly
			{ "Butterfly | Crystallized Ph1",  "rbxassetid://114256896900755" },
			{ "Butterfly | Crystallized Ph2",  "rbxassetid://101314810394056" },
			{ "Butterfly | Crystallized Ph3",  "rbxassetid://102085944033634" },
			{ "Butterfly | Crystallized Ph4",  "rbxassetid://113909865804774" },
			{ "Butterfly | Crystallized Emerald", "rbxassetid://120648733296846" },
			{ "Butterfly | Crystallized Sapphire","rbxassetid://119861115134760" },
			{ "Butterfly | Crystallized Ruby",    "rbxassetid://86664710226137"  },
			{ "Butterfly | Crystallized BP",      "rbxassetid://100067029274145" },
			{ "Butterfly | Vermilion (FN)",    "rbxassetid://102297274576710" },
			// M9 Bayonet
			{ "M9 | Crystallized Ph1",         "rbxassetid://87931026004752"  },
			{ "M9 | Crystallized Ph2",         "rbxassetid://91448370983218"  },
			{ "M9 | Crystallized Ph3",         "rbxassetid://72076477148372"  },
			{ "M9 | Crystallized Ph4",         "rbxassetid://140156493036254" },
			{ "M9 | Crystallized Emerald",     "rbxassetid://117986963361759" },
			{ "M9 | Crystallized Sapphire",    "rbxassetid://121623283509685" },
			{ "M9 | Crystallized Ruby",        "rbxassetid://91833537069848"  },
			{ "M9 | Crystallized BP",          "rbxassetid://103270045896612" },
			{ "M9 | Vermilion (FN)",           "rbxassetid://120173306439662" },
			// Nomad
			{ "Nomad | Emerald (FN)",          "rbxassetid://111769292981204" },
			{ "Nomad | Blue Gem (FN)",         "rbxassetid://88340427060384"  },
			{ "Nomad | Crimson Mandala (FN)",  "rbxassetid://76754360966012"  },
			{ "Nomad | Tiger Stripe (FN)",     "rbxassetid://133973830029608" },
			// Survivor
			{ "Survivor | Stained (FN)",       "rbxassetid://130643347893995" },
			{ "Survivor | Carmine (FN)",       "rbxassetid://111913098680746" },
			{ "Survivor | Rusted (FN)",        "rbxassetid://127798086973131" },
			// Holster models (lobby knife)
			{ "Nomad (Lobby)",                 "rbxassetid://135246032617110" },
			{ "Bayonet (Lobby)",               "rbxassetid://85111442135872"  },
		};
		inline constexpr int knife_skin_count = static_cast<int>(std::size(knife_skins));
	}
}

