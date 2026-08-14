#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace CallGate {

// подменяем указатель реализации у BoundFuncDesc и ждём, пока движок
// сам дёрнет метод — тогда наш stub исполнит одну команду на его потоке.
// Hyperion сторожит .text, поэтому inline-хуки исключены: пишем только
// в .data и держим stub в чужой dll / xrw-странице.

bool Install(const char* method_name = nullptr);
void Remove();
bool Ready();

// rcx/rdx/r8/r9 = a0..a3, результат из rax
bool Invoke(std::uint64_t fn,
            std::uint64_t a0, std::uint64_t a1,
            std::uint64_t a2, std::uint64_t a3,
            std::uint64_t* out_ret, unsigned timeout_ms = 3000);

// буфер в процессе под out-параметры (>= 256 байт)
std::uint64_t Scratch();

// сколько раз слот дёрнулся с момента установки — так меряем, горячий ли он
std::uint64_t Calls();

std::uint64_t SlotAddress();
int LastFail();

} // namespace CallGate
} // namespace Features
} // namespace Cheat
