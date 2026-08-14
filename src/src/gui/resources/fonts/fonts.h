#pragma once

#include "imgui.h"
#include "app/Settings.h"
#include <cmath>

namespace fonts {
    extern ImFont* fredoka_one;
    extern ImFont* imgui;
    extern ImFont* tahoma_bold;
    extern ImFont* proggy_clean;
    extern ImFont* visitor;
    extern ImFont* verdana;
    extern ImFont* menu;

    extern ImFont* tahoma;
    extern ImFont* esp;
    extern ImFont* esp_bold;

    void load(ImGuiIO& io);

    inline ImFont* by_index(int index) {
		// 0 fredoka, 1 tahoma bold, 2 proggy clean, 3 visitor, 4 verdana, 5 segoe ui, 6 imgui default
        switch (index) {
        case 0: if (fredoka_one)  return fredoka_one;  break;
        case 1: if (tahoma_bold)  return tahoma_bold;  break;
        case 2: if (proggy_clean) return proggy_clean; break;
        case 3: if (visitor)      return visitor;      break;
        case 4: if (verdana)      return verdana;      break;
        case 5: if (imgui)        return imgui;        break;
        case 6: if (menu)         return menu;         break;
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
        if (imgui)
            return imgui;
        if (verdana)
            return verdana;
        return ImGui::GetFont();
    }

    inline ImFont* ui_bold() {
        if (tahoma_bold)
            return tahoma_bold;
        return ui();
    }

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
