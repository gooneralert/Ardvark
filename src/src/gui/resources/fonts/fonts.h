#pragma once

#include "imgui.h"
#include "app/Settings.h"
#include <cmath>

namespace fonts {
    extern ImFont* proxima_soft_bold;
    extern ImFont* proxima_soft_extrabold;  // wordmark only
    extern ImFont* fredoka_one;
    extern ImFont* imgui;
    extern ImFont* tahoma_bold;
    extern ImFont* proggy_clean;
    extern ImFont* visitor;
    extern ImFont* verdana;
    extern ImFont* menu;

    extern ImFont* tahoma;
    extern ImFont* music_regular;   // Inter (music player), loaded at startup
    extern ImFont* music_bold;
    extern ImFont* esp;
    extern ImFont* esp_bold;

    // Inter - the body/heading face of the Matcha UI remake (the reference the
    // menu replicates). Loaded from the .ttf files in resources/fonts, which are
    // byte-identical to the arrays the remake embeds. Either pointer stays null
    // when the .ttf is missing, so every use site must fall back (menu_regular /
    // menu_bold do that for the menu).
    extern ImFont* inter_regular;
    extern ImFont* inter_semi_bold;

    void load(ImGuiIO& io);

    inline ImFont* by_index(int index) {
		// 0 prox soft bold, 1 fredoka, 2 tahoma bold, 3 proggy clean, 4 visitor,
		// 5 verdana, 6 segoe ui, 7 imgui default, 8 prox soft extrabold,
		// 9 inter regular, 10 inter semibold
		// (new weights are appended so saved `gui.font` values keep working)
        switch (index) {
        case 0: if (proxima_soft_bold) return proxima_soft_bold; break;
        case 1: if (fredoka_one)  return fredoka_one;  break;
        case 2: if (tahoma_bold)  return tahoma_bold;  break;
        case 3: if (proggy_clean) return proggy_clean; break;
        case 4: if (visitor)      return visitor;      break;
        case 5: if (verdana)      return verdana;      break;
        case 6: if (imgui)        return imgui;        break;
        case 7: if (menu)         return menu;         break;
        case 8: if (proxima_soft_extrabold) return proxima_soft_extrabold; break;
        case 9:  if (inter_regular)   return inter_regular;   break;
        case 10: if (inter_semi_bold) return inter_semi_bold; break;
        default: break;
        }

        if (menu)
            return menu;
        if (verdana)
            return verdana;
        if (fredoka_one)
            return fredoka_one;
        if (imgui)
            return imgui;
        if (tahoma_bold)
            return tahoma_bold;
        return ImGui::GetFont();
    }

    inline ImFont* selected() {
        return by_index(Cheat::g_Settings.esp.font);
    }

    inline ImFont* ui() {
        if (inter_regular)
            return inter_regular;
        if (imgui)
            return imgui;
        if (verdana)
            return verdana;
        return ImGui::GetFont();
    }

    inline ImFont* ui_bold() {
        if (inter_semi_bold)
            return inter_semi_bold;
        if (tahoma_bold)
            return tahoma_bold;
        return ui();
    }

    // The menu's own face: Inter Regular / SemiBold, the exact pair the Matcha
    // UI remake draws with (see fonts.cpp - the .ttf files are byte-identical to
    // the arrays that remake embeds). Both fall back to the legacy UI fonts when
    // the files were not found, so the menu always has a usable font.
    inline ImFont* menu_regular() { return inter_regular   ? inter_regular   : ui(); }
    inline ImFont* menu_bold()    { return inter_semi_bold ? inter_semi_bold : ui_bold(); }
    // Row-label size from the reference (MATCHA_UI_SPEC §3): the menu pushes
    // Inter at 13 px so every metric derived from GetFontSize() - section
    // headers at 0.82x, control text, key chips - lands on the measured value.
    inline float menu_size() { return 13.0f; }

    inline float ui_size(ImFont* font = nullptr) {
        ImFont* f = font ? font : ui();
        if (f && f->LegacySize > 0.0f)
            return f->LegacySize;
        return 14.0f;
    }

    inline float snap_px(float size) {
        if (size < 8.0f)
            size = 8.0f;
        return size;
    }
}
