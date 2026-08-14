#include "pch.h"
#include "keybind.h"
#include "text.h"
#include "imgui.h"
#include <stdio.h>
#include <string.h>
#include <Windows.h>

namespace widgets
{
    const char* key_name(int key)
    {
        if (key <= 0)
            return "-";

        switch (key)
        {
        case VK_LBUTTON: return "lmb";
        case VK_RBUTTON: return "rmb";
        case VK_MBUTTON: return "m3";
        case VK_XBUTTON1: return "m4";
        case VK_XBUTTON2: return "m5";

        case VK_BACK: return "back";
        case VK_TAB: return "tab";
        case VK_RETURN: return "enter";
        case VK_SHIFT: return "shift";
        case VK_CONTROL: return "ctrl";
        case VK_MENU: return "alt";
        case VK_PAUSE: return "pause";
        case VK_CAPITAL: return "caps";
        case VK_ESCAPE: return "esc";
        case VK_SPACE: return "space";
        case VK_PRIOR: return "pgup";
        case VK_NEXT: return "pgdn";
        case VK_END: return "end";
        case VK_HOME: return "home";
        case VK_LEFT: return "left";
        case VK_UP: return "up";
        case VK_RIGHT: return "right";
        case VK_DOWN: return "down";
        case VK_SNAPSHOT: return "prtsc";
        case VK_INSERT: return "ins";
        case VK_DELETE: return "del";

        case 0x30: return "0";
        case 0x31: return "1";
        case 0x32: return "2";
        case 0x33: return "3";
        case 0x34: return "4";
        case 0x35: return "5";
        case 0x36: return "6";
        case 0x37: return "7";
        case 0x38: return "8";
        case 0x39: return "9";

        case 0x41: return "a";
        case 0x42: return "b";
        case 0x43: return "c";
        case 0x44: return "d";
        case 0x45: return "e";
        case 0x46: return "f";
        case 0x47: return "g";
        case 0x48: return "h";
        case 0x49: return "i";
        case 0x4A: return "j";
        case 0x4B: return "k";
        case 0x4C: return "l";
        case 0x4D: return "m";
        case 0x4E: return "n";
        case 0x4F: return "o";
        case 0x50: return "p";
        case 0x51: return "q";
        case 0x52: return "r";
        case 0x53: return "s";
        case 0x54: return "t";
        case 0x55: return "u";
        case 0x56: return "v";
        case 0x57: return "w";
        case 0x58: return "x";
        case 0x59: return "y";
        case 0x5A: return "z";

        case VK_LWIN: return "lwin";
        case VK_RWIN: return "rwin";
        case VK_APPS: return "menu";

        case VK_NUMPAD0: return "np0";
        case VK_NUMPAD1: return "np1";
        case VK_NUMPAD2: return "np2";
        case VK_NUMPAD3: return "np3";
        case VK_NUMPAD4: return "np4";
        case VK_NUMPAD5: return "np5";
        case VK_NUMPAD6: return "np6";
        case VK_NUMPAD7: return "np7";
        case VK_NUMPAD8: return "np8";
        case VK_NUMPAD9: return "np9";
        case VK_MULTIPLY: return "np*";
        case VK_ADD: return "np+";
        case VK_SUBTRACT: return "np-";
        case VK_DECIMAL: return "np.";
        case VK_DIVIDE: return "np/";

        case VK_F1: return "f1";
        case VK_F2: return "f2";
        case VK_F3: return "f3";
        case VK_F4: return "f4";
        case VK_F5: return "f5";
        case VK_F6: return "f6";
        case VK_F7: return "f7";
        case VK_F8: return "f8";
        case VK_F9: return "f9";
        case VK_F10: return "f10";
        case VK_F11: return "f11";
        case VK_F12: return "f12";

        case VK_NUMLOCK: return "numlk";
        case VK_SCROLL: return "scroll";

        case VK_LSHIFT: return "lshift";
        case VK_RSHIFT: return "rshift";
        case VK_LCONTROL: return "lctrl";
        case VK_RCONTROL: return "rctrl";
        case VK_LMENU: return "lalt";
        case VK_RMENU: return "ralt";

        case VK_OEM_1: return ";";
        case VK_OEM_PLUS: return "=";
        case VK_OEM_COMMA: return ",";
        case VK_OEM_MINUS: return "-";
        case VK_OEM_PERIOD: return ".";
        case VK_OEM_2: return "/";
        case VK_OEM_3: return "`";
        case VK_OEM_4: return "[";
        case VK_OEM_5: return "\\";
        case VK_OEM_6: return "]";
        case VK_OEM_7: return "'";

        default: break;
        }

        static char unknown[16];
        snprintf(unknown, sizeof(unknown), "k%d", key);
        return unknown;
    }

    static bool s_ignore_key[256];

