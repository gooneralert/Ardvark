#pragma once
#include "app/Settings.h"
#include "core/roblox/math/Math.h"
#include "core/roblox/classes/Classes.h"
#include <cstdint>

namespace Cheat {
namespace Features {
namespace AimTarget {

	struct Scene {
		bool ok = false;
		Matrix4x4 view{};
		Vector2 viewport{};
		Camera camera{ 0 };
		Vector3 cam_pos{};
		Vector3 local_pos{}; // hrp, для дистанции
		std::uint64_t local_player = 0;
		std::uint64_t local_char = 0;
		std::uint64_t local_team_folder = 0;
	};

	Scene Resolve();
	bool Select(const Settings::AimbotConfig& cfg, const Scene& sc,
		Vector3& out_world, Vector2& out_screen);
	bool HitchanceOk(const Settings::AimbotConfig& cfg);
	void Clear();

	std::uint64_t Current();
	int CurrentPart();
	Vector3 CurrentPoint();

} // namespace AimTarget
} // namespace Features
} // namespace Cheat
