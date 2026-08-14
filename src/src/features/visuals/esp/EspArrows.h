#pragma once

#include "core/player/PlayerHandler.h"
#include "core/roblox/math/Math.h"
#include "imgui.h"

#include <cstddef>

namespace Cheat {
namespace Visuals {
namespace EspArrows {

bool PointOnOverlay(const Vector2& screen, float overlay_w, float overlay_h, float pad);

float CameraFovDegrees(float raw);

bool IsInsideCameraFov(const Vector3& origin, const Matrix4x4& cam_rot,
	const Vector3& target, float fov_deg, float aspect);

void ArrowDirYaw(const Vector3& origin, const Matrix4x4& cam_rot,
	const Vector3& target, float& out_dx, float& out_dy);

void DrawOffscreenArrow(ImDrawList* dl, float overlay_w, float overlay_h,
	float dx, float dy, ImU32 color,
	float size, float radius,
	ImFont* font, float font_size, const char* info_text);

void BuildArrowInfo(const Cheat::PlayerCache& cache, float dist_studs, char* buf, size_t buf_n);

} // namespace EspArrows
} // namespace Visuals
} // namespace Cheat
