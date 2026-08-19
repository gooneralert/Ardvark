#pragma once
/* =============================================================
/*                      jewsploit manual offsets
/* -------------------------------------------------------------
/*  Hand-managed offsets that are NOT part of the auto-generated
/*  Offsets.h (e.g. engine-chams internals that public dumpers
/*  like the geeg source don't provide). Add new groups here as
/*  needed; engine features can read them instead of Offsets.h.
/* =============================================================
*/

#include <cstdint>

namespace ManualOffsets {

    // Engine chams — FastClusterEntity (entity-level fields).
    // Source: custom dump.
    namespace FastClusterEntity {
        inline constexpr uintptr_t VTableRva              = 0x6AE1C10;
        inline constexpr uintptr_t ContextPtr             = 0x8;
        inline constexpr uintptr_t RenderQueueId          = 0x10;
        inline constexpr uintptr_t AlphaByte              = 0x14;
        inline constexpr uintptr_t MaterialPtr            = 0x20;
        inline constexpr uintptr_t DecalMaterialPtr       = 0x48;
        inline constexpr uintptr_t TechniqueArrayPtr      = 0x70;
        inline constexpr uintptr_t PrimitiveIndexArrayPtr = 0x80;
        inline constexpr uintptr_t BBoxMinX               = 0x98;
        inline constexpr uintptr_t BBoxMinY               = 0x9C;
        inline constexpr uintptr_t BBoxMinZ               = 0xA0;
        inline constexpr uintptr_t BBoxMaxX               = 0xA4;
        inline constexpr uintptr_t BBoxMaxY               = 0xA8;
        inline constexpr uintptr_t BBoxMaxZ               = 0xAC;
        inline constexpr uintptr_t BeginOffset            = 0x0;
        inline constexpr uintptr_t EndOffset              = 0x8;
        inline constexpr uintptr_t EntryStride            = 136;
        inline constexpr uintptr_t Stride                 = 0x88;

        // Chams-internal sub-structures (used by the standalone EngineChams path).
        // Values carried from the old header; re-validate against a fresh dump.
        namespace Context {
            inline constexpr uintptr_t PrimitivePoolPtr   = 0x1a0;
        }
        namespace PrimitivePool {
            inline constexpr uintptr_t ArrayBase          = 0x20;
        }
        namespace PrimitiveRecord {
            inline constexpr uintptr_t Stride             = 0x30;
            inline constexpr uintptr_t Translation        = 0x24;
        }
    }

    // Material layer fields — the engine-chams code reads these via
    // MaterialLayer:: (your dump grouped them under FastClusterEntity;
    // kept as a separate group here so the existing code maps cleanly).
    namespace MaterialLayer {
        inline constexpr uintptr_t FillModeByte           = 0x11;
        inline constexpr uintptr_t MatFlags               = 0x18;
        inline constexpr uintptr_t Param                  = 0x1C;
        inline constexpr uintptr_t Flags2                 = 0x20;
        inline constexpr uintptr_t ColorData              = 0x24;
        inline constexpr uintptr_t Stride                 = 0x88;
    }

// Technique array of a primitive — begin/end pointer slots (chams walk
    // this to find material layers). Values carried over from the old header.
    namespace TechniqueArray {
        inline constexpr uintptr_t BeginOffset           = 0x0;
        inline constexpr uintptr_t EndOffset             = 0x8;
    }

// Render-queue bucket ids used by engine chams (StyleQueue). Carried over from
    // the old header — geeg exposes RenderJob instead of RenderQueue.
    namespace RenderQueue {
        inline constexpr uintptr_t Opaque                 = 0x0;
        inline constexpr uintptr_t Terrain                = 0x1;
        inline constexpr uintptr_t Decals                 = 0x2;
        inline constexpr uintptr_t OpaqueCasters          = 0x3;
        inline constexpr uintptr_t OpaqueAdorns           = 0x4;
        inline constexpr uintptr_t OpaqueWithAlpha        = 0x5;
        inline constexpr uintptr_t Water                  = 0x6;
        inline constexpr uintptr_t GlassTint              = 0x7;
        inline constexpr uintptr_t Glass                  = 0x8;
        inline constexpr uintptr_t Transparent            = 0x9;
        inline constexpr uintptr_t TransparentCasters     = 0xa;
        inline constexpr uintptr_t OnTopWithDepth         = 0xb;
        inline constexpr uintptr_t OnTopReadOnlyDepth     = 0xc;
        inline constexpr uintptr_t AlwaysOnTop            = 0xd;
        inline constexpr uintptr_t AlwaysOnTopAdorns      = 0xe;
        inline constexpr uintptr_t Screen                 = 0xf;
        inline constexpr uintptr_t ScreenOnTopOfBlur      = 0x10;
    }

    // GuiBase2D — manual override while the auto-generated Offsets.h value is
    // wrong. Auto header still keeps AbsolutePosition = 0x108; code paths that
    // used to read it now use this override (0x10C) until the dumper is fixed.
    namespace GuiBase2D {
        inline constexpr uintptr_t AbsolutePosition = 0x10C;
    }

    // Btools — Workspace MouseCommand shared_ptr pipeline (FoulzExternal dump).
    // Each command slot is a shared_ptr pair: object ptr at the offset, control
    // block ptr at offset + 8 (there is no separate refcount constant).
    namespace Btools {
        inline constexpr uintptr_t WorkspaceCurrentCommand  = 0x878;
        inline constexpr uintptr_t WorkspaceStickyCommand   = 0x888;
        inline constexpr uintptr_t MouseCommandWorkspace    = 0x50;
        inline constexpr int       ToolAllocationSize       = 0xD0;
    }

} // namespace ManualOffsets