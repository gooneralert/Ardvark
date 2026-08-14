#pragma once
#include "app/Settings.h"
#include "core/roblox/math/Math.h"
#include "imgui.h"

namespace Cheat {
namespace Features {
namespace AimFov {

	ImVec2 OverlaySize();
	ImVec2 CursorClient();
	ImVec2 Anchor(const Settings::AimbotConfig& cfg);
	void Draw(const Settings::AimbotConfig& cfg);
	void DrawTracer(const Settings::AimbotConfig& cfg, const Vector2& target_screen);

} // namespace AimFov
} // namespace Features
} // namespace Cheat
