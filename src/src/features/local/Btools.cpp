#include "pch.h"
#include "Btools.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/offsets/manual_offsets.h"
#include "core/globals/Globals.h"
#include "core/console/Console.h"
#include "app/Settings.h"

#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>

namespace Cheat {
namespace Features {

namespace {

// fake tool layout (matches the real tool ctors)
const uintptr_t tool_ref_offset = 0x8;
const long     tool_ref_value   = -1;

// fake shared_ptr control block: game does lock inc [ctrl+8] / exchange [ctrl+12]
const int       control_block_size = 0x20;
const uintptr_t control_field1     = 0x8;
const uintptr_t control_field2     = 0xC;
const uintptr_t control_value1     = 0x7FFFFFF0;
const uintptr_t control_value2     = 1;

// RTTI names for the tools
const char* const hammer_names[] = { ".?AVHammerTool@RBX@@", ".?AVHammerTool@@", nullptr };
const char* const grab_names[]   = { ".?AVGrabTool@RBX@@", ".?AVGrabTool@@", ".?AVDragTool@RBX@@", ".?AVDragTool@@", nullptr };
const char* const clone_names[]  = { ".?AVCloneTool@RBX@@", ".?AVCloneTool@@", nullptr };

uintptr_t g_hammer_vtable = 0;
uintptr_t g_grab_vtable   = 0;
uintptr_t g_clone_vtable  = 0;
uintptr_t g_active_tool   = 0;
uintptr_t g_active_ctrl   = 0;
int       g_active_tool_idx = -1;
uintptr_t g_orig_cur_obj = 0, g_orig_cur_ctrl = 0;
uintptr_t g_orig_stk_obj = 0, g_orig_stk_ctrl = 0;
bool      g_saved        = false;

struct Section { uintptr_t addr; uint32_t size; uint32_t chars; };
bool IsReadable(const Section& s) { return (s.chars & 0x40000000) != 0; }

std::vector<Section> GetPESections()
{
    std::vector<Section> out;
    const uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
    if (!base) return out;
    const uint32_t pe = g_Memory.Read<std::uint32_t>(base + 0x3C);
    // Loose sanity check only. NOTE: MUST NOT require pe >= 0x1000 — Roblox's
    // e_lfanew is often well below 0x1000 (e.g. 0xE8), and the C# reference
    // does no range check at all. A strict floor here returns an empty section
    // list and btools discovery silently finds nothing.
    if (pe < 0x40 || pe > 0x1000000) return out;
    const uint16_t nsec = g_Memory.Read<std::uint16_t>(base + pe + 6);
    const uint16_t opt  = g_Memory.Read<std::uint16_t>(base + pe + 0x14);
    if (!nsec || nsec > 128) return out;
    const uintptr_t table = base + pe + 0x18 + opt;
    for (uint16_t i = 0; i < nsec; ++i)
    {
        const uint32_t vsz = g_Memory.Read<std::uint32_t>(table + (std::size_t)i * 40 + 8);
        const uint32_t va  = g_Memory.Read<std::uint32_t>(table + (std::size_t)i * 40 + 12);
        const uint32_t fl  = g_Memory.Read<std::uint32_t>(table + (std::size_t)i * 40 + 36);
        if (vsz && vsz < 0x10000000)
            out.push_back({ base + va, vsz, fl });
    }
    return out;
}

// RTTI: name string -> type descriptor -> complete object locator -> vtable
uintptr_t FindVTableByRtti(const char* name, const std::vector<Section>& readable)
{
    const uintptr_t baseAddr = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
    if (!baseAddr || !name || !name[0]) return 0;
    const std::size_t nameLen = std::strlen(name);
    uintptr_t typeInfo = 0;
    uint32_t  typeRva  = 0;
    const std::uint32_t chunk = 0x10000;

    // mangled name -> type descriptor (name sits 16 bytes inside)
    for (const auto& sec : readable)
    {
        if (typeInfo) break;
        for (std::uint32_t off = 0; off < sec.size && !typeInfo; off += chunk)
        {
            std::uint32_t rs = std::min<std::uint32_t>(chunk + 256, sec.size - off);
            std::vector<std::uint8_t> b(rs);
            g_Memory.ReadRaw(sec.addr + off, b.data(), rs); // zero-initialised; scan regardless of partial reads (like C#)
            for (std::uint32_t i = 0; i + nameLen + 1 <= rs; ++i)
            {
                bool ok = true;
                for (std::size_t k = 0; k < nameLen; ++k)
                    if (b[i + k] != (std::uint8_t)name[k]) { ok = false; break; }
                if (ok && b[i + nameLen] == 0)
                {
                    typeInfo = sec.addr + off + i - 16;
                    typeRva  = (std::uint32_t)(typeInfo - baseAddr);
                    break;
                }
            }
        }
    }
    if (!typeInfo) return 0;

    // complete object locator: signature 1 + reference to type rva
    uintptr_t col = 0;
    for (const auto& sec : readable)
    {
        if (col) break;
        for (std::uint32_t off = 0; off < sec.size && !col; off += chunk)
        {
            std::uint32_t rs = std::min<std::uint32_t>(chunk, sec.size - off);
            std::vector<std::uint8_t> b(rs);
            g_Memory.ReadRaw(sec.addr + off, b.data(), rs); // scan regardless of partial reads (like C#)
            for (std::uint32_t j = 0; j + 24 <= rs; j += 4)
            {
                const std::uint32_t sig  = *(std::uint32_t*)(b.data() + j);
                const std::uint32_t desc = *(std::uint32_t*)(b.data() + j + 12);
                if (sig == 1 && desc == typeRva) { col = sec.addr + off + j; break; }
            }
        }
    }
    if (!col) return 0;

    // vtable = first 8-byte slot pointing at the col
    for (const auto& sec : readable)
    {
        for (std::uint32_t off = 0; off < sec.size; off += chunk)
        {
            std::uint32_t rs = std::min<std::uint32_t>(chunk, sec.size - off);
            std::vector<std::uint8_t> b(rs);
            g_Memory.ReadRaw(sec.addr + off, b.data(), rs); // scan regardless of partial reads (like C#)
            for (std::uint32_t j = 0; j + 16 <= rs; j += 8)
            {
                if ((std::uintptr_t)(*(std::int64_t*)(b.data() + j)) == col)
                    return sec.addr + off + j + 8;
            }
        }
    }
    return 0;
}

uintptr_t FindFirstVTable(const char* const* names, const std::vector<Section>& readable)
{
    for (int n = 0; names[n]; ++n)
    {
        const uintptr_t v = FindVTableByRtti(names[n], readable);
        if (v) return v;
    }
    return 0;
}

void Discover()
{
    // NOTE: deliberately no one-shot guard. Like the C# reference loop, we
    // re-scan until at least one vtable is found. If the tool types haven't
    // been loaded/mapped yet (e.g. the game is still initialising the build
    // place), a single run would leave every vtable at 0 and the selected
    // btool would never activate.
    auto secs = GetPESections();
    std::vector<Section> rd;
    for (auto& s : secs)
        if (IsReadable(s)) rd.push_back(s);
    static bool s_logged_secs = false;
    if (!s_logged_secs)
    {
        s_logged_secs = true;
        Cheat::Console::Log(Cheat::Console::Color::Cyan,
            "btools: PE e_lfanew ok, sections=%zu readable=%zu",
            secs.size(), rd.size());
    }
    g_hammer_vtable = FindFirstVTable(hammer_names, rd);
    g_grab_vtable   = FindFirstVTable(grab_names, rd);
    g_clone_vtable  = FindFirstVTable(clone_names, rd);
}

uintptr_t MakeControlBlock()
{
    uintptr_t c = g_Memory.Alloc(control_block_size, PAGE_READWRITE);
    if (!c) return 0;
    std::vector<std::uint8_t> z(control_block_size, 0);
    g_Memory.WriteRaw(c, z.data(), z.size());
    g_Memory.Write<std::uintptr_t>(c + control_field1, control_value1);
    g_Memory.Write<std::uintptr_t>(c + control_field2, control_value2);
    return c;
}

uintptr_t MakeNameContainer(const char* name)
{
    const int containerSize = 0x40;
    uintptr_t container = g_Memory.Alloc(containerSize, PAGE_READWRITE);
    if (!container) return 0;
    std::vector<std::uint8_t> z(containerSize, 0);
    g_Memory.WriteRaw(container, z.data(), z.size());

    const std::size_t len = std::strlen(name);
    uintptr_t str = container + ::Instance::Name;
    if (len >= 16)
    {
        uintptr_t heap = g_Memory.Alloc(16, PAGE_READWRITE);
        if (!heap) return 0;
        std::uint8_t buf[16] = {};
        std::memcpy(buf, name, 16);
        g_Memory.WriteRaw(heap, buf, 16);
        g_Memory.Write<std::uintptr_t>(str, heap);
    }
    else
    {
        std::uint8_t sso[16] = {};
        std::memcpy(sso, name, len);
        g_Memory.WriteRaw(str, sso, 16);
    }
    g_Memory.Write<std::uintptr_t>(str + 0x18, len);
    g_Memory.Write<std::uintptr_t>(str + 0x20, 15);
    return container;
}

uintptr_t ResolveWorkspace()
{
    if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
        return 0;
    const uintptr_t local = g_Memory.Read<std::uintptr_t>(
        Cheat::Globals::Players->address + ::Player::LocalPlayer);
    if (!g_Memory.IsValid(local)) return 0;
    const uintptr_t mouse = g_Memory.Read<std::uintptr_t>(local + ::Player::Mouse);
    if (!g_Memory.IsValid(mouse)) return 0;
    return g_Memory.Read<std::uintptr_t>(mouse + ::PlayerMouse::Workspace);
}

bool ActivateWithVTable(uintptr_t workspace, uintptr_t vtable)
{
    if (!workspace || !vtable) return false;
    const uintptr_t modBase = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
    if (!modBase) return false;
    const uintptr_t modEnd = modBase + 0x10000000;
    if (vtable < modBase || vtable >= modEnd) return false;

    if (!g_saved)
    {
        g_orig_cur_obj  = g_Memory.Read<std::uintptr_t>(workspace + ManualOffsets::Btools::WorkspaceCurrentCommand);
        g_orig_cur_ctrl = g_Memory.Read<std::uintptr_t>(workspace + ManualOffsets::Btools::WorkspaceCurrentCommand + 8);
        g_orig_stk_obj  = g_Memory.Read<std::uintptr_t>(workspace + ManualOffsets::Btools::WorkspaceStickyCommand);
        g_orig_stk_ctrl = g_Memory.Read<std::uintptr_t>(workspace + ManualOffsets::Btools::WorkspaceStickyCommand + 8);
        g_saved = true;
    }

    const int toolSize = ManualOffsets::Btools::ToolAllocationSize;
    uintptr_t tool = g_Memory.Alloc((SIZE_T)toolSize, PAGE_READWRITE);
    if (!tool) return false;
    std::vector<std::uint8_t> z((std::size_t)toolSize, 0);
    g_Memory.WriteRaw(tool, z.data(), z.size());

    g_Memory.Write<std::uintptr_t>(tool, vtable);
    g_Memory.Write<std::int64_t>(tool + tool_ref_offset, tool_ref_value);
    g_Memory.Write<std::uintptr_t>(tool + ManualOffsets::Btools::MouseCommandWorkspace, workspace);

    const uintptr_t nc = MakeNameContainer("Tool");
    if (!nc) return false;
    g_Memory.Write<std::uintptr_t>(tool + ::Instance::NameContainer, nc);

    const uintptr_t ctrl = MakeControlBlock();
    if (!ctrl) return false;

    // shared_ptr pair per slot: [object @ Command, ctrl block @ Command + 8]
    g_Memory.Write<std::uintptr_t>(workspace + ManualOffsets::Btools::WorkspaceCurrentCommand, tool);
    g_Memory.Write<std::uintptr_t>(workspace + ManualOffsets::Btools::WorkspaceCurrentCommand + 8, ctrl);
    g_Memory.Write<std::uintptr_t>(workspace + ManualOffsets::Btools::WorkspaceStickyCommand, tool);
    g_Memory.Write<std::uintptr_t>(workspace + ManualOffsets::Btools::WorkspaceStickyCommand + 8, ctrl);

    g_active_tool = tool;
    g_active_ctrl = ctrl;
    return true;
}

void Deactivate()
{
    // Restore the original workspace command pointers that we saved on the
    // first activation (mirrors btools.cs Deactivate()).
    if (g_saved)
    {
        const uintptr_t ws = ResolveWorkspace();
        if (ws)
        {
            g_Memory.Write<std::uintptr_t>(ws + ManualOffsets::Btools::WorkspaceCurrentCommand, g_orig_cur_obj);
            g_Memory.Write<std::uintptr_t>(ws + ManualOffsets::Btools::WorkspaceCurrentCommand + 8, g_orig_cur_ctrl);
            g_Memory.Write<std::uintptr_t>(ws + ManualOffsets::Btools::WorkspaceStickyCommand, g_orig_stk_obj);
            g_Memory.Write<std::uintptr_t>(ws + ManualOffsets::Btools::WorkspaceStickyCommand + 8, g_orig_stk_ctrl);
        }
    }

    // NOTE: deliberately do NOT free the allocated fake tool / control block /
    // name container. The game still holds shared_ptr references to them and
    // freeing them causes a use-after-free (crash / tool stops being applied).
    // This is intentionally leaked, exactly like the C# reference method.
    g_active_tool = 0;
    g_active_ctrl = 0;
    g_saved = false;
}

} // namespace

void Btools::Tick()
{
    auto& m = Cheat::g_Settings.misc;

    // rising edge on the toggle -> proves the local tab is linked to this tick
    static bool s_was_on = false;
    if (m.bTools && !s_was_on)
        Cheat::Console::Log(Cheat::Console::Color::Green, "btools: toggled ON (local tab linked)");
    s_was_on = m.bTools;

    if (!m.bTools)
    {
        if (g_active_tool != 0)
            Deactivate();
        g_active_tool_idx = -1; // like C# last_activated_tool = -1 on disable
        return;
    }

    // Discover the tool vtables if not found yet (retried until found, like the
    // C# loop). The scan only runs once they're all found, so it isn't done
    // every frame while active.
    if (g_hammer_vtable == 0 && g_grab_vtable == 0 && g_clone_vtable == 0)
    {
        Discover();
        static bool s_logged_disc = false;
        if ((g_hammer_vtable || g_grab_vtable || g_clone_vtable) && !s_logged_disc)
        {
            s_logged_disc = true;
            Cheat::Console::Log(Cheat::Console::Color::Cyan,
                "btools: vtables hammer=%p grab=%p clone=%p",
                (void*)g_hammer_vtable, (void*)g_grab_vtable, (void*)g_clone_vtable);
        }
        if (!(g_hammer_vtable || g_grab_vtable || g_clone_vtable))
        {
            static bool s_logged_nofind = false;
            if (!s_logged_nofind)
            {
                s_logged_nofind = true;
                Cheat::Console::Log(Cheat::Console::Color::Red,
                    "btools: RTTI discovery found NO tool vtables");
            }
            return; // nothing found yet, will retry next tick
        }
    }

    const uintptr_t ws = ResolveWorkspace();
    if (!ws)
    {
        static bool s_logged_ws = false;
        if (!s_logged_ws)
        {
            s_logged_ws = true;
            Cheat::Console::Log(Cheat::Console::Color::Yellow,
                "btools: workspace resolve FAILED (no workspace)");
        }
        return;
    }

    // Only (re)activate when the tool selection changed or nothing is active yet
    // (selected tool index != last activated index).
    if (m.bToolsTool != g_active_tool_idx)
    {
        // Deactivate the current tool (back to the original command) first.
        if (g_active_tool != 0)
            Deactivate();

        uintptr_t vtable = 0;
        if (m.bToolsTool == 0)      vtable = g_hammer_vtable;
        else if (m.bToolsTool == 1) vtable = g_grab_vtable;
        else if (m.bToolsTool == 2) vtable = g_clone_vtable;

        static const char* tool_names[] = { "hammer", "grab", "clone" };
        if (ActivateWithVTable(ws, vtable))
        {
            Cheat::Console::Log(Cheat::Console::Color::Green,
                "btools: %s ACTIVATED tool=%p ws=%p",
                tool_names[m.bToolsTool], (void*)g_active_tool, (void*)ws);
            g_active_tool_idx = m.bToolsTool;
        }
        else
        {
            Cheat::Console::Log(Cheat::Console::Color::Red,
                "btools: %s ACTIVATE FAILED (vtable=%p ws=%p)",
                tool_names[m.bToolsTool], (void*)vtable, (void*)ws);
        }
    }
}

void Btools::Shutdown()
{
    if (g_active_tool != 0)
        Deactivate();
    g_hammer_vtable = g_grab_vtable = g_clone_vtable = 0;
}

} // namespace Features
} // namespace Cheat