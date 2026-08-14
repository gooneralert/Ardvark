#pragma once

#include "features/visuals/esp/EspLayout.h"
#include "app/Settings.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <vector>

namespace Cheat {
namespace Visuals {
namespace PreviewLayoutDrag {

enum Elem : int {
	ElemNone = -1,
	ElemName = 0,
	ElemDistance,
	ElemTool,
	ElemFlags,
	ElemHealthText,
	ElemHealthBar,
	ElemCount
};

struct ElemHit {
	int id{ ElemNone };
	ImRect rect{};
};

inline int* SidePtrFor(int id, Settings& s)
{
	if (id == ElemName) return &s.esp.name_side;
	if (id == ElemDistance) return &s.esp.distance_side;
	if (id == ElemTool) return &s.esp.tool_side;
	if (id == ElemFlags) return &s.esp.flags_side;
	if (id == ElemHealthText) return &s.esp.health_text_side;
	if (id == ElemHealthBar) return &s.esp.healthbar_side;
	return nullptr;
}

inline float* OffPtrFor(int id, Settings& s)
{
	if (id == ElemName) return &s.esp.name_off;
	if (id == ElemDistance) return &s.esp.distance_off;
	if (id == ElemTool) return &s.esp.tool_off;
	if (id == ElemFlags) return &s.esp.flags_off;
	if (id == ElemHealthText) return &s.esp.health_text_off;
	return nullptr;
}

inline ImRect HitRectFor(const std::vector<ElemHit>& hits, int id)
{
	for (const auto& h : hits) {
		if (h.id == id) return h.rect;
	}
	return {};
}

inline const char* ElemLabel(int id)
{
	if (id == ElemName) return "name";
	if (id == ElemDistance) return "distance";
	if (id == ElemTool) return "tool";
	if (id == ElemFlags) return "flags";
	if (id == ElemHealthText) return "health text";
	if (id == ElemHealthBar) return "health bar";
	return "";
}

inline void DrawSideHighlight(ImDrawList* dl, const EspLayout::Box& b, int side, ImU32 col)
{
	float t = 2.f;
	if (side == EspLayout::Top)
	{
		dl->AddRectFilled(ImVec2(b.x1, b.y1 - t), ImVec2(b.x2, b.y1 + 1.f), col);
	}

	else if (side == EspLayout::Bottom)
	{
		dl->AddRectFilled(ImVec2(b.x1, b.y2 - 1.f), ImVec2(b.x2, b.y2 + t), col);
	}

	else if (side == EspLayout::Left)
	{
		dl->AddRectFilled(ImVec2(b.x1 - t, b.y1), ImVec2(b.x1 + 1.f, b.y2), col);
	}

	else
	{
		dl->AddRectFilled(ImVec2(b.x2 - 1.f, b.y1), ImVec2(b.x2 + t, b.y2), col);
	}
}

inline int HitTestElems(const std::vector<ElemHit>& hits, const ImVec2& mouse)
{
	for (int i = (int)hits.size() - 1; i >= 0; --i) {
		if (hits[i].rect.Contains(mouse))
			return hits[i].id;
	}
	return ElemNone;
}

} // namespace PreviewLayoutDrag
} // namespace Visuals
} // namespace Cheat

namespace PreviewLayoutDrag = Cheat::Visuals::PreviewLayoutDrag;
