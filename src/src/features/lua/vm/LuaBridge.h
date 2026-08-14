#pragma once

#include <cstdint>

struct lua_State;

namespace Cheat {
namespace Features {
namespace LuaBridge {

void Register(lua_State* L);
void PushInstance(lua_State* L, std::uint64_t address);
void RefreshGlobals(lua_State* L);

// throws lua error if not Instance userdata
std::uint64_t CheckAddress(lua_State* L, int idx = 1);

} // namespace LuaBridge
} // namespace Features
} // namespace Cheat
