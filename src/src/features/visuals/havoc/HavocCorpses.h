#pragma once

#include "HavocShared.h"

namespace Cheat {
namespace Visuals {
namespace HavocCorpses {

inline void Render(ImDrawList* draw_list, ImFont* font, float font_size,
	const Matrix4x4& view, const Vector2& viewport,
	const Vector3& cam_pos, float scale_x, float scale_y,
	Instance& loots)
{
	using namespace HavocShared;

	static FolderCache g_corpses;

	std::vector<std::shared_ptr<Instance>> folders;
	static const char* chars_path[] = { "Loots", "Characters" };
	static const char* bodies_path[] = { "Bodies" };
	if (auto chars = ResolvePath(loots, chars_path, 2))
		folders.push_back(chars);
	if (auto bodies = ResolvePath(loots, bodies_path, 1))
		folders.push_back(bodies);
	RefreshCache(g_corpses, folders, 400, false);
	DrawWorldEntries(draw_list, font, font_size, view, viewport, cam_pos, scale_x, scale_y,
		g_corpses.items, g_Settings.esp.corpse_color,
		g_Settings.esp.loot_chams_shader, false, false);
}

} // namespace HavocCorpses
} // namespace Visuals
} // namespace Cheat
