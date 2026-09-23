#pragma once
// -----------------------------------------------------------------------------
// liquid_ui.h - entry point for the vendored LiquidUI (liquidDX11) glass kit.
//
// The kit is checked in under src/gui/LiquidUI and is compiled straight from
// there (see jewsploit.vcxproj):
//
//   glass/glass.cpp      Glass::Renderer  - DX11 liquid/frost glass shader
//                                           renderer + the widget kit
//                                           (buttons, toggles, sliders,
//                                           sidebar nav, modals, ...)
//   glass/backdrop.cpp   Glass::Backdrop  - DXGI output duplication capture of
//                                           the desktop behind the overlay +
//                                           the down/up gaussian blur chain
//   glass/surfaces.cpp   the extra glass "surfaces" (dock/launchpad/...)
//
// Project code only ever includes THIS header (and gui/glass.h, which wraps
// the renderer for the overlay windows).
// -----------------------------------------------------------------------------
#include "LiquidUI/examples/example_win32_directx11/src/glass/backdrop.h"
#include "LiquidUI/examples/example_win32_directx11/src/glass/glass.h"
