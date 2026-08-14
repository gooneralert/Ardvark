#pragma once

struct lua_State;

namespace Cheat {
namespace Features {
namespace LuaFiles {

// Ported from the old luavm environment (luavm_env.cpp): the full
// script-runner filesystem API, rooted at the workspace folder.
void Register(lua_State* L);

} // namespace LuaFiles
} // namespace Features
} // namespace Cheat
