#pragma once

// Fly ported from charm-main's movement module (`fly` + its helpers):
//   * `fly`                     (movement.cpp)
//   * `get_camera_fly_basis`    (movement.cpp)
//   * `fly_direction_from_keys` (movement.cpp)
//   * `write_velocity_stable`   (movement.cpp)
// Velocity-based flight. While the bind is engaged the workspace gravity is
// zeroed and the local HumanoidRootPart's velocity is driven each frame from
// the camera basis + WASD/Space/Ctrl keys at (fly_speed * 2.5). The gravity
// is restored once the bind is released or fly is disabled.
// Runs its own worker thread driven by the misc settings.

namespace Cheat {
namespace Features {
namespace Fly {

void Start();
void Stop();

}
}
}
