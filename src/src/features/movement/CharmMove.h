#pragma once

// Movement features ported from charm-main's movement module:
//   * Gravity override      (charm's `gravity_override`)
//   * Tickrate manipulation (charm's `tickrate_manipulation`)
// (The separate Fly feature is the charm `fly` ported to Fly.cpp.)
// Each feature runs its own worker thread driven by the misc settings.

namespace Cheat {
namespace Features {
namespace CharmMove {

void GravityStart();
void GravityStop();

void TickrateStart();
void TickrateStop();

}
}
}
