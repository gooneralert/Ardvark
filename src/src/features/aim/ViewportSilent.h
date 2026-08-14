#pragma once
#include "core/roblox/math/Math.h"

namespace Cheat {
namespace Features {
namespace ViewportSilent {

// writer thread долбит Camera::Viewport
void SetActive(bool on, const Vector3& world_target = {});
void Restore();
void Shutdown();
bool Aiming();

} // namespace ViewportSilent
} // namespace Features
} // namespace Cheat
