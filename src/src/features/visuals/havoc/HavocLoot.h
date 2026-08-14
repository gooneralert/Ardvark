#pragma once

#include "HavocShared.h"

namespace Cheat {
namespace Visuals {
namespace HavocLoot {

inline void Render(ImDrawList* draw_list, ImFont* font, float font_size,
	const Matrix4x4& view, const Vector2& viewport,
	const Vector3& cam_pos, float scale_x, float scale_y,
	Instance& loots)
{
	using namespace HavocShared;

	static FolderCache g_loot;

	std::vector<std::shared_ptr<Instance>> folders;
	for (const auto& child : loots.GetChildren())
	{
		if (!g_Memory.IsValid(child.address))
			continue;
		if (child.GetName() == "Items" && child.GetClassName() == "Folder")
		{
			folders.push_back(std::make_shared<Instance>(child));
			break;
		}
	}
	RefreshCache(g_loot, folders, 500, true);
	DrawWorldEntries(draw_list, font, font_size, view, viewport, cam_pos, scale_x, scale_y,
		g_loot.items, g_Settings.esp.ground_loot_color,
		g_Settings.esp.loot_chams_shader, true, g_Settings.esp.loot_chams);
}

} // namespace HavocLoot
} // namespace Visuals
} // namespace Cheat
