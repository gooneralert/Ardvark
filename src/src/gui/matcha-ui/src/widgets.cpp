#include "pch.h"
#include "widgets.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
    constexpr float columns[2][2] = { { 14, 283 }, { 308, 284 } };
    // Panel area is laid out in its own scroll region (0,88)..(606,932) rather
    // than the whole window, so a page can be taller than the viewport.
    constexpr float content_top = 0;
    constexpr float content_bottom = 856;

    struct
    {
        float x, y, width;
    } layout;

    ImRect row;
    float row_right;
    float content_y;
    ImGuiID binding;
    float frame_max_bottom;

    ImGuiWindow* window()
    {
        return ImGui::GetCurrentWindow();
    }

    float text_width(const char* str)
    {
        return ImGui::CalcTextSize(str, nullptr, true).x;
    }

    void text(ImDrawList* dl, ImVec2 pos, ImU32 color, const char* str)
    {
        dl->AddText(ImFloor(pos), color, str, ImGui::FindRenderedTextEnd(str));
    }

    float centered(const ImRect& bb)
    {
        return bb.Min.y + (bb.GetHeight() - ImGui::GetFontSize()) * 0.5f;
    }

    void set_row(const ImRect& bb)
    {
        row = bb;
        row_right = window()->WorkRect.Max.x;
    }

    ImRect trailing(float width, float height, float overhang = 0)
    {
        ImVec2 min = ImFloor({ row_right + overhang - width, row.GetCenter().y - height * 0.5f });
        row_right = min.x - 6;
        return { min, min + ImVec2(width, height) };
    }

    void frame(ImDrawList* dl, const ImRect& bb, ImU32 fill, ImU32 border, float rounding = 4)
    {
        dl->AddRectFilled(bb.Min, bb.Max, fill, rounding);
        dl->AddRect(bb.Min, bb.Max, border, rounding);
    }

    bool entry(const char* label, bool selected, float height, float pad)
    {
        ImGuiWindow* w = window();
        ImGuiID id = w->GetID(label);
        ImVec2 pos = w->DC.CursorPos;
        ImRect bb(pos, { w->WorkRect.Max.x, pos.y + height });

        ImGui::ItemSize(bb.GetSize());
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (hovered || selected)
            w->DrawList->AddRectFilled(bb.Min, bb.Max, col::white(selected ? 20 : 12), 4);

        text(w->DrawList, { bb.Min.x + pad, centered(bb) }, selected ? col::accent : col::text, label);
        return pressed;
    }

    const char* key_name(int key)
    {
        switch (key)
        {
        case 0: return "none";
        case VK_LBUTTON: return "lmb";
        case VK_RBUTTON: return "rmb";
        case VK_MBUTTON: return "mmb";
        case VK_XBUTTON1: return "xbutton1";
        case VK_XBUTTON2: return "xbutton2";
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return "control";
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return "shift";
        case VK_MENU: case VK_LMENU: case VK_RMENU: return "alt";
        case VK_INSERT: return "ins";
        case VK_DELETE: return "delete";
        case VK_SPACE: return "space";
        default: break;
        }

        static char name[32];
        const UINT sc = MapVirtualKeyA((UINT)key, MAPVK_VK_TO_VSC);
        if (sc && GetKeyNameTextA((LONG)(sc << 16), name, sizeof(name)) > 0)
        {
            for (char* c = name; *c; ++c)
                *c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
            return name;
        }

        ImFormatString(name, sizeof(name), "vk%02X", key);
        return name;
    }

    void header(ImDrawList* dl, float y)
    {
        ImGuiWindow* w = window();
        dl->AddLine({ w->WorkRect.Min.x, w->Pos.y + y }, { w->WorkRect.Max.x, w->Pos.y + y }, col::divider);
        w->DC.CursorPos.y = w->DC.CursorMaxPos.y = content_y = w->Pos.y + y + 10;
    }

    void begin_child(const char* id, float height)
    {
        if (height == ui::fill_height)
            height = content_bottom - layout.y;

        ImGui::SetCursorPos({ layout.x, layout.y });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 14, 0 });
        ImGui::BeginChild(id, { layout.width, height },
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding | (height > 0 ? 0 : ImGuiChildFlags_AutoResizeY),
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
    }
}

void ui::column(int index)
{
    layout = { columns[index][0], content_top, columns[index][1] };
}

void ui::begin_frame()
{
    frame_max_bottom = 0.f;
}

float ui::frame_content_bottom()
{
    return frame_max_bottom;
}

