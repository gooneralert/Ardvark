#pragma once

struct lua_State;
struct Vector3;
struct Matrix4x4;

namespace Cheat {
namespace Features {
namespace LuaTypes {

void Register(lua_State* L);

void PushVector3(lua_State* L, const Vector3& v);
bool ToVector3(lua_State* L, int idx, Vector3& out);

void PushVector2(lua_State* L, float x, float y);
bool ToVector2(lua_State* L, int idx, float& x, float& y);

void PushColor3(lua_State* L, float r, float g, float b);
bool ToColor3(lua_State* L, int idx, float& r, float& g, float& b);

void PushCFrame(lua_State* L, const Vector3& pos, const Matrix4x4& rot);
bool ToCFrame(lua_State* L, int idx, Vector3& pos, Matrix4x4& rot);

} // namespace LuaTypes
} // namespace Features
} // namespace Cheat
