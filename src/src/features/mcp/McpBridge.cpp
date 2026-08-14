#include "pch.h"
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "McpBridge.h"

#pragma comment(lib, "ws2_32.lib")

#undef GetClassName

#include "execute/Server.h"

namespace Cheat {
namespace Features {
namespace McpBridge {

//constexpr unsigned short k_port = 3847;
//constexpr int k_max_children = 256;
//constexpr int k_max_search = 80;
//constexpr int k_max_depth = 8;

void Start()
{
	if (detail::g_run.exchange(true))
		return;

	detail::g_thread = std::thread(detail::Loop);
}

void Stop()
{
	if (!detail::g_run.exchange(false))
		return;

	detail::CloseListen();
	if (detail::g_thread.joinable())
		detail::g_thread.join();
}

bool Running()
{
	return detail::g_run.load(std::memory_order_relaxed);
}

}
}
}
