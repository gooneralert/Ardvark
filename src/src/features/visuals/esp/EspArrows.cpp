#include "pch.h"
#include "EspArrows.h"
#include "EspText.h"
#include "features/visuals/havoc/HavocWorldEsp.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cfloat>

namespace Cheat {
namespace Visuals {
namespace EspArrows {

// 2 pi / 2
static float g_pi = 3.14159265358979323846f;

bool PointOnOverlay(const Vector2& screen, float overlay_w, float overlay_h, float pad)
{
	if (screen.x < -pad || screen.x > overlay_w + pad)
		return false;
	if (screen.y < -pad || screen.y > overlay_h + pad)
		return false;
	return true;
}

float CameraFovDegrees(float raw)
{
	// raw иногда в радианах
	if (raw > 0.f && raw < 3.2f)
		return raw * (180.f / g_pi);

	return raw;
}

bool IsInsideCameraFov(const Vector3& origin, const Matrix4x4& cam_rot,
	const Vector3& target, float fov_deg, float aspect)
{
	Vector3 rel = target - origin;
	float sx = rel.x * cam_rot.m[0][0] + rel.y * cam_rot.m[1][0] + rel.z * cam_rot.m[2][0];
	float sy = rel.x * cam_rot.m[0][1] + rel.y * cam_rot.m[1][1] + rel.z * cam_rot.m[2][1];
	float forward =
		rel.x * (-cam_rot.m[0][2]) + rel.y * (-cam_rot.m[1][2]) + rel.z * (-cam_rot.m[2][2]);
	if (forward <= 0.f)
		return false;

	float fov = fov_deg;
	if (fov < 1.f) fov = 1.f;
	if (fov > 120.f) fov = 120.f;

	float a = aspect;
	if (a < 0.01f) a = 0.01f;

	float half_v = std::tan(fov * 0.5f * (g_pi / 180.f)) * forward;
	float half_h = half_v * a;
	return std::fabs(sx) <= half_h * 1.02f && std::fabs(sy) <= half_v * 1.02f;
}

// стрелка по yaw, сзади вниз, +x вправо от камеры
void ArrowDirYaw(const Vector3& origin, const Matrix4x4& cam_rot,
	const Vector3& target, float& out_dx, float& out_dy)
{
	float lx = -cam_rot.m[0][2];
	float lz = -cam_rot.m[2][2];
	float llen = std::sqrt(lx * lx + lz * lz);
	if (llen < 1e-4f)
	{
		lx = 0.f;
		lz = -1.f;
		llen = 1.f;
	}

	lx /= llen;
	lz /= llen;

	float rx = -lz;
	float rz = lx;
	float dxw = target.x - origin.x;
	float dzw = target.z - origin.z;
	float dx = dxw * rx + dzw * rz;
	float dy = -(dxw * lx + dzw * lz);

	float len = std::sqrt(dx * dx + dy * dy);
	if (len < 1e-4f)
	{
		out_dx = 0.f;
		out_dy = 1.f;
		return;
	}

	out_dx = dx / len;
	out_dy = dy / len;
}

// стрелки когда чел за экраном
void DrawOffscreenArrow(ImDrawList* dl, float overlay_w, float overlay_h,
	float dx, float dy, ImU32 color,
	float size, float radius,
	ImFont* font, float font_size, const char* info_text)
{
	ImVec2 center(overlay_w * 0.5f, overlay_h * 0.5f);
	float max_rx = overlay_w * 0.5f - 18.f;
	float max_ry = overlay_h * 0.5f - 18.f;
	if (max_rx < 40.f) max_rx = 40.f;
	if (max_ry < 40.f) max_ry = 40.f;

	float r = radius;
	if (r < 40.f) r = 40.f;
	float rmax = max_rx;
	if (max_ry > rmax) rmax = max_ry;
	if (r > rmax) r = rmax;

	float tip_x = center.x + dx * r;
	float tip_y = center.y + dy * r;
	float nx = (tip_x - center.x) / max_rx;
	float ny = (tip_y - center.y) / max_ry;
	float el = nx * nx + ny * ny;
	if (el > 1.f)
	{
		float sc = 1.f / std::sqrt(el);
		tip_x = center.x + (tip_x - center.x) * sc;
		tip_y = center.y + (tip_y - center.y) * sc;
	}

	ImVec2 tip(tip_x, tip_y);
	float s = size;
	if (s < 6.f) s = 6.f;
	float px = -dy, py = dx;
	ImVec2 a(tip.x - dx * s + px * s * 0.55f, tip.y - dy * s + py * s * 0.55f);
	ImVec2 b(tip.x - dx * s - px * s * 0.55f, tip.y - dy * s - py * s * 0.55f);
	dl->AddTriangleFilled(tip, a, b, color);
	dl->AddTriangle(tip, a, b, IM_COL32(0, 0, 0, 200), 1.0f);

	if (info_text && info_text[0] && font)
	{
		ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, info_text);
		ImVec2 tp(tip.x + dx * (s * 0.35f + 4.f) - ts.x * 0.5f,
			tip.y + dy * (s * 0.35f + 4.f) - ts.y * 0.5f);

		float xmin = 2.f;
		float xmax = overlay_w - ts.x - 2.f;
		float ymin = 2.f;
		float ymax = overlay_h - ts.y - 2.f;
		if (tp.x < xmin) tp.x = xmin;
		if (tp.x > xmax) tp.x = xmax;
		if (tp.y < ymin) tp.y = ymin;
		if (tp.y > ymax) tp.y = ymax;
		tp.x = std::floor(tp.x);
		tp.y = std::floor(tp.y);
		EspText::DrawTextWithOutline(dl, font, font_size, tp, color, info_text);
	}
}

void BuildArrowInfo(const Cheat::PlayerCache& cache, float dist_studs, char* buf, size_t buf_n)
{
	buf[0] = '\0';
	const auto& ai = Cheat::g_Settings.esp.arrow_info;
	auto append = [&](const char* part) {
		if (!part || !part[0])
			return;
		size_t used = std::strlen(buf);
		if (used == 0)
			std::snprintf(buf, buf_n, "%s", part);

		else
			std::snprintf(buf + used, buf_n - used, " | %s", part);
	};

	if (ai[Cheat::Settings::ARROW_NAME])
	{
		const std::string* shown = &cache.name;
		if (Cheat::g_Settings.esp.name_mode == 0 && !cache.displayName.empty())
			shown = &cache.displayName;
		if (!shown->empty())
			append(shown->c_str());
	}

	if (ai[Cheat::Settings::ARROW_DISTANCE])
	{
		char dbuf[32];
		// havoc — всегда метры
		if (Cheat::Visuals::HavocWorldEsp::IsActivePlace() ||
			Cheat::g_Settings.esp.distance_unit == 1)
			std::snprintf(dbuf, sizeof(dbuf), "%.0fm",
				Cheat::Visuals::HavocWorldEsp::StudsToMeters(dist_studs));

		else
			std::snprintf(dbuf, sizeof(dbuf), "%.0f", dist_studs);
		append(dbuf);
	}

	if (ai[Cheat::Settings::ARROW_HEALTH] && cache.humanoid)
	{
		char hbuf[32];
		std::snprintf(hbuf, sizeof(hbuf), "%.0f",
			Cheat::Humanoid(cache.humanoid->address).GetHealth());
		append(hbuf);
	}

	if (ai[Cheat::Settings::ARROW_TOOL] && !cache.toolName.empty())
		append(cache.toolName.c_str());
}

} // namespace EspArrows
} // namespace Visuals
} // namespace Cheat