void ui::begin_panel(const char* title, float height)
{
    begin_child(title, height);
    ImGuiWindow* w = window();
    text(w->DrawList, { w->WorkRect.Min.x, w->Pos.y + 11 }, col::text, title);
    header(w->DrawList, 30);
}

void ui::begin_panel(const char* id, items tabs, int& tab, float height)
{
    begin_child(id, height);
    ImGuiWindow* w = window();
    ImDrawList* dl = w->DrawList;

    float x = w->WorkRect.Min.x;
    for (int i = 0; i < static_cast<int>(tabs.size()); ++i)
    {
        ImRect bb({ x, w->Pos.y + 12 }, { x + text_width(tabs[i]) + 16, w->Pos.y + 32 });
        ImGuiID tab_id = w->GetID(tabs[i]);
        ImGui::ItemAdd(bb, tab_id);

        bool hovered, held;
        if (ImGui::ButtonBehavior(bb, tab_id, &hovered, &held))
            tab = i;

        if (tab == i)
            dl->AddRectFilled(bb.Min, bb.Max, col::tint, 4);
        text(dl, { bb.Min.x + 8, centered(bb) }, tab == i ? col::accent : hovered ? col::text : col::dim, tabs[i]);
        x = bb.Max.x + 6;
    }
    header(dl, 38);
}

void ui::end_panel(float gap)
{
    spacing(8);
    ImGui::EndChild();
    // GetItemRectMax() is in screen space and includes the container's scroll;
    // subtract it so layout.y stays in panel-area local coordinates.
    layout.y = ImGui::GetItemRectMax().y - (ImGui::GetWindowPos().y - ImGui::GetScrollY()) + gap;
    frame_max_bottom = std::max(frame_max_bottom, layout.y);
}

void ui::begin_box(const char* id, float height, float padding)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, col::control);
    ImGui::PushStyleColor(ImGuiCol_Border, col::frame);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { padding, padding + 1 });
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8);
    ImGui::BeginChild(id, { -FLT_MIN, height }, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
}

void ui::end_box()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

bool ui::checkbox(const char* label, bool& value, ImU32 color)
{
    ImGuiWindow* w = window();
    ImGuiID id = w->GetID(label);
    ImVec2 pos = w->DC.CursorPos;
    ImRect bb(pos, pos + ImVec2(26 + text_width(label), 18));

    ImGui::ItemSize(bb.GetSize());
    set_row(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed)
        value = !value;

    ImU32 box = value ? col::checked : hovered ? col::white(57) : col::check;
    w->DrawList->AddRectFilled(pos, pos + ImVec2(18, 18), box, 4);
    text(w->DrawList, { pos.x + 26, centered(bb) }, color, label);
    return pressed;
}

bool ui::slider(const char* label, float& value, float min, float max, const char* format)
{
    ImGuiWindow* w = window();
    ImGuiID id = w->GetID(label);
    ImVec2 pos = w->DC.CursorPos;
    ImRect bb(pos, { w->WorkRect.Max.x, pos.y + 30 });
    ImRect track({ pos.x, pos.y + 17 }, { bb.Max.x, pos.y + 25 });

    ImGui::ItemSize(bb.GetSize());
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    ImGui::ButtonBehavior(ImRect(track.Min - ImVec2(0, 5), track.Max + ImVec2(0, 5)), id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);

    bool changed = false;
    if (held)
    {
        float t = ImSaturate((ImGui::GetIO().MousePos.x - track.Min.x) / track.GetWidth());
        float v = min + (max - min) * t;
        changed = v != value;
        value = v;
    }

    char buf[32];
    ImFormatString(buf, sizeof(buf), format, value);

    ImDrawList* dl = w->DrawList;
    text(dl, { pos.x, pos.y - 3 }, col::text, label);
    text(dl, { track.Max.x - text_width(buf), pos.y - 3 }, col::text, buf);
    dl->AddRectFilled(track.Min, track.Max, hovered || held ? col::hovered : col::control, 3);

    float t = ImSaturate((value - min) / (max - min));
    if (t > 0)
        dl->AddRectFilled(track.Min, { track.Min.x + std::max(4.0f, track.GetWidth() * t), track.Max.y }, col::fill, 3);
    return changed;
}

bool ui::slider(const char* label, int& value, int min, int max)
{
    float v = static_cast<float>(value);
    bool changed = slider(label, v, static_cast<float>(min), static_cast<float>(max));
    value = static_cast<int>(std::lround(v));
    return changed;
}

