#pragma once
// -----------------------------------------------------------------------------
// menu_window.h - the main menu's own top-level window.
//
// The fullscreen overlay (renderer/Renderer.cpp) stays for ESP/watermark and
// the launcher navbar and is always click-through, so it never steals input
// from the game. The menu itself lives here: a WS_POPUP window with matcha's
// real DWM blur (SetWindowCompositionAttribute ACCENT_ENABLE_BLURBEHIND +
// DwmExtendFrameIntoClientArea), its own DX11 swapchain and its own ImGui
// context (the menu face is Inter). It is shown/hidden by the menu key.
// -----------------------------------------------------------------------------
#include <windows.h>

namespace menu_window
{
    bool init(HINSTANCE instance);
    void shutdown();

    // per-frame: shows/hides the window from gui::menu_visible() and renders it
    void tick();
    void hide();

    bool visible();
    HWND hwnd();          // the menu HWND (nullptr until init succeeds)

    // true when the menu window (or one of its children) currently owns the
    // foreground - used so the overlay keeps rendering while the menu has focus
    bool owns_foreground();
}
