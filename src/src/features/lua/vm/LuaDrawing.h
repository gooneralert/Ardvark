#pragma once

struct lua_State;

namespace Cheat {
namespace Features {
namespace LuaDrawing {

void Register(lua_State* L);
void Clear();
void Render();

} // namespace LuaDrawing
} // namespace Features
} // namespace Cheat
