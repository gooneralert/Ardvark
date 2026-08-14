#include "pch.h"
#include "AimFov.h"
#include "renderer/Renderer.h"
#include "imgui.h"
#include <Windows.h>

namespace Cheat {
namespace Features {
namespace AimFov {

	ImVec2 OverlaySize()
	{
		HWND oh = Renderer::GetHwnd();
		if (oh)
		{
			RECT ocr{};
			if (GetClientRect(oh, &ocr))
			{
				float w = (float)(ocr.right - ocr.left);
				float h = (float)(ocr.bottom - ocr.top);
				if (w < 1.f) w = 1.f;
				if (h < 1.f) h = 1.f;
				return ImVec2(w, h);
			}
		}
		return ImGui::GetIO().DisplaySize;
	}

	ImVec2 CursorClient()
	{
		POINT p{};
		ImVec2 sz = OverlaySize();
		if (!GetCursorPos(&p))
		{
			return ImVec2(sz.x * 0.5f, sz.y * 0.5f);
		}

		HWND overlay = Renderer::GetHwnd();
		if (overlay)
		{
			ScreenToClient(overlay, &p);
		}

		float x = (float)p.x;
		float y = (float)p.y;
		// вне оверлея в центр, похуй
		if (x < 0.f || y < 0.f || x > sz.x || y > sz.y)
		{
			return ImVec2(sz.x * 0.5f, sz.y * 0.5f);
		}

		return ImVec2(x, y);
	}

	ImVec2 Anchor(const Settings::AimbotConfig& cfg)
	{
		ImVec2 sz = OverlaySize();
		if (cfg.fov_position == 1)
		{
			return CursorClient();
		}

		return ImVec2(sz.x * 0.5f, sz.y * 0.5f);
	}

	void Draw(const Settings::AimbotConfig& cfg)
	{
		if (!cfg.fov_enabled)
		{
			return;
		}

		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		if (!dl)
		{
			return;
		}

		ImVec2 center = Anchor(cfg);
		float radius = cfg.fov_size;
		if (radius < 1.f) radius = 1.f;

		ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(
			cfg.fov_outline_color[0], cfg.fov_outline_color[1],
			cfg.fov_outline_color[2], cfg.fov_outline_color[3]));

		if (cfg.fov_style == 1)
		{
			float fill_a = cfg.fov_color[3] * 0.35f;
			float rim_a = cfg.fov_color[3];
			if (rim_a > 1.f) rim_a = 1.f;

			ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(
				cfg.fov_color[0], cfg.fov_color[1], cfg.fov_color[2], fill_a));
			ImU32 rim = ImGui::ColorConvertFloat4ToU32(ImVec4(
				cfg.fov_color[0], cfg.fov_color[1], cfg.fov_color[2], rim_a));
			dl->AddCircleFilled(center, radius, fill, 64);
			dl->AddCircle(center, radius, rim, 64, 1.75f);
		}

		else
		{
			ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
				cfg.fov_color[0], cfg.fov_color[1], cfg.fov_color[2], cfg.fov_color[3]));
			dl->AddCircle(center, radius, col, 64, 1.5f);
		}

		if (cfg.fov_outline)
		{
			dl->AddCircle(center, radius + 1.0f, outline, 64, 2.0f);
			dl->AddCircle(center, radius - 1.0f,
				IM_COL32(0, 0, 0, (int)(cfg.fov_outline_color[3] * 100.0f)), 64, 1.0f);
		}
	}

	void DrawTracer(const Settings::AimbotConfig& cfg, const Vector2& target_screen)
	{
		if (!cfg.tracer)
		{
			return;
		}

		ImDrawList* dl = ImGui::GetForegroundDrawList();
		if (!dl)
		{
			return;
		}

		ImVec2 from = Anchor(cfg);
		ImVec2 to(target_screen.x, target_screen.y);
		ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
			cfg.tracer_color[0], cfg.tracer_color[1],
			cfg.tracer_color[2], cfg.tracer_color[3]));

		dl->AddLine(from, to, IM_COL32(0, 0, 0, (int)(cfg.tracer_color[3] * 180.f)), 2.5f);
		dl->AddLine(from, to, col, 1.25f);
	}

} // namespace AimFov
} // namespace Features
} // namespace Cheat
