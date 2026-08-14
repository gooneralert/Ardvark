#pragma once

#include "HavocShared.h"

namespace Cheat {
namespace Visuals {
namespace HavocContainers {

inline void Render(ImDrawList* draw_list, ImFont* font, float font_size,
	const Matrix4x4& view, const Vector2& viewport,
	const Vector3& cam_pos, float scale_x, float scale_y,
	Instance& loots)
{
	using namespace HavocShared;

	static FolderCache g_containers;

	std::vector<std::shared_ptr<Instance>> folders;
	static const char* crates_path[] = { "Loots", "Crates" };
	static const char* stashes_path[] = { "Loots", "Stashes" };
	if (auto crates = ResolvePath(loots, crates_path, 2))
		folders.push_back(crates);
	if (auto stashes = ResolvePath(loots, stashes_path, 2))
		folders.push_back(stashes);
	RefreshCache(g_containers, folders, 800, false);
	DrawWorldEntries(draw_list, font, font_size, view, viewport, cam_pos, scale_x, scale_y,
		g_containers.items, g_Settings.esp.containers_color,
		g_Settings.esp.containers_chams_shader, false,
		g_Settings.esp.containers_chams);
}

} // namespace HavocContainers
} // namespace Visuals
} // namespace Cheat
