#include "pch.h"
#include "HitboxExpander.h"
#include "HitboxJobs.h"

#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/player/PlayerHandler.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"
#include "renderer/Renderer.h"
#include "imgui.h"

#include <vector>

namespace Cheat {
namespace Features {
namespace HitboxExpander {

namespace {

bool WorldToScreenVm(const Matrix4x4& vm, const Vector2& vp, const Vector3& pos, Vector2& screen, float sx, float sy)
{
	float w = pos.x * vm.m[3][0] + pos.y * vm.m[3][1] + pos.z * vm.m[3][2] + vm.m[3][3];
	if (w < 0.01f)
	{
		return false;
	}

	float x = pos.x * vm.m[0][0] + pos.y * vm.m[0][1] + pos.z * vm.m[0][2] + vm.m[0][3];
	float y = pos.x * vm.m[1][0] + pos.y * vm.m[1][1] + pos.z * vm.m[1][2] + vm.m[1][3];
	float inv = 1.f / w;
	x *= inv;
	y *= inv;
	screen.x = ((vp.x * 0.5f) + (x * vp.x * 0.5f)) * sx;
	screen.y = ((vp.y * 0.5f) - (y * vp.y * 0.5f)) * sy;
	return true;
}

void CornersWorld(const Vector3& pos, const Vector3& sz, const Matrix4x4& rot, Vector3 out[8])
{
	Vector3 half(sz.x * 0.5f, sz.y * 0.5f, sz.z * 0.5f);
	Vector3 local[8] = {
		{ -half.x, -half.y, -half.z }, { -half.x, -half.y,  half.z },
		{ -half.x,  half.y, -half.z }, { -half.x,  half.y,  half.z },
		{  half.x, -half.y, -half.z }, {  half.x, -half.y,  half.z },
		{  half.x,  half.y, -half.z }, {  half.x,  half.y,  half.z }
	};

	for (int i = 0; i < 8; ++i)
	{
		const Vector3& c = local[i];
		out[i] = Vector3(
			pos.x + (rot.m[0][0] * c.x + rot.m[0][1] * c.y + rot.m[0][2] * c.z),
			pos.y + (rot.m[1][0] * c.x + rot.m[1][1] * c.y + rot.m[1][2] * c.z),
			pos.z + (rot.m[2][0] * c.x + rot.m[2][1] * c.y + rot.m[2][2] * c.z));
	}
}

ImU32 Col4(const float c[4], float a_mul)
{
	float a = c[3] * a_mul;
	if (a < 0.f) a = 0.f;
	if (a > 1.f) a = 1.f;
	return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), (int)(a * 255));
}

void DrawPartViz(ImDrawList* dl, const Matrix4x4& vm, const Vector2& vp, float sx, float sy,
                 const Vector3& pos, const Vector3& sz, const Matrix4x4& rot, int mode, const float color[4])
{
	Vector3 world[8];
	CornersWorld(pos, sz, rot, world);

	Vector2 sp[8];
	bool ok[8]{};
	int n_ok = 0;
	float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f;

	for (int i = 0; i < 8; ++i)
	{
		ok[i] = WorldToScreenVm(vm, vp, world[i], sp[i], sx, sy);
		if (!ok[i])
		{
			continue;
		}

		++n_ok;
		if (sp[i].x < min_x) min_x = sp[i].x;
		if (sp[i].x > max_x) max_x = sp[i].x;
		if (sp[i].y < min_y) min_y = sp[i].y;
		if (sp[i].y > max_y) max_y = sp[i].y;
	}

	if (n_ok < 2)
	{
		return;
	}

	ImU32 col = Col4(color, 1.f);
	ImU32 fill = Col4(color, 0.35f);

	if (mode == 0)
	{
		// 2d aabb
		dl->AddRect(ImVec2(min_x, min_y), ImVec2(max_x, max_y), col, 0.f, 0, 1.5f);
		return;
	}

	if (mode == 2)
	{
		// filled сначала, потом рёбра
		static const int faces[6][4] = {
			{0,1,3,2},{4,5,7,6},{0,1,5,4},{2,3,7,6},{0,2,6,4},{1,3,7,5}
		};

		for (int f = 0; f < 6; ++f)
		{
			ImVec2 poly[4];
			bool all = true;
			for (int i = 0; i < 4; ++i)
			{
				int idx = faces[f][i];
				if (!ok[idx])
				{
					all = false;
					break;
				}

				poly[i] = ImVec2(sp[idx].x, sp[idx].y);
			}

			if (!all)
			{
				continue;
			}

			dl->AddConvexPolyFilled(poly, 4, fill);
		}
	}

	static const int edges[12][2] = {
		{0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
	};

	for (int e = 0; e < 12; ++e)
	{
		int a = edges[e][0];
		int b = edges[e][1];
		if (!ok[a] || !ok[b])
		{
			continue;
		}

		dl->AddLine(ImVec2(sp[a].x, sp[a].y), ImVec2(sp[b].x, sp[b].y), col, 1.5f);
	}
}

} // namespace

void Render()
{
	auto& hb = g_Settings.hitbox;
	if (!hb.enabled || !hb.visualize)
	{
		return;
	}

	if (!Globals::Workspace || !Globals::Players || !g_Memory.IsAttached())
	{
		return;
	}

	auto cam_ptr = Globals::Workspace->GetCurrentCamera();
	if (!cam_ptr)
	{
		return;
	}

	Camera camera(cam_ptr->address);
	Vector2 viewport = camera.GetViewportSize();
	if (viewport.x < 1.f || viewport.y < 1.f)
	{
		return;
	}

	float sx = 1.f, sy = 1.f;
	HWND oh = Renderer::GetHwnd();
	if (oh)
	{
		RECT ocr{};
		if (GetClientRect(oh, &ocr))
		{
			float ow = (float)(ocr.right - ocr.left);
			float ohh = (float)(ocr.bottom - ocr.top);
			if (ow > 1.f && ohh > 1.f)
			{
				sx = ow / viewport.x;
				sy = ohh / viewport.y;
			}
		}
	}

	static const uintptr_t s_mod = g_Memory.GetModuleBase();
	uintptr_t ve = g_Memory.Read<uintptr_t>(s_mod + ::VisualEngine::Pointer);
	if (!g_Memory.IsValid(ve))
	{
		return;
	}

	Matrix4x4 vm = g_Memory.Read<Matrix4x4>(ve + ::VisualEngine::ViewMatrix);
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl)
	{
		return;
	}

	std::uint64_t lp = g_Memory.Read<std::uint64_t>(
		Globals::Players->address + ::Player::LocalPlayer);
	std::uint64_t local_team = 0;
	if (g_Settings.misc.teamcheck)
	{
		local_team = PlayerHandler::LocalTeamFolder();
	}

	int mode = hb.viz_mode;
	if (mode < 0) mode = 0;
	if (mode > 2) mode = 2;

	PlayerHandler::ForEachPlayer([&](const PlayerCache& c)
	{
		if (c.address == lp)
		{
			return;
		}

		if (g_Settings.misc.teamcheck && PlayerHandler::IsTeammate(c, local_team))
		{
			return;
		}

		std::vector<PartJob> jobs;
		CollectJobs(c, jobs);
		for (const PartJob& j : jobs)
		{
			BasePart bp(j.part->address);
			Vector3 pos = bp.GetPosition();
			// визуал — раздутый размер (как в мире)
			Vector3 sz = bp.GetSize();
			Matrix4x4 rot = bp.GetRotation();
			DrawPartViz(dl, vm, viewport, sx, sy, pos, sz, rot, mode, hb.viz_color);
		}
	});
}

}
}
}
