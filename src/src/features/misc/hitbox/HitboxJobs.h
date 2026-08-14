#pragma once

#include "core/player/PlayerHandler.h"
#include "core/roblox/classes/Classes.h"

#include <memory>
#include <vector>

namespace Cheat {
namespace Features {
namespace HitboxExpander {

struct PartJob {
	std::shared_ptr<Instance> part;
	float scale;
};

void CollectJobs(const PlayerCache& c, std::vector<PartJob>& out);

}
}
}
