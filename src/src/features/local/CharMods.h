#pragma once

// Avatar appearance modifications ported from charm-main's movement module:
//   * Animation changer (charm's `animationchanger`)
//   * Fake headless      (charm's `fakeheadless`)
//   * Korblox leg        (charm's `korblox`)
// All driven by the misc settings; run as one background worker thread.

namespace Cheat {
namespace Features {
namespace CharMods {

void Start();
void Stop();

int AnimPackCount();
const char* const* AnimPackNames();

}
}
}
