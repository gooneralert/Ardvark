#pragma once
// UI-only stubs. These exist so the menu compiles without any game/external backend.
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <d3d11.h>
#include <imgui/imgui.h>

#define OFFSET(a, b) 0

namespace Offsets
{
	namespace Player { inline constexpr std::uint64_t LocalPlayer = 0; inline constexpr std::uint64_t UserId = 0; }
	namespace DataModel { inline constexpr std::uint64_t GameId = 0; inline constexpr std::uint64_t PlaceId = 0; }
	namespace RunService { inline constexpr std::uint64_t HeartbeatFPS = 0; }
	namespace StatsItem { inline constexpr std::uint64_t Value = 0; }
}

namespace math
{
	struct vector3 { float x = 0, y = 0, z = 0; };
}

namespace rbx
{
	struct instance_t
	{
		std::uint64_t address = 0;
		std::string get_name() const { return {}; }
		instance_t find_first_child_by_class(const char*) const { return {}; }
		instance_t find_first_child(const char*) const { return {}; }
	};
}

namespace cache
{
	struct part_data_t {};
	struct player_t
	{
		rbx::instance_t instance{};
		std::string name;
		std::string display_name;
	};
	inline player_t cached_local_player{};
}

struct dummy_memory_t
{
	template <typename T>
	T read(std::uint64_t) const { return T{}; }
};
inline dummy_memory_t g_dummy_memory{};
inline dummy_memory_t* memory = &g_dummy_memory;

namespace game
{
	struct node_t
	{
		std::uint64_t address = 0;
		rbx::instance_t find_first_child_by_class(const char*) const { return {}; }
	};
	inline node_t datamodel{};
	inline node_t players{};
	inline node_t local_player{};
	inline HWND get_roblox_window() { return nullptr; }
}

namespace font_config
{
	namespace tahoma { inline float font_size = 13.f; }
}

namespace external_config
{
	inline std::string cheat_name = "Dejected";
}

namespace config
{
	struct config_info_t { std::string name; std::string path; };
	inline std::vector<config_info_t> get_config_list() { return {}; }
	inline bool save_config(const std::string&) { return false; }
	inline bool load_config(const std::string&) { return false; }
	inline void open_file_location() {}
}

namespace custom_entities
{
	inline std::mutex containers_mtx;
	inline void add_container(const char*) {}
}

namespace explorer
{
	struct node_t
	{
		node_t* parent = nullptr;
		std::vector<node_t*> children;
		std::string name;
		std::string class_name;
	};

	struct explorer_t
	{
		node_t* root = nullptr;
		void render_window(bool*) {}
		void render_settings() {}
		void render_node(node_t*, int = 0) {}
		void render_properties() {}
	};

	inline std::unique_ptr<explorer_t> explorer = std::make_unique<explorer_t>();
	inline void render_humanoid_scanner_panel(explorer_t*) {}
	inline void render_techy_skin_scanner_panel(explorer_t*) {}
}

namespace players
{
	inline void render_window(bool*) {}
}

namespace startup
{
	inline std::atomic<const char*> status_msg{ "" };
	inline std::atomic<float> progress{ 1.f };
	inline std::atomic<bool> ready{ true };
}

inline void ForceRescan() {}

inline RECT s_explorer_rect{};
inline bool s_explorer_visible = false;

class AvatarManager {};
inline AvatarManager* g_avatar_manager = nullptr;

void matcha_ui_init_stubs();