    static void arm_ignore()
    {
        for (int k = 0; k < 256; ++k)
            s_ignore_key[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
        s_ignore_key[VK_LBUTTON] = true;
    }

    static void clear_ignore()
    {
        for (int k = 0; k < 256; ++k)
            s_ignore_key[k] = false;
    }

    static int poll_bind_key()
    {
        for (int k = 1; k < 256; ++k)
        {
            if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU)
                continue;

            bool down = (GetAsyncKeyState(k) & 0x8000) != 0;
            if (s_ignore_key[k])
            {
                if (!down)
                    s_ignore_key[k] = false;
                continue;
            }
            if (down)
                return k;
        }
        return 0;
    }

    bool keybind(const char* id, int* key)
    {
        ImGui::PushID(id);

        static ImGuiID waiting = 0;
        ImGuiID self = ImGui::GetID("##kb");

        bool listening = (waiting == self);
        const char* name = listening ? "..." : key_name(*key);

        char buf[32];
        snprintf(buf, sizeof(buf), "[%s]", name);
        ImVec2 text_size = ImGui::CalcTextSize(buf);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##kb", text_size);
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        bool rclicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

        if (clicked)
        {
            if (listening)
            {
                waiting = 0;
                clear_ignore();
            }
            else
            {
                waiting = self;
                arm_ignore();
            }
        }
        if (rclicked && !listening)
            *key = 0;

        bool changed = false;
        if (listening)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                *key = 0;
                waiting = 0;
                clear_ignore();
                changed = true;
            }
            else
            {
                int pressed = poll_bind_key();
                if (pressed && pressed != VK_ESCAPE)
                {
                    *key = pressed;
                    waiting = 0;
                    clear_ignore();
                    changed = true;
                }
            }
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImU32 col = ImGui::GetColorU32(listening ? ImVec4(1.f, 1.f, 1.f, 1.f) : (hovered ? ImVec4(0.85f, 0.85f, 0.85f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f)));
        text_outlined(draw, pos, col, buf);

        ImGui::PopID();
        return changed;
    }

    bool checkbox_keybind(const char* label, bool* value, int* key)
    {
        constexpr float label_gap = 6.f;

        ImGui::PushID(label);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        float avail_w = ImGui::CalcItemWidth();
        if (avail_w < 1.f)
            avail_w = ImGui::GetContentRegionAvail().x;
        ImVec2 text_size = ImGui::CalcTextSize(label);
        float box_size = text_size.y;
        float row_h = box_size;

        ImGui::InvisibleButton("##cb", ImVec2(avail_w, row_h));
        bool hovered = ImGui::IsItemHovered();

        char kb_buf[32];
        static ImGuiID waiting = 0;
        ImGuiID kb_id = ImGui::GetID("##kb");
        bool listening = (waiting == kb_id);
        const char* name = listening ? "..." : key_name(*key);
        snprintf(kb_buf, sizeof(kb_buf), "[%s]", name);
        ImVec2 kb_size = ImGui::CalcTextSize(kb_buf);

        ImVec2 kb_min(pos.x + avail_w - kb_size.x, pos.y);
        ImVec2 kb_max(pos.x + avail_w, pos.y + row_h);
        ImVec2 box_min = pos;
        ImVec2 box_max(box_min.x + box_size, box_min.y + box_size);

        ImVec2 mouse = ImGui::GetIO().MousePos;
        bool over_kb = mouse.x >= kb_min.x && mouse.x <= kb_max.x && mouse.y >= kb_min.y && mouse.y <= kb_max.y;
        bool over_left = hovered && !over_kb;

        bool changed = false;
        if (over_left && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            *value = !*value;
            changed = true;
        }

        if (over_kb && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (listening)
            {
                waiting = 0;
                clear_ignore();
            }
            else
            {
                waiting = kb_id;
                arm_ignore();
            }
        }
        if (over_kb && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !listening)
            *key = 0;

        if (listening)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                *key = 0;
                waiting = 0;
                clear_ignore();
                changed = true;
            }
            else
            {
                int pressed = poll_bind_key();
                if (pressed && pressed != VK_ESCAPE)
                {
                    *key = pressed;
                    waiting = 0;
                    clear_ignore();
                    changed = true;
                }
            }
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (*value)
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)));
        else
        {
            draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.35f)));
            if (over_left)
                draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)));
        }
        draw->AddRect(box_min, box_max, ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 1.f)));

        ImU32 text_col = ImGui::GetColorU32(*value ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f));
        text_outlined(draw, ImVec2(box_max.x + label_gap, pos.y), text_col, label);

        ImU32 kb_col = ImGui::GetColorU32(listening ? ImVec4(1.f, 1.f, 1.f, 1.f) : (over_kb ? ImVec4(0.85f, 0.85f, 0.85f, 1.f) : ImVec4(0.55f, 0.55f, 0.55f, 1.f)));
        text_outlined(draw, kb_min, kb_col, kb_buf);

        ImGui::PopID();
        return changed;
    }
}
