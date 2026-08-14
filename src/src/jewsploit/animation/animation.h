#pragma once

#include <imgui.h>

namespace anim
{
	float approach(float cur, float tgt, float speed, float dt);
	ImVec2 approach(const ImVec2& cur, const ImVec2& tgt, float speed, float dt);

	// 0..1 ease out cubic
	float ease_out_cubic(float t);
}
