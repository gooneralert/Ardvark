#pragma once
#include <cstdint>

namespace Cheat::Console {
    enum class Color : unsigned short {
        Default   = 0x07,
        Dim       = 0x08,
        Gray      = 0x07,
        White     = 0x0F,
        Red       = 0x0C,
        Green     = 0x0A,
        Blue      = 0x09,
        Cyan      = 0x0B,
        Magenta   = 0x0D,
        Yellow    = 0x0E,
        Orange    = 0x06,
        Teal      = 0x03,
        Purple    = 0x05,
        Lime      = 0x02,
        Sky       = 0x0B,
        Pink      = 0x0D,
        BrightRed = 0x4C,
    };

    void Clear();
    void Log(Color color, const char* fmt, ...);
    void Log(const char* fmt, ...);
    void Ptr(Color color, const char* name, std::uint64_t addr);
    void Ptr(const char* name, std::uint64_t addr);
    void DumpWorld();
    void DumpSilent(bool ok, std::uint64_t handler, std::uint64_t stub,
                    std::uint64_t state, std::uint64_t slot, const char* detail);
    void DumpGate(bool ok, const char* method, std::uint64_t slot,
                  std::uint64_t handler, std::uint64_t stub,
                  std::uint64_t state, bool cave, int fail);
    void GateTimeout(const char* method, std::uint64_t calls);
    void DumpCrash(unsigned long exit_code);
    void DumpLastCrash();
}
