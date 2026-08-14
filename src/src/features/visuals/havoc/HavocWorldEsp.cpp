#include "pch.h"
#include "HavocWorldEsp.h"
#include "HavocCorpses.h"
#include "HavocLoot.h"
#include "HavocContainers.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "app/Settings.h"

namespace Cheat {
namespace Visuals {
namespace HavocWorldEsp {

bool IsActivePlace()
{
	if (!Globals::InstanceDataModel.address) return false;
	if (!g_Memory.IsValid(Globals::InstanceDataModel.address)) return false;
	return Globals::InstanceDataModel.GetPlaceId() == kPlaceId;
}

bool BeyondRange(float dist_studs)
{
	if (!IsActivePlace())
	{
		return false;
	}

	return StudsToMeters(dist_studs) > kMaxMeters;
}

// world esp для havoc (трупы / лут / контейнеры)
void Render(ImDrawList* draw_list, ImFont* font, float font_size,
	const Matrix4x4& view, const Vector2& viewport,
	const Vector3& cam_pos, float overlay_w, float overlay_h,
	float scale_x, float scale_y)
{
	(void)overlay_w;
	(void)overlay_h;

	if (!IsActivePlace())
		return;
	if (!g_Settings.esp.enabled)
		return;
	if (!g_Settings.esp.corpses && !g_Settings.esp.ground_loot && !g_Settings.esp.containers)
		return;
	if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
		return;
	if (!draw_list || !font)
		return;

	static const char* loots_path[] = { "Buildings", "Loots" };
	auto loots = HavocShared::ResolvePath(*Globals::Workspace, loots_path, 2);
	if (!loots || !g_Memory.IsValid(loots->address))
		return;

	if (g_Settings.esp.corpses)
	{
		HavocCorpses::Render(draw_list, font, font_size, view, viewport, cam_pos,
			scale_x, scale_y, *loots);
	}

	if (g_Settings.esp.ground_loot)
	{
		HavocLoot::Render(draw_list, font, font_size, view, viewport, cam_pos,
			scale_x, scale_y, *loots);
	}

	if (g_Settings.esp.containers)
	{
		HavocContainers::Render(draw_list, font, font_size, view, viewport, cam_pos,
			scale_x, scale_y, *loots);
	}
}

} // namespace HavocWorldEsp
} // namespace Visuals
} // namespace Cheat
