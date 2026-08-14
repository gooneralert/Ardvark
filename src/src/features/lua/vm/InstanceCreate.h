#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace InstanceCreate {

bool New(const char* className, std::uint64_t parent, std::uint64_t* out_addr);
bool SetParent(std::uint64_t inst, std::uint64_t parent);

// пишет std::string по адресу поля
bool SetString(std::uint64_t field, const char* text);

// то же, но для Content: адрес строки внутри него, а не начало объекта
bool SetContent(std::uint64_t string_field, const char* text);

int LastFail();

} // namespace InstanceCreate
} // namespace Features
} // namespace Cheat
