#pragma once

// Movement features ported from charm-main's movement module:
//   * Gravity override      (charm's `gravity_override`)
//   * Tickrate manipulation (charm's `tickrate_manipulation`)
// Each feature runs its own worker thread driven by the misc settings.
// (The charm `cframe` feature was removed in favor of the two-method fly
// ported from FoulzExternal's flight.cs.)

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
