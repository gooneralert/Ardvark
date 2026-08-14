#pragma once

#include <string>

namespace Cheat {
namespace Features {
namespace LuaPreprocess {

// Convert Luau-only syntax into Lua 5.4 before compiling.
// Currently: rewrites Luau `continue` into a `goto`/label pair (ported from
// the old script runner's PreprocessLua), so Matcha-style scripts load in the
// standard-Lua VM.
std::string Preprocess(const std::string& src);

} // namespace LuaPreprocess
} // namespace Features
} // namespace Cheat
