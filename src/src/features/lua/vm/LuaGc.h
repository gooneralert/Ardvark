#pragma once

struct lua_State;

namespace Cheat {
namespace Features {
namespace LuaGc {

void Register(lua_State* L);
void Stop();

} // namespace LuaGc
} // namespace Features
} // namespace Cheat