bool ui::combo(const char* label, int& current, items options)
{
    ImGuiWindow* w = window();
    ImGuiID id = w->GetID(label);
    ImVec2 pos = w->DC.CursorPos;
    ImRect box({ pos.x, pos.y + 20 }, { w->WorkRect.Max.x, pos.y + 48 });

    ImGui::ItemSize(ImVec2(box.GetWidth(), 50));
    if (!ImGui::ItemAdd(box, id))
        return false;

    bool hovered, held;
    if (ImGui::ButtonBehavior(box, id, &hovered, &held))
        ImGui::OpenPopupEx(id);

    bool open = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);
    if (current < 0 || current >= static_cast<int>(options.size()))
        current = 0;   // settings carried over from elsewhere may be out of range
    ImDrawList* dl = w->DrawList;
    text(dl, { pos.x, pos.y - 3 }, col::text, label);
    frame(dl, box, hovered || open ? col::hovered : col::control, col::frame);
    text(dl, { box.Min.x + 12, centered(box) }, col::text, options[current]);
    text(dl, { box.Max.x - 26, centered(box) }, col::dim, icon::chevron);

    if (!open)
        return false;

    bool changed = false;
    ImGui::SetNextWindowPos({ box.Min.x, box.Max.y + 4 });
    ImGui::SetNextWindowSizeConstraints({ box.GetWidth(), 0 }, { box.GetWidth(), FLT_MAX });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 4, 4 });
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 2 });
    if (ImGui::BeginPopupEx(id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
    {
        for (int i = 0; i < static_cast<int>(options.size()); ++i)
        {
            if (entry(options[i], i == current, 24, 8))
            {
                current = i;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    return changed;
}

bool ui::button(const char* label, float width, float height)
{
    ImGuiWindow* w = window();
    ImGuiID id = w->GetID(label);
    ImVec2 pos = w->DC.CursorPos;
    if (width <= 0)
        width = w->WorkRect.Max.x - pos.x;
    ImRect bb(pos, pos + ImVec2(width, height));

    ImGui::ItemSize(bb.GetSize());
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = w->DrawList;
    frame(dl, bb, held ? col::control : hovered ? col::hovered : col::button, col::white(10));
    text(dl, { bb.GetCenter().x - text_width(label) * 0.5f, centered(bb) }, hovered ? col::bright : col::text, label);
    return pressed;
}

int ui::buttons(std::initializer_list<const char*> labels)
{
    float count = static_cast<float>(labels.size());
    float width = std::floor((ImGui::GetContentRegionAvail().x - 6 * (count - 1)) / count);

    int pressed = -1, i = 0;
    for (const char* label : labels)
    {
        if (i)
            ImGui::SameLine(0, 6);
        if (button(label, width))
            pressed = i;
        ++i;
    }
    return pressed;
}

bool ui::input(const char* id, char* buf, size_t size, const char* hint)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 10, (28 - ImGui::GetFontSize()) * 0.5f });
    ImGui::PushStyleColor(ImGuiCol_Border, col::frame);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = ImGui::InputTextWithHint(id, hint, buf, size);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    return changed;
}

bool ui::list(const char* id, items entries, int& selected, float height)
{
    bool changed = false;
    begin_box(id, height, 8);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
    for (int i = 0; i < static_cast<int>(entries.size()); ++i)
    {
        if (entry(entries[i], i == selected, 28, 13))
        {
            selected = i;
            changed = true;
        }
    }
    ImGui::PopStyleVar();
    end_box();
    return changed;
}

void ui::row(const char* str, ImU32 color)
{
    ImGuiWindow* w = window();
    ImVec2 pos = w->DC.CursorPos;
    ImRect bb(pos, pos + ImVec2(text_width(str), 18));

    ImGui::ItemSize(bb.GetSize());
    set_row(bb);
    text(w->DrawList, { pos.x, centered(bb) }, color, str);
}

void ui::label(const char* str, ImU32 color)
{
    ImGuiWindow* w = window();
    if (w->DC.CursorPos.y > content_y + 1)
        w->DC.CursorPos.y += 6;

    ImVec2 pos = w->DC.CursorPos;
    ImRect bb(pos, pos + ImVec2(text_width(str), 8));

    ImGui::ItemSize(bb.GetSize());
    set_row(bb);
    text(w->DrawList, { pos.x, pos.y - 3 }, color, str);
}

void ui::center(const char* str, ImU32 color)
{
    ImGuiWindow* w = window();
    ImVec2 pos = w->DC.CursorPos;
    float x = (w->WorkRect.Min.x + w->WorkRect.Max.x - text_width(str)) * 0.5f;

    ImGui::ItemSize(ImVec2(0, 8));
    text(w->DrawList, { x, pos.y - 3 }, color, str);
}

void ui::badge(const char* str)
{
    ImGuiWindow* w = window();
    ImVec2 pos = w->DC.CursorPos;
    float width = text_width(str) + 16;

    ImGui::ItemSize(ImVec2(width, 16));
    draw_badge(w->DrawList, { (w->WorkRect.Min.x + w->WorkRect.Max.x - width) * 0.5f, pos.y }, str);
}

void ui::spacing(float height)
{
    ImGui::ItemSize(ImVec2(0, height));
}

void ui::separator()
{
    ImGuiWindow* w = window();
    float y = w->DC.CursorPos.y;
    w->DrawList->AddLine({ w->WorkRect.Min.x, y }, { w->WorkRect.Max.x, y }, col::frame);
    ImGui::ItemSize(ImVec2(0, 1));
}

// Ardvark stores binds as Win32 virtual-key codes, so poll VK directly instead
// of ImGuiKey (matcha's own ImGui build exposed a different key enum).
void ui::keybind(int& key)
{
    ImGuiWindow* w = window();
    ImGuiID id = w->GetID(&key);
    bool waiting = binding == id;
    const char* name = waiting ? "..." : key_name(key);
    ImRect bb = trailing(std::max(40.0f, text_width(name) + 20), 20, 4);

    if (!ImGui::ItemAdd(bb, id))
        return;

    bool hovered, held;
    if (ImGui::ButtonBehavior(bb, id, &hovered, &held))
        binding = waiting ? 0 : id;

    if (waiting)
    {
        for (int vk = 1; vk < 256; ++vk)
        {
            // ignore the mouse button that opened the bind
            if (vk == VK_LBUTTON || vk == VK_RBUTTON)
                continue;

            if ((GetAsyncKeyState(vk) & 0x8000) == 0)
                continue;

            key = vk == VK_ESCAPE ? 0 : vk;
            binding = 0;
            break;
        }
    }

    frame(w->DrawList, bb, hovered ? col::hovered : col::control, col::white(27), 10);
    text(w->DrawList, { bb.GetCenter().x - text_width(name) * 0.5f, centered(bb) }, waiting ? col::accent : col::text, name);
}

void ui::color(ImVec4& value)
{
    ImGuiWindow* w = window();
    ImGuiID id = w->GetID(&value);
    ImRect bb = trailing(18, 18);

    if (!ImGui::ItemAdd(bb, id))
        return;

    bool hovered, held;
    if (ImGui::ButtonBehavior(bb, id, &hovered, &held))
        ImGui::OpenPopupEx(id);

    ImGui::RenderColorRectWithAlphaCheckerboard(w->DrawList, bb.Min, bb.Max, ImGui::GetColorU32(value), 6, { 0, 0 }, 4);

    if (!ImGui::IsPopupOpen(id, ImGuiPopupFlags_None))
        return;

    ImGui::SetNextWindowPos({ bb.Max.x, bb.Max.y + 6 }, ImGuiCond_Always, { 1, 0 });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8, 8 });
    if (ImGui::BeginPopupEx(id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::SetNextItemWidth(170);
        ImGui::ColorPicker4("##picker", &value.x, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void ui::help(const char* tip)
{
    ImGuiWindow* w = window();
    ImGuiID id = w->GetID(tip);
    ImRect bb = trailing(text_width("?"), ImGui::GetFontSize(), -2);

    ImGui::ItemAdd(bb, id);
    bool hovered = ImGui::ItemHoverable(bb, id, ImGuiItemFlags_None);
    text(w->DrawList, bb.Min, hovered ? col::text : col::faint, "?");

    if (hovered)
        ImGui::SetTooltip("%s", tip);
}

bool ui::chip(const char* label)
{
    ImGuiWindow* w = window();
    ImGuiID id = w->GetID(label);
    ImRect bb = trailing(text_width(label) + 12, 16, 4);

    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    frame(w->DrawList, bb, hovered ? col::hovered : col::control, col::frame);
    text(w->DrawList, { bb.Min.x + 6, centered(bb) }, hovered ? col::text : col::dim, label);
    return pressed;
}

void ui::value(const char* str, ImU32 color)
{
    ImRect bb = trailing(text_width(str), ImGui::GetFontSize());
    text(window()->DrawList, bb.Min, color, str);
}

void ui::draw_badge(ImDrawList* dl, ImVec2 pos, const char* str)
{
    ImRect bb(pos, pos + ImVec2(text_width(str) + 16, 16));
    frame(dl, bb, col::tint, IM_COL32(181, 96, 96, 32));
    text(dl, { bb.Min.x + 8, centered(bb) }, col::accent, str);
}
