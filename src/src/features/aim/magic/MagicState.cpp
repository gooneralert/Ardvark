#include "pch.h"
#include "MagicState.h"

namespace Cheat {
namespace Features {
namespace MagicBullet {
namespace mb {

	Hook g_hook{};
	bool g_wallbang = false;
	std::chrono::steady_clock::time_point g_lastFail{};

} // namespace mb
} // namespace MagicBullet
} // namespace Features
} // namespace Cheat
