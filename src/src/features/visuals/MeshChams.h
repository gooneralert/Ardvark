#pragma once

#include "core/roblox/math/Math.h"
#include "imgui.h"

#include <cstdint>

namespace Cheat {
namespace Visuals {
namespace MeshChams {

// color: ImGui tris; shader: очередь в MeshDxShader (Flush до ImGui GPU)
void Draw(
	ImDrawList* dl,
	std::uint64_t character,
	const Matrix4x4& view,
	const Vector2& viewport,
	float scale_x,
	float scale_y,
	ImU32 fill_col);

// outline: каждый тип = стиль фейда + анимация
const char* const* OutlineStyleNames();
int OutlineStyleNameCount();

// bounding type=mesh: экранный/world AABB по реальным мешам (аксы тоже)
bool ExpandBounds(
	std::uint64_t character,
	const Matrix4x4& view,
	const Vector2& viewport,
	float scale_x,
	float scale_y,
	float& min_x, float& max_x,
	float& min_y, float& max_y,
	Vector3& wmin, Vector3& wmax);

} // namespace MeshChams
} // namespace Visuals
} // namespace Cheat
