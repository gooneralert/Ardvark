#pragma once
#include "core/roblox/math/Math.h"

namespace Cheat {
namespace Features {
namespace PhantomSilent {

// PF silent (Camera.Part LookAt) — логику сурса не менять
void SetActive(bool on, const Vector3& world_target = {});

} // namespace PhantomSilent
} // namespace Features
} // namespace Cheat
