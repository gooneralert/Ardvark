#pragma once
/* =============================================================
 * Runtime MSVC RTTI vtable resolver.
 * -------------------------------------------------------------
 * Resolves a C++ class's primary vtable at runtime from its
 * mangled RTTI name instead of shipping a hard-coded vtable RVA that goes
 * stale on every Roblox update.
 *
 * Path: name string -> TypeDescriptor -> RTTICompleteObjectLocator
 *       -> vtable slot. Same offline recipe as tools/ida/raycast.py
 *       (find_fastcluster_vtable_rva) and the same live recipe already
 *       proven in features/local/Btools.cpp (FindVTableByRtti).
 *
 * NOTE: this scans C++ RTTI (pch/.rdata), NOT the Lua reflection
 * ClassDescriptor/FunctionDescriptors list that Reflect::RaycastSlot
 * walks. FastClusterEntity is an internal Graphics class and has no
 * reflection entry, so the raycast-style name scan can't be used.
 */

#include <cstdint>

namespace Cheat {
namespace Roblox {
namespace Rtti {

// Absolute vtable base address (the value an object's first 8 bytes hold
// for that class). 0 = not found. Caller must cache the result.
std::uintptr_t VTableByRttiName(const char* name);

} // namespace Rtti
} // namespace Roblox
} // namespace Cheat