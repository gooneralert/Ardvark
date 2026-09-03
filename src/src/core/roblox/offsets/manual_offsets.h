#pragma once
/* =============================================================
/*                      jewsploit manual offsets
/* -------------------------------------------------------------
/*  Hand-managed offsets that are NOT part of the auto-generated
/*  Offsets.h. As of the engine-chams RTTI migration this file only
/*  holds the Btools group; the former FastClusterEntity / MaterialLayer /
/*  TechniqueArray / RenderQueue groups were moved into the engine-chams
/*  feature itself (src/features/visuals/EngineChamsOffsets.h).
/* =============================================================
*/

#include <cstdint>

namespace ManualOffsets {

    // Btools — Workspace MouseCommand shared_ptr pipeline (FoulzExternal dump).
    // Each command slot is a shared_ptr pair: object ptr at the offset, control
    // block ptr at offset + 8 (there is no separate refcount constant).
    namespace Btools {
        inline constexpr uintptr_t WorkspaceCurrentCommand  = 0x8A0;
        inline constexpr uintptr_t WorkspaceStickyCommand   = 0x8B0;
        inline constexpr uintptr_t MouseCommandWorkspace    = 0x50;
        inline constexpr int       ToolAllocationSize       = 0xD0;
    }

} // namespace ManualOffsets