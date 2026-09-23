#pragma once
#include <d3d11.h>
#include <imgui.h>

// Slight static backdrop blur. Tint/alpha is unchanged — only what's behind is softened.
namespace backdrop_blur
{
	bool init(ID3D11Device* device, ID3D11DeviceContext* context);
	void shutdown();
	void update();
	void set_paused(bool paused);
	void fill(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, ImU32 tint);
}
