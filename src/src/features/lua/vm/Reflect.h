#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace Reflect {

// движок интернирует каждую строку (имя класса, имя метода) в RBX::Name
// и дальше везде таскает только указатель. таблицы читаются снаружи
std::uint64_t Name(std::uintptr_t base, const char* text);
std::uint64_t Creator(std::uintptr_t base, std::uint64_t name);

// New (migrated): scan the Creators DenseHashMap directly by class name and
// return the ICreator (class_desc + ClassDescriptor::Creator). No name interning.
std::uint64_t CreatorByName(std::uintptr_t base, const char* className);

// New (migrated): get a class's ClassDescriptor from the Creators map (guide
// "Instance.new") by class name. Used by CallGate to scan instance methods.
std::uint64_t ClassDescriptorByName(std::uintptr_t base, const char* className);

// New (migrated): scan a class descriptor's FunctionDescriptors list for a
// named function (guide "find_first_func") and return its FunctionDescriptor.
std::uint64_t FindFunction(std::uintptr_t class_desc, const char* name);

// New (migrated): resolve the WorldRoot "Raycast" function slot (the address of
// the bound function pointer that raycast hooks patch) by scanning WorldRoot's
// FunctionDescriptors. No hard-coded descriptor RVA.
std::uintptr_t RaycastSlot();

} // namespace Reflect
} // namespace Features
} // namespace Cheat
