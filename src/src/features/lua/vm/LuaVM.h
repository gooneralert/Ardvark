#pragma once

#include <string>

struct lua_State;

namespace Cheat {
namespace Features {
namespace LuaVM {

bool Initialize();
void Shutdown();

// true = ok; ошибка уже в output executor
bool Execute(const std::string& source, const char* chunk_name = "script");

lua_State* State();
bool Ready();

// resume yields (task.wait) — крутит фоновый тикер из Initialize
void Tick(float dt);

// только в очередь resume (без yield) — потом lua_yield / lua_yieldk
void ScheduleWait(lua_State* L, float sec);

// kind: 0 hb  1 PlayerAdded  2 PlayerRemoving  3 CharacterAdded
// 4 InputBegan  5 InputEnded  6 ChildAdded  7 ChildRemoved  8 DescendantAdded  9 PropChanged
// 10 BindableEvent.Event  11 OnClientEvent  12 ProximityPrompt.Triggered
void PushSignal(lua_State* L, int kind, std::uint64_t owner = 0, const char* prop = nullptr);

// args уже на L (self + ...); локальный fire, не RemoteEvent
void FireSignal(lua_State* L, int kind, std::uint64_t owner, const char* prop);

} // namespace LuaVM
} // namespace Features
} // namespace Cheat
