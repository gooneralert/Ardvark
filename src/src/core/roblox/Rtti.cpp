#include "pch.h"
#include "Rtti.h"

#include "core/memory/Memory.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Cheat {
namespace Roblox {
namespace Rtti {
namespace {

struct Section
{
    uintptr_t addr;
    uint32_t size;
    uint32_t chars;
};

// IMAGE_SCN_MEM_READ
bool IsReadable(const Section& s)
{
    return (s.chars & 0x40000000) != 0;
}

std::vector<Section> ReadableSections()
{
    std::vector<Section> out;
    const uintptr_t baseAddr = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
    if (!baseAddr)
        return out;

    // PE header walk (same as Btools::GetPESections).
    const std::uint32_t pe = g_Memory.Read<std::uint32_t>(baseAddr + 0x3C);
    if (pe < 0x40 || pe > 0x1000000)
        return out;
    const std::uint16_t nsec = g_Memory.Read<std::uint16_t>(baseAddr + pe + 6);
    const std::uint16_t opt = g_Memory.Read<std::uint16_t>(baseAddr + pe + 0x14);
    if (!nsec || nsec > 128)
        return out;

    const uintptr_t table = baseAddr + pe + 0x18 + opt;
    for (std::uint16_t i = 0; i < nsec; ++i)
    {
        const std::uint32_t vsz = g_Memory.Read<std::uint32_t>(table + (std::size_t)i * 40 + 8);
        const std::uint32_t va = g_Memory.Read<std::uint32_t>(table + (std::size_t)i * 40 + 12);
        const std::uint32_t fl = g_Memory.Read<std::uint32_t>(table + (std::size_t)i * 40 + 36);
        if (vsz && vsz < 0x10000000)
            out.push_back({ baseAddr + va, vsz, fl });
    }
    return out;
}

} // namespace

// MSVC RTTI: name string -> type descriptor (-0x10) -> COL (sig==1,
// +0x0C == type descriptor RVA) -> first 8-byte slot pointing at the COL.
std::uintptr_t VTableByRttiName(const char* name)
{
    if (!name || !name[0] || !g_Memory.IsAttached())
        return 0;

    const uintptr_t baseAddr = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
    const auto readable = ReadableSections();
    if (!baseAddr || readable.empty())
        return 0;

    const std::size_t nameLen = std::strlen(name);
    constexpr std::uint32_t kChunk = 0x10000;

    // 1) mangled name string (the type descriptor starts 0x10 bytes before it)
    uintptr_t typeInfo = 0;
    std::uint32_t typeRva = 0;
    for (const auto& sec : readable)
    {
        if (typeInfo)
            break;
        for (std::uint32_t off = 0; off < sec.size && !typeInfo; off += kChunk)
        {
            const std::uint32_t rs = std::min<std::uint32_t>(kChunk + 256, sec.size - off);
            std::vector<std::uint8_t> b(rs);
            g_Memory.ReadRaw(sec.addr + off, b.data(), rs);
            for (std::uint32_t i = 0; i + nameLen + 1 <= rs; ++i)
            {
                if (std::memcmp(b.data() + i, name, nameLen) == 0 && b[i + nameLen] == 0)
                {
                    typeInfo = sec.addr + off + i - 0x10;
                    typeRva = (std::uint32_t)(typeInfo - baseAddr);
                    break;
                }
            }
        }
    }
    if (!typeInfo)
        return 0;

    // 2) RTTICompleteObjectLocator: signature == 1, +0x0C == pTypeDescriptor RVA
    uintptr_t col = 0;
    for (const auto& sec : readable)
    {
        if (col)
            break;
        for (std::uint32_t off = 0; off < sec.size && !col; off += kChunk)
        {
            const std::uint32_t rs = std::min<std::uint32_t>(kChunk, sec.size - off);
            std::vector<std::uint8_t> b(rs);
            g_Memory.ReadRaw(sec.addr + off, b.data(), rs);
            for (std::uint32_t j = 0; j + 24 <= rs; j += 4)
            {
                const std::uint32_t sig = *(std::uint32_t*)(b.data() + j);
                const std::uint32_t desc = *(std::uint32_t*)(b.data() + j + 12);
                if (sig == 1 && desc == typeRva)
                {
                    col = sec.addr + off + j;
                    break;
                }
            }
        }
    }
    if (!col)
        return 0;

    // 3) vtable = first 8-byte slot whose value is the COL address; the slot
    // itself is vtable[-1], so the vtable base is the next qword.
    for (const auto& sec : readable)
    {
        for (std::uint32_t off = 0; off < sec.size; off += kChunk)
        {
            const std::uint32_t rs = std::min<std::uint32_t>(kChunk, sec.size - off);
            std::vector<std::uint8_t> b(rs);
            g_Memory.ReadRaw(sec.addr + off, b.data(), rs);
            for (std::uint32_t j = 0; j + 16 <= rs; j += 8)
            {
                if ((std::uintptr_t)(*(std::int64_t*)(b.data() + j)) == col)
                    return sec.addr + off + j + 8;
            }
        }
    }
    return 0;
}

} // namespace Rtti
} // namespace Roblox
} // namespace Cheat