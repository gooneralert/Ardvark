//============ Copyright KiwiHax, All rights reserved ============//
//
//  Purpose: 
//
//================================================================//

#include "../imgui_internal.h"
#include "imgui_addons.h"
#include "../../src/menu/keybind/keybind.h"
#include "../../src/menu/menu_theme.h"
#include "../../src/render/render.h"

#include <cctype>
#include <map>
#include <unordered_map>
#include <string>
#include <cstring>
#include <utility>
#include <windows.h>

using namespace ImGui;

// ---------------------------------------------------------------------------
//  Shared "active glow" fade — used by toggles, buttons, and sliders so every
//  interactive element eases its halo in/out over the same fixed duration
//  instead of drifting with a frame-rate-dependent exponential decay.
// ---------------------------------------------------------------------------
namespace
{
    // Advances a per-id linear progress value toward 0/1 over exactly
    // `duration_sec`, then reshapes it with a smoothstep ease-in-out curve.
    float GlowAlpha(ImGuiID id, bool active, float duration_sec = 0.2f)
    {
        static std::map<ImGuiID, float> s_progress;
        float& p = s_progress[id];

        ImGuiContext& g = *GImGui;
        const float step = (duration_sec > 0.f) ? (g.IO.DeltaTime / duration_sec) : 1.f;
        p = ImClamp(p + (active ? step : -step), 0.f, 1.f);

        // Ease-in-out (smoothstep): slow-fast-slow instead of a linear ramp
        return p * p * (3.f - 2.f * p);
    }

    // Per-section tab-content fade state, written by ImAdd::BeginChild (keyed
    // by the child window's own id) and consumed by ImAdd::EndChild to draw
    // the reveal overlay — {eased fade progress 0..1, content y-offset}.
    std::map<ImGuiID, std::pair<float, float>> g_tab_fade_state;

    // ImAdd::BeginReveal/EndReveal state — one entry per reveal block, keyed
    // by the id computed at BeginReveal() time. `g_reveal_stack` mirrors
    // ImGui's own Begin/End nesting so EndReveal() can find its matching
    // entry without the caller having to repeat the string id.
    struct RevealState { float progress = 0.f; float height = 0.f; };
    std::map<ImGuiID, RevealState> g_reveal_state;
    std::vector<ImGuiID> g_reveal_stack;

    // Smooth gaussian-style glow for rounded rects.
    // Draws `layers` expanding rings where each ring's alpha follows a bell
    // curve (peaks near the shape edge, fades smoothly to zero at `spread` px
    // out), giving a soft halo rather than a hard-edged decay loop.
    // `peak_alpha` is the brightest the glow gets (at the shape edge).
    void DrawGlowRect(ImDrawList* dl, const ImRect& bb, float rounding, float alpha01,
                      ImVec4 col = ImVec4(1.f, 1.f, 1.f, 1.f),
                      float spread = 14.f, int layers = 18)
    {
        if (alpha01 <= 0.003f) return;

        // Full-width items sit flush with the child clip, so the halo is
        // visible above/below (row gap) but sliced off on the left/right.
        // Widen only X; keep the vertical clip so scroll areas stay clean.
        const ImVec2 clip_min = dl->GetClipRectMin();
        const ImVec2 clip_max = dl->GetClipRectMax();
        dl->PushClipRect(
            ImVec2(clip_min.x - spread, clip_min.y),
            ImVec2(clip_max.x + spread, clip_max.y),
            false);

        const float peak = col.w * alpha01;
        const float r    = col.x, g = col.y, b = col.z;

        for (int i = 1; i <= layers; ++i)
        {
            // Normalized distance from shape edge: 0 = edge, 1 = outer limit
            const float t   = (float)i / (float)layers;
            // Gaussian falloff: e^(-4 * t^2) peaks at t=0 and is ~0 at t=1
            const float a   = peak * expf(-4.f * t * t);
            if (a < 0.002f) continue;

            const float exp = spread * t;
            dl->AddRectFilled(
                bb.Min - ImVec2(exp, exp),
                bb.Max + ImVec2(exp, exp),
                IM_COL32((int)(r*255.f),(int)(g*255.f),(int)(b*255.f),(int)(a*255.f)),
                rounding + exp);
        }

        dl->PopClipRect();
    }

    // Smooth gaussian-style glow for circles.
    void DrawGlowCircle(ImDrawList* dl, const ImVec2& center, float radius, float alpha01,
                        ImVec4 col = ImVec4(1.f, 1.f, 1.f, 1.f),
                        float spread = 12.f, int layers = 16)
    {
        if (alpha01 <= 0.003f) return;

        const float peak = col.w * alpha01;
        const float r    = col.x, g = col.y, b = col.z;

        for (int i = 1; i <= layers; ++i)
        {
            const float t   = (float)i / (float)layers;
            const float a   = peak * expf(-4.f * t * t);
            if (a < 0.002f) continue;

            dl->AddCircleFilled(center, radius + spread * t,
                IM_COL32((int)(r*255.f),(int)(g*255.f),(int)(b*255.f),(int)(a*255.f)),
                32);
        }
    }
}

// Public wrappers around the internal glow helpers so custom-drawn elements
// (config cards, etc.) can reuse the exact same animated highlight as the
// built-in widgets.
float ImAdd::GlowAlphaEased(ImGuiID id, bool active, float duration_sec)
{
    return GlowAlpha(id, active, duration_sec);
}

void ImAdd::DrawGlowRectBg(ImDrawList* dl, const ImVec2& bb_min, const ImVec2& bb_max, float rounding, float alpha01,
    ImVec4 col, float spread, int layers)
{
    DrawGlowRect(dl, ImRect(bb_min, bb_max), rounding, alpha01, col, spread, layers);
}

ImVec4 ImAdd::HexToColorVec4(unsigned int hex_color, float alpha)
{
    ImVec4 color;

    color.x = ((hex_color >> 16) & 0xFF) / 255.0f;
    color.y = ((hex_color >> 8) & 0xFF) / 255.0f;
    color.z = (hex_color & 0xFF) / 255.0f;
    color.w = alpha;

    return color;
}

float ImAdd::GetColorPickerWidth()
{
    return ImAdd::kColorSwatchWidth;
}

float ImAdd::GetKeyBindWidth()
{
    return 36.f;
}

void ImAdd::SeparatorText(const char* label, float thickness)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;
    
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(-0.1f, g.FontSize), label_size.x, g.FontSize);

    const ImRect total_bb(pos, pos + size);
    ItemSize(total_bb);
    if (!ItemAdd(total_bb, id)) {
        return;
    }

    window->DrawList->AddText(pos, GetColorU32(ImGuiCol_TextDisabled), label);

    if (thickness > 0)
        window->DrawList->AddLine(pos + ImVec2(label_size.x + style.ItemInnerSpacing.x, size.y / 2), pos + ImVec2(size.x, size.y / 2), GetColorU32(ImGuiCol_Border), thickness);
}

void ImAdd::VSeparator(float margin, float thickness)
{
    if (thickness <= 0)
        return;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(thickness, -0.1f), thickness, thickness);

    const ImRect bb(pos, pos + size);
    const ImRect bb_rect(pos + ImVec2(0, margin), pos + size - ImVec2(0, margin));

    ItemSize(ImVec2(thickness, 0.0f));
    if (!ItemAdd(bb, 0))
        return;

    window->DrawList->AddRectFilled(bb_rect.Min, bb_rect.Max, GetColorU32(ImGuiCol_Border));
}

bool ImAdd::SelectableLabel(const char* label, bool selected, bool centered, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);

    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    // Colors — fixed 100ms ease-in-out fade between dim/bright text on
    // hover/selection instead of a frame-rate-dependent exponential
    const float sel_t = GlowAlpha(id, hovered || selected, 0.1f);
    const ImVec4 colLabel = ImLerp(GetStyleColorVec4(ImGuiCol_TextDisabled), GetStyleColorVec4(ImGuiCol_Text), sel_t);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddText(pos + ImTrunc(ImVec2(centered ? (size.x / 2 - label_size.x / 2) : 0.0f, size.y / 2 - label_size.y / 2)), GetColorU32(colLabel), label);

    return pressed;
}

void render_checkmark_sunshine(ImDrawList* dl, ImVec2 pos, ImU32 col, float sz)
{
    float th = 1.f;
    pos += ImVec2(th * 0.25f, th * 0.25f);
    float third = sz / 3.f;
    float bx = pos.x + third;
    float by = pos.y + sz - third * 0.5f;
    dl->PathLineTo(ImVec2(bx - third, by - third));
    dl->PathLineTo(ImVec2(bx, by));
    dl->PathLineTo(ImVec2(bx + third * 2.f, by - third * 2.f));
    dl->PathStroke(col, 0, th);
}

bool ImAdd::CheckBox(const char* label, bool* v, float trailing_extra)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const char* label_end = FindRenderedTextEnd(label);
    ImFont* label_font = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : GetFont();
    const ImVec2 label_size = label_font->CalcTextSizeA(MenuTheme::FontPx(MenuTheme::Typography::kLabelSize), FLT_MAX, 0.f, label, label_end);
    const float sz = GetFontSize() + style.CellPadding.y * 2.f;

    const float box_sz = 20.f;
    const float rounding = 4.f;
    const float label_gap = 5.f;

    const ImVec2 pos = window->DC.CursorPos;
    const float label_w = (label_size.x > 0) ? label_gap + label_size.x : 0.f;
    const ImRect total_bb(pos, pos + ImVec2(box_sz + label_w + trailing_extra, sz));
    const float box_y = IM_ROUND(pos.y + (sz - box_sz) * 0.5f);
    const ImRect box_bb(ImVec2(IM_ROUND(pos.x), box_y), ImVec2(IM_ROUND(pos.x) + box_sz, box_y + box_sz));

    ItemSize(total_bb, style.CellPadding.y);
    if (!ItemAdd(total_bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
    bool checked = v ? *v : false;
    if (pressed && v) *v = !*v, checked = *v;

    // Sunshine anim: use GlowAlpha for smooth 0.2s fade
    const float t = GlowAlpha(id, checked, 0.2f);
    const float hov_t = GlowAlpha(id ^ 0xB55A4F09u, hovered, 0.1f);

    MenuTheme::AddRectFilledCrisp(window->DrawList, box_bb.Min, box_bb.Max, IM_COL32(41, 46, 66, 255), rounding);
    if (t > 0.01f)
        MenuTheme::AddRectFilledCrisp(window->DrawList, box_bb.Min, box_bb.Max, IM_COL32(0, 0, 0, (int)(t * 255)), rounding);

    // Label
    if (label_size.x > 0) {
        ImFont* f = label_font;
        ImVec2 ls = label_size;
        ImVec2 lp(box_bb.Max.x + 5.f, pos.y + (sz - ls.y)*0.5f);
        window->DrawList->AddText(f, MenuTheme::FontPx(MenuTheme::Typography::kLabelSize), ImVec2(ImFloor(lp.x), ImFloor(lp.y)), IM_COL32(122, 131, 165, 255), label, label_end);
    }
    RenderNavHighlight(total_bb, id);
    return pressed;
}

static bool DrawColorSwatch(const char* label, float col[4], const ImRect& bb)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImVec4 col_v4(col[0], col[1], col[2], col[3]);
    const ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip;

    ImGuiID id = window->GetID(label);
    ItemAdd(bb, id);

    bool hovered = false;
    bool held = false;
    const bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    const float rounding = 4.f;
    window->DrawList->AddRectFilled(bb.Min, bb.Max, GetColorU32(col_v4), rounding);

    if (pressed)
        OpenPopup(label);

    if (BeginPopup(label))
    {
        ColorPicker4(label, col, flags);
        EndPopup();
    }

    return pressed;
}

bool ImAdd::CheckBoxColor(const char* label, bool* v, float col[4], bool layout_color)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const float avail = GetContentRegionAvail().x;
    const ImVec2 row = window->DC.CursorPos;
    const float swatch_reserve = kColorSwatchGap + kColorSwatchWidth;
    bool changed = CheckBox(label, v, swatch_reserve);

    if (label && FindRenderedTextEnd(label)[0] != '\0')
    {
        const ImVec2 swatch_min(
            row.x + avail - kColorSwatchWidth,
            row.y + (GetItemRectSize().y - kColorSwatchSize) * 0.5f);
        const ImRect swatch_bb(swatch_min, swatch_min + ImVec2(kColorSwatchWidth, kColorSwatchSize));

        char color_id[80];
        ImFormatString(color_id, IM_ARRAYSIZE(color_id), "##%s_col", label);

        if (layout_color)
        {
            SetCursorScreenPos(swatch_min);
            ColorEdit4(color_id, col);
            SetCursorScreenPos(ImVec2(row.x, GetItemRectMax().y + GetStyle().ItemSpacing.y));
        }
        else
        {
            DrawColorSwatch(color_id, col, swatch_bb);
        }
    }

    return changed;
}

void ImAdd::CheckBoxColorRow3(
    const char* l0, bool* v0, float* c0,
    const char* l1, bool* v1, float* c1,
    const char* l2, bool* v2, float* c2)
{
    const float x0 = GetCursorPosX();
    const float y0 = GetCursorPosY();
    const float col_w = GetContentRegionAvail().x / 3.f;
    const float row_h = GetFontSize() + GImGui->Style.CellPadding.y * 2.f;

    PushID(0);
    SetCursorPos(ImVec2(x0, y0));
    CheckBoxColor(l0, v0, c0, false);
    PopID();

    PushID(1);
    SetCursorPos(ImVec2(x0 + col_w, y0));
    CheckBoxColor(l1, v1, c1, false);
    PopID();

    PushID(2);
    SetCursorPos(ImVec2(x0 + col_w * 2.f, y0));
    CheckBoxColor(l2, v2, c2, false);
    PopID();

    SetCursorPos(ImVec2(x0, y0 + row_h + GImGui->Style.ItemSpacing.y));
}

// ---------------------------------------------------------------------------
//  CheckBoxRow2 — two independent toggles side-by-side, each exactly 50% of
//  the row width. Used to grid up simple "Enable"-style checkboxes instead of
//  stacking them one per line.
// ---------------------------------------------------------------------------
bool ImAdd::CheckBoxRow2(const char* label1, bool* v1, const char* label2, bool* v2)
{
    const float x0 = GetCursorPosX();
    const float y0 = GetCursorPosY();
    const float col_w = GetContentRegionAvail().x * 0.5f;
    bool changed = ImAdd::CheckBox(label1, v1);
    SetCursorPos(ImVec2(x0 + col_w, y0));
    changed |= ImAdd::CheckBox(label2, v2);
    return changed;
}

bool ImAdd::CheckBoxKeyBind(const char* label, bool* v, ImGuiKey* k, int* activation_mode)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    const float avail = GetContentRegionAvail().x;
    const ImVec2 row = window->DC.CursorPos;

    ImFont* kfont = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : GetFont();
    const float ksz = MenuTheme::FontPx(11.f);
    char preview[32] = "not bound";
    if (k && *k != ImGuiKey_None && *k != 0)
    {
        int vk = keybind::imgui_key_to_vk(*k);
        const char* name = keybind::get_key_name(vk);
        snprintf(preview, sizeof(preview), "%s", name);
        for (char* p = preview; *p; ++p)
            *p = (char)tolower((unsigned char)*p);
    }
    const float pill_w = ImMax(kfont->CalcTextSizeA(ksz, FLT_MAX, 0.f, preview).x + 16.f, 28.f);

    bool changed = CheckBox(label, v, pill_w + 8.f);
    char kb_id[96];
    ImFormatString(kb_id, IM_ARRAYSIZE(kb_id), "##kb_%s", label);
    SetCursorScreenPos(ImVec2(row.x + avail - pill_w, row.y + 1.f));
    changed |= KeyBind(kb_id, k, ImVec2(pill_w, 0), activation_mode);
    SetCursorScreenPos(ImVec2(row.x, GetItemRectMax().y + GetStyle().ItemSpacing.y));
    return changed;
}

// ---------------------------------------------------------------------------
//  EnableRow  —  "Label"  [checkbox] [⚙]  aligned to full row width
//  The label sits at the left, checkbox + gear icon sit at the far right.
//  Clicking the gear opens the keybind popup (right-click menu of KeyBind).
// ---------------------------------------------------------------------------
bool ImAdd::EnableRow(const char* label, bool* v, ImGuiKey* k, int* activation_mode)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g    = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float sz     = GetFontSize() + style.CellPadding.y * 2.0f;  // widget height
    const float avail  = GetContentRegionAvail().x;
    const float gear_w = sz;                   // gear button is a square
    const float cb_w   = (sz * 0.62f * 1.6f) * 1.8f;   // iOS-style pill width — must match ImAdd::CheckBox's track_w
    const float gap    = style.ItemInnerSpacing.x;

    ImVec2 pos = window->DC.CursorPos;

    // Reserve the full row
    const ImRect row_bb(pos, pos + ImVec2(avail, sz));
    ItemSize(row_bb, style.CellPadding.y);
    if (!ItemAdd(row_bb, 0)) return false;

    ImDrawList* dl   = window->DrawList;
    ImFont*     font = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : GetFont();

    // ── Label (left-aligned, vertically centred) ──────────────────────────
    const ImVec2 label_sz_small = font->CalcTextSizeA(MenuTheme::FontPx(MenuTheme::Typography::kLabelSize), FLT_MAX, 0.f, label);
    const float label_y = pos.y + (sz - label_sz_small.y) * 0.5f;
    dl->AddText(font, MenuTheme::FontPx(MenuTheme::Typography::kLabelSize), ImVec2(ImFloor(pos.x), ImFloor(label_y)),
        MenuTheme::Typography::kLabelColor, label);

    // ── Gear icon (far right) ──────────────────────────────────────────────
    const float gear_x = pos.x + avail - gear_w;
    const ImRect gear_bb(ImVec2(gear_x, pos.y), ImVec2(gear_x + gear_w, pos.y + sz));
    const ImGuiID gear_id = window->GetID((std::string(label) + "##gear").c_str());

    bool g_hov = false, g_held = false;
    bool gear_pressed = false;
    SetCursorScreenPos(gear_bb.Min);
    ItemSize(gear_bb);
    if (ItemAdd(gear_bb, gear_id))
        gear_pressed = ButtonBehavior(gear_bb, gear_id, &g_hov, &g_held);

    // Gear background tint on hover
    if (g_hov || g_held)
        dl->AddRectFilled(gear_bb.Min, gear_bb.Max,
            GetColorU32(ImGuiCol_FrameBgHovered), 3.f);

    // Draw gear (⚙) using simple lines: outer circle + 6 teeth + inner circle
    {
        const ImVec2 c(gear_x + gear_w * 0.5f, pos.y + sz * 0.5f);
        const float  r_out  = sz * 0.28f;
        const float  r_in   = sz * 0.16f;
        const float  r_tooth= sz * 0.36f;
        const ImU32  col    = GetColorU32(g_hov
            ? ImGuiCol_Text : ImGuiCol_TextDisabled);
        const float  th     = ImMax(1.f, sz * 0.08f);
        const int    teeth  = 6;
        const float  pi2    = 6.28318530f;
        // Outer ring
        dl->AddCircle(c, r_out, col, 32, th);
        // Inner circle
        dl->AddCircleFilled(c, r_in, col, 16);
        // Teeth
        for (int i = 0; i < teeth; ++i)
        {
            const float a0 = pi2 * i / teeth - 0.18f;
            const float a1 = pi2 * i / teeth + 0.18f;
            ImVec2 p0(c.x + cosf(a0) * r_out,  c.y + sinf(a0) * r_out);
            ImVec2 p1(c.x + cosf(a1) * r_out,  c.y + sinf(a1) * r_out);
            ImVec2 p2(c.x + cosf(a1) * r_tooth,c.y + sinf(a1) * r_tooth);
            ImVec2 p3(c.x + cosf(a0) * r_tooth,c.y + sinf(a0) * r_tooth);
            dl->AddQuadFilled(p0, p1, p2, p3, col);
        }
    }

    // Open keybind popup on gear click
    std::string popup_id = std::string(label) + "##kb_popup";
    if (gear_pressed)
    {
        SetNextWindowPos(GetMousePos());
        OpenPopup(popup_id.c_str());
    }

    // Keybind popup (Hold / Toggle / Always / Clear + key capture)
    bool kb_changed = false;
    int current_mode = activation_mode ? *activation_mode : 1;
    float max_tw = ImMax(ImMax(CalcTextSize("Hold").x, CalcTextSize("Toggle").x),
                         ImMax(CalcTextSize("Always").x, CalcTextSize("Clear").x));
    PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
    SetNextWindowSize(ImVec2(max_tw + style.FramePadding.x * 2.f, 0), ImGuiCond_Always);
    if (BeginPopup(popup_id.c_str()))
    {
        if (SelectableLabel("Hold",   current_mode == 1)) { if (activation_mode) *activation_mode = 1; CloseCurrentPopup(); }
        if (SelectableLabel("Toggle", current_mode == 0)) { if (activation_mode) *activation_mode = 0; CloseCurrentPopup(); }
        if (SelectableLabel("Always", current_mode == 2)) { if (activation_mode) *activation_mode = 2; CloseCurrentPopup(); }
        Separator();
        if (SelectableLabel("Clear",  false)) { if (k) *k = ImGuiKey_None; CloseCurrentPopup(); }

        // Capture next key press
        if (k && IsWindowFocused())
        {
            for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key)
            {
                if (IsKeyPressed((ImGuiKey)key) && key != ImGuiKey_Escape)
                { *k = (ImGuiKey)key; kb_changed = true; CloseCurrentPopup(); break; }
            }
            if (IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();
        }
        EndPopup();
    }
    PopStyleVar();

    // ── Checkbox (right of label, left of gear) ───────────────────────────
    const float cb_x = gear_x - gap - cb_w;
    SetCursorScreenPos(ImVec2(cb_x, pos.y));
    bool cb_changed = CheckBox((std::string("##en_") + label).c_str(), v);

    // Restore cursor to next line
    SetCursorScreenPos(ImVec2(pos.x, pos.y + sz + style.ItemSpacing.y));

    return cb_changed || kb_changed;
}

bool ImAdd::Button(const char* label, const ImVec2& size_arg, ImDrawFlags draw_flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    const float hover_t = GlowAlpha(id ^ 0x9E3779B1u, hovered, 0.1f);
    const float held_t  = GlowAlpha(id ^ 0x517CC1B7u, hovered && held, 0.1f);
    const ImVec4 slot    = ImVec4(41/255.f, 46/255.f, 66/255.f, 1.f);  // #292e42
    const ImVec4 slot_h  = ImVec4(50/255.f, 56/255.f, 66/255.f, 1.f);  // #323842
    const ImVec4 colFrame4 = ImLerp(ImLerp(slot, slot_h, hover_t), slot_h, held_t);
    ImU32 colFrame = GetColorU32(colFrame4);

    RenderNavCursor(total_bb, id);
    const float rounding = 8.f;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
    RenderFrame(total_bb.Min, total_bb.Max, colFrame, false, rounding);
    ImGui::PopStyleVar();
	RenderText(pos + ImTrunc((size - label_size) / 2), label, NULL, true, false);

    return pressed;
}

bool ImAdd::ButtonAccent(const char* label, const ImVec2& size_arg, ImDrawFlags draw_flags)
{
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Header]);
    PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_HeaderHovered]);
    PushStyleColor(ImGuiCol_ButtonActive, style.Colors[ImGuiCol_HeaderActive]);
    PushStyleColor(ImGuiCol_ButtonShadow, style.Colors[ImGuiCol_FrameBgShadow]);

    bool result = Button(label, size_arg, draw_flags);

	PopStyleColor(3);

    return result;
}

bool ImAdd::Combo(const char* label, int* selected_index, std::vector<const char*> items)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImFont* font = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : g.Font;
    const float label_sz_f = MenuTheme::FontPx(MenuTheme::Typography::kLabelSize + 2.f);

    // Visible label text stops at "##" (ImGui's hidden-id-suffix convention).
    // Combos that only carry an id (e.g. "## Mode") render with no label and
    // keep the dropdown box at full width, exactly like before.
    const char* label_display_end = FindRenderedTextEnd(label);
    const bool has_label = label_display_end > label;
    const ImVec2 label_size_px = has_label ? font->CalcTextSizeA(label_sz_f, FLT_MAX, 0.f, label, label_display_end) : ImVec2(0.f, 0.f);

    const float total_w = CalcItemWidth();
    const float height  = GetFrameHeight() + 2.f;

    int items_count = (int)items.size();

    const ImVec2 pos = window->DC.CursorPos;

    const float label_h = has_label ? label_size_px.y + 5.f : 0.f;
    const ImVec2 frame_pos(pos.x, pos.y + label_h);
    const float  width = total_w;
    ImVec2 size = ImVec2(width, height);

    const ImRect total_bb(pos, pos + ImVec2(total_w, label_h + height));
    const ImRect frame_bb(frame_pos, frame_pos + size);
    ItemSize(ImVec2(total_w, label_h + height));
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors — only the dropdown box itself is clickable, not the label
    bool hovered, held;
    bool pressed = ButtonBehavior(frame_bb, id, &hovered, &held);

    std::string popup_str_id = std::string(std::string(label) + "::combo_popup");

    if (pressed)
    {
        OpenPopup(popup_str_id.c_str());
    }

    PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.FramePadding.y));
    if (BeginPopupEx(GetID(popup_str_id.c_str()), ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
    {
        SetWindowPos(frame_pos + ImVec2(0, height + style.FramePadding.y), ImGuiCond_Always);
        SetWindowSize(ImVec2(width, ImGui::GetFontSize() * items_count + style.FramePadding.y * (items_count + 1)), ImGuiCond_Always);

        for (int i = 0; i < items_count; i++)
        {
            if (ImAdd::SelectableLabel(items[i], i == *selected_index, true, ImVec2(GetContentRegionAvail().x, GetFontSize())))
            {
                *selected_index = i;
                CloseCurrentPopup();
            }
        }

        EndPopup();
    }
    PopStyleVar(2);

    // Colors — crossfade idle/hovered/active over a fixed 100ms ease-in-out,
    // frame-rate independent, matching every other control's hover feedback
    const float hover_t = GlowAlpha(id ^ 0x9E3779B1u, hovered, 0.1f);
    (void)held;

    RenderNavCursor(frame_bb, id);

    const ImU32 combo_bg = ImGui::ColorConvertFloat4ToU32(ImLerp(
        ImVec4(41/255.f, 46/255.f, 66/255.f, 1.f),
        ImVec4(50/255.f, 56/255.f, 66/255.f, 1.f), hover_t));
    MenuTheme::AddRectFilledCrisp(window->DrawList, frame_bb.Min, frame_bb.Max, combo_bg, 6.f);
    {
        const ImVec2 o0(IM_ROUND(frame_bb.Min.x) + 0.5f, IM_ROUND(frame_bb.Min.y) + 0.5f);
        const ImVec2 o1(IM_ROUND(frame_bb.Max.x) - 0.5f, IM_ROUND(frame_bb.Max.y) - 0.5f);
        window->DrawList->AddRect(o0, o1, IM_COL32(65, 72, 104, 255), 6.f, 0, 1.f);
    }

    std::string preview_item;
    if (*selected_index > items.size()) {
        preview_item = "*unknown item*";
    }
    else
    {
        preview_item = items[*selected_index];
    }

    // Preview text — white, with a subtle dark 1px border around the text for
    // legibility against the frame background.
    float font_size = MenuTheme::FontPx(MenuTheme::Typography::kValueSize + 2.f);

    const float arrow_area = GetFontSize() + style.FramePadding.x;
    const ImVec2 text_sz   = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, preview_item.c_str());
    ImVec2 text_pos;
    text_pos.x = roundf(frame_pos.x + style.FramePadding.x + 2.f);
    text_pos.y = roundf(frame_pos.y + (height - text_sz.y) * 0.5f);

    window->DrawList->AddText(font, font_size, text_pos, IM_COL32(236, 236, 244, 255), preview_item.c_str());

    // Triangle dropdown arrow — use the PNG icon if loaded, else fall back to ImGui's built-in
    {
        const float  ar   = GetFontSize();          // arrow region size
        const float  ax   = frame_pos.x + width - ar - style.FramePadding.x;
        const float  ay   = frame_pos.y + (height - ar) * 0.5f;
        const ImVec2 amin(ax, ay);
        const ImVec2 amax(ax + ar, ay + ar);
        if (MenuTheme::combo_arrow_srv)
            window->DrawList->AddImage(
                ImTextureRef((ImTextureID)MenuTheme::combo_arrow_srv),
                amin, amax,
                ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
                IM_COL32(65, 72, 104, 255));
        else
            RenderArrow(window->DrawList, frame_pos + ImVec2(width - GetFontSize() - style.FramePadding.x, style.FramePadding.y), IM_COL32(65, 72, 104, 255), ImGuiDir_Down);
    }

    // Label — drawn at the Typography::kLabelSize/kLabelColor hierarchy tier,
    // vertically centered against the dropdown box
    if (has_label)
    {
        window->DrawList->AddText(font, label_sz_f, pos, MenuTheme::Typography::kLabelColor, label, label_display_end);
    }

    return pressed;
}

bool ImAdd::ColorEdit4(const char* label, float col[4], const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImVec4 col_v4(col[0], col[1], col[2], col[3]);
	const ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip;

    const ImVec2 size(
        size_arg.x > 0.f ? size_arg.x : ImAdd::kColorSwatchWidth,
        size_arg.y > 0.f ? size_arg.y : ImAdd::kColorSwatchSize);

    const float line_h = window->DC.CurrLineSize.y;
    ImVec2 swatch_pos = window->DC.CursorPos;
    // Vertically center the swatch on the current line (e.g. beside a checkbox label).
    if (window->DC.IsSameLine && line_h > size.y)
        swatch_pos.y += (line_h - size.y) * 0.5f;
    ImGuiID id = window->GetID(label);
    const ImVec2 item_size(size.x, window->DC.IsSameLine ? ImMax(size.y, line_h) : size.y);
    ItemSize(item_size);
    const ImRect swatch_bb(swatch_pos, swatch_pos + size);
    if (!ItemAdd(swatch_bb, id)) return false;
    bool hovered, held;
    bool pressed = ButtonBehavior(swatch_bb, id, &hovered, &held);
    const float rounding = 4.f;
    window->DrawList->AddRectFilled(swatch_bb.Min, swatch_bb.Max, GetColorU32(col_v4), rounding);

    if (pressed)
    {
		OpenPopup(label);
    }

    if (BeginPopup(label))
    {
        ColorPicker4(label, col, flags);
        EndPopup();
    }

    return false;
}

bool ImAdd::KeyBind(const char* str_id, ImGuiKey* k, const ImVec2& size_arg, int* activation_mode)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiIO& io = g.IO;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    ImVec2 pos = window->DC.CursorPos;

    char buf_display[32] = "Not Bound";

    bool is_selecing = false;
    bool is_bound    = false;

    if (*k != ImGuiKey_None && *k != 0 && g.ActiveId != id)
    {
        int vk = keybind::imgui_key_to_vk(*k);
        const char* name = keybind::get_key_name(vk);
        snprintf(buf_display, sizeof(buf_display), "%s", name);
        is_bound = true;
    }
    else if (g.ActiveId == id)
    {
        is_selecing = true;
        strcpy_s(buf_display, sizeof buf_display, "Press any key...");
    }

    ImFont* kfont = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : GetFont();
    const float ksz = MenuTheme::FontPx(11.f);
    const ImVec2 name_sz = kfont->CalcTextSizeA(ksz, FLT_MAX, 0.f, buf_display);
    const float pill_h = 20.f;
    const float pill_w = ImMax(size_arg.x > 0.f ? size_arg.x : (name_sz.x + 16.f), 28.f);
    ImVec2 size = ImVec2(pill_w, pill_h);
    ImRect frame_bb(pos, pos + size);
    ImRect total_bb(pos, frame_bb.Max);

    ImGui::ItemSize(total_bb, style.CellPadding.y);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    const bool hovered = ImGui::ItemHoverable(frame_bb, id, 0);

    // Icon color: bright white while capturing, soft white when a key is
    // already bound, dim grey when unbound — same tiers used by the sidebar
    // settings icon (selected/idle). Each tier crossfades over a fixed 100ms
    // ease-in-out instead of snapping instantly.
    const float bound_t  = GlowAlpha(id ^ 0xAAAAAAAAu, is_bound,   0.1f);
    const float active_t = GlowAlpha(id ^ 0xBBBBBBBBu, is_selecing, 0.1f);
    const ImVec4 colLabel = ImLerp(
        ImLerp(GetStyleColorVec4(ImGuiCol_TextDisabled), ImVec4(1.f, 1.f, 1.f, 0.85f), bound_t),
        ImVec4(1.f, 1.f, 1.f, 1.f), active_t);

    const bool user_clicked = hovered && IsMouseClicked(ImGuiMouseButton_Left);
    const bool right_clicked = hovered && IsMouseClicked(ImGuiMouseButton_Right);

    std::string popup_id = std::string(str_id) + "##keybind_popup";

    bool value_changed = false;

    if (right_clicked)
    {
        SetNextWindowPos(GetMousePosOnOpeningCurrentPopup());
        OpenPopup(popup_id.c_str());
    }

    int current_mode = activation_mode != nullptr ? *activation_mode : 1;
    
    float max_text_width = 0.0f;
    max_text_width = ImMax(max_text_width, CalcTextSize("Hold").x);
    max_text_width = ImMax(max_text_width, CalcTextSize("Toggle").x);
    max_text_width = ImMax(max_text_width, CalcTextSize("Always").x);
    max_text_width = ImMax(max_text_width, CalcTextSize("Clear").x);
    
    float popup_width = max_text_width + style.FramePadding.x * 2.0f;
    
    PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
    SetNextWindowSize(ImVec2(popup_width, 0), ImGuiCond_Always);
    if (BeginPopup(popup_id.c_str()))
    {
        if (ImAdd::SelectableLabel("Hold", current_mode == 1))
        {
            if (activation_mode != nullptr)
                *activation_mode = 1;
            CloseCurrentPopup();
        }
        if (ImAdd::SelectableLabel("Toggle", current_mode == 0))
        {
            if (activation_mode != nullptr)
                *activation_mode = 0;
            CloseCurrentPopup();
        }
        if (ImAdd::SelectableLabel("Always", current_mode == 2))
        {
            if (activation_mode != nullptr)
                *activation_mode = 2;
            CloseCurrentPopup();
        }
        Separator();
        if (ImAdd::SelectableLabel("Clear", false))
        {
            *k = ImGuiKey_None;
            value_changed = true;
            CloseCurrentPopup();
        }
        EndPopup();
    }
    PopStyleVar();

    static std::map<ImGuiID, int> activation_frame;
    static std::map<ImGuiID, bool> mouse_was_down[5];

    if (user_clicked)
    {
        ImGui::SetActiveID(id, window);
        ImGui::FocusWindow(window);
        activation_frame[id] = g.FrameCount;
        mouse_was_down[0][id] = IsMouseDown(ImGuiMouseButton_Left);
        mouse_was_down[1][id] = IsMouseDown(ImGuiMouseButton_Right);
        mouse_was_down[2][id] = IsMouseDown(ImGuiMouseButton_Middle);
    }
    else if (IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (g.ActiveId == id)
            ImGui::ClearActiveID();
    }
    int key = *k;

    if (hovered && IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (g.ActiveId != id)
        {
            // Start capturing
            memset(io.MouseDown, 0, sizeof(io.MouseDown));
            SetActiveID(id, window);
            FocusWindow(window);
            activation_frame[id] = g.FrameCount;
            mouse_was_down[0][id] = IsMouseDown(ImGuiMouseButton_Left);
            mouse_was_down[1][id] = IsMouseDown(ImGuiMouseButton_Right);
            mouse_was_down[2][id] = IsMouseDown(ImGuiMouseButton_Middle);
        }
    }

    if (IsMouseClicked(ImGuiMouseButton_Left) && g.ActiveId == id && !hovered)
    {
        // Clicked outside - cancel
        ClearActiveID();
        activation_frame.erase(id);
        mouse_was_down[0].erase(id);
        mouse_was_down[1].erase(id);
        mouse_was_down[2].erase(id);
        mouse_was_down[3].erase(id);
        mouse_was_down[4].erase(id);
    }

    // Handle key capture
    if (g.ActiveId == id)
    {
        static std::map<ImGuiID, bool> prev_mouse_down[5];
        
        bool skip_mouse = false;
        auto it_frame = activation_frame.find(id);
        if (it_frame != activation_frame.end())
        {
            if (g.FrameCount <= it_frame->second + 3)
            {
                skip_mouse = true;
            }
            else
            {
                activation_frame.erase(id);
            }
        }

        bool left_now = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool right_now = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        bool middle_now = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
        bool x1_now = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
        bool x2_now = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;
        
        if (skip_mouse)
        {
            prev_mouse_down[0][id] = left_now;
            prev_mouse_down[1][id] = right_now;
            prev_mouse_down[2][id] = middle_now;
            prev_mouse_down[3][id] = x1_now;
            prev_mouse_down[4][id] = x2_now;
        }
        else if (!value_changed)
        {
            bool left_prev = prev_mouse_down[0][id];
            bool right_prev = prev_mouse_down[1][id];
            bool middle_prev = prev_mouse_down[2][id];
            bool x1_prev = prev_mouse_down[3][id];
            bool x2_prev = prev_mouse_down[4][id];
            
            if (left_now && !left_prev)
            {
                *k = ImGuiKey_MouseLeft;
                value_changed = true;
                ClearActiveID();
                activation_frame.erase(id);
                mouse_was_down[0].erase(id);
                mouse_was_down[1].erase(id);
                mouse_was_down[2].erase(id);
                prev_mouse_down[0].erase(id);
                prev_mouse_down[1].erase(id);
                prev_mouse_down[2].erase(id);
                prev_mouse_down[3].erase(id);
                prev_mouse_down[4].erase(id);
            }
            else if (right_now && !right_prev)
            {
                *k = ImGuiKey_MouseRight;
                value_changed = true;
                ClearActiveID();
                activation_frame.erase(id);
                mouse_was_down[0].erase(id);
                mouse_was_down[1].erase(id);
                mouse_was_down[2].erase(id);
                prev_mouse_down[0].erase(id);
                prev_mouse_down[1].erase(id);
                prev_mouse_down[2].erase(id);
                prev_mouse_down[3].erase(id);
                prev_mouse_down[4].erase(id);
            }
            else if (middle_now && !middle_prev)
            {
                *k = ImGuiKey_MouseMiddle;
                value_changed = true;
                ClearActiveID();
                activation_frame.erase(id);
                mouse_was_down[0].erase(id);
                mouse_was_down[1].erase(id);
                mouse_was_down[2].erase(id);
                prev_mouse_down[0].erase(id);
                prev_mouse_down[1].erase(id);
                prev_mouse_down[2].erase(id);
                prev_mouse_down[3].erase(id);
                prev_mouse_down[4].erase(id);
            }
            else if (x1_now && !x1_prev)
            {
                *k = ImGuiKey_MouseX1;
                value_changed = true;
                ClearActiveID();
                activation_frame.erase(id);
                mouse_was_down[0].erase(id);
                mouse_was_down[1].erase(id);
                mouse_was_down[2].erase(id);
                prev_mouse_down[0].erase(id);
                prev_mouse_down[1].erase(id);
                prev_mouse_down[2].erase(id);
                prev_mouse_down[3].erase(id);
                prev_mouse_down[4].erase(id);
            }
            else if (x2_now && !x2_prev)
            {
                *k = ImGuiKey_MouseX2;
                value_changed = true;
                ClearActiveID();
                activation_frame.erase(id);
                mouse_was_down[0].erase(id);
                mouse_was_down[1].erase(id);
                mouse_was_down[2].erase(id);
                prev_mouse_down[0].erase(id);
                prev_mouse_down[1].erase(id);
                prev_mouse_down[2].erase(id);
                prev_mouse_down[3].erase(id);
                prev_mouse_down[4].erase(id);
            }
            else
            {
                prev_mouse_down[0][id] = left_now;
                prev_mouse_down[1][id] = right_now;
                prev_mouse_down[2][id] = middle_now;
                prev_mouse_down[3][id] = x1_now;
                prev_mouse_down[4][id] = x2_now;
            }
        }

        // Check keyboard keys if no mouse button was pressed
        if (!value_changed)
        {
            // Check all possible keys
            for (int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; i++) // only named keyboard/gamepad keys
            {
                ImGuiKey key_test = (ImGuiKey)i;

                // Skip mouse inputs (already handled above) and escape
                if ((key_test >= ImGuiKey_MouseLeft && key_test <= ImGuiKey_MouseWheelY) || key_test == ImGuiKey_Escape)
                    continue;

                if (IsKeyPressed(key_test)) // Pressed, not Down, avoids "instant bind"
                {
                    *k = key_test;
                    value_changed = true;
                    ClearActiveID();
                    activation_frame.erase(id);
                    mouse_was_down[0].erase(id);
                    mouse_was_down[1].erase(id);
                    mouse_was_down[2].erase(id);
                    break;
                }
            }
        }

        // Escape cancels
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            ClearActiveID();
            activation_frame.erase(id);
            mouse_was_down[0].erase(id);
            mouse_was_down[1].erase(id);
            mouse_was_down[2].erase(id);
        }
    }

    // Render — small clickable settings/gear icon (matches the "E" glyph used
    // for the Settings tab in the sidebar) instead of a text-filled rectangle.
    ImGui::RenderNavHighlight(total_bb, id);

    // Subtle hover/active background tint, same treatment as EnableRow's gear
    // — fades in/out over 100ms instead of snapping on/off
    MenuTheme::AddRectFilledCrisp(window->DrawList, pos, pos + size, IM_COL32(41, 46, 66, 255), ImMin(size.x, size.y) * 0.5f);
    {
        char pill[32];
        snprintf(pill, sizeof(pill), "%s", buf_display);
        for (char* p = pill; *p; ++p)
            *p = (char)tolower((unsigned char)*p);
        const ImVec2 tsz = kfont->CalcTextSizeA(ksz, FLT_MAX, 0.f, pill);
        window->DrawList->AddText(kfont, ksz,
            ImVec2(pos.x + (size.x - tsz.x) * 0.5f, pos.y + (size.y - tsz.y) * 0.5f),
            IM_COL32(122, 131, 165, 255), pill);
    }

    if (hovered && !is_selecing)
    {
        ImGui::SetTooltip("%s\nLeft Click to rebind \xC2\xB7 Right Click for mode", buf_display);
    }

    return value_changed;
}

bool ImAdd::Tab(const char* label, bool selected, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    // Rendered at a fixed, explicit size — slightly bigger than widget labels
    // (kLabelSize) but smaller than the section's tab header (kTabHeaderSize)
    // — rather than whatever font size happens to be ambient at the call site.
    const float text_sz = MenuTheme::FontPx(MenuTheme::Typography::kSubTabSize);
    ImFont* font = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : GetFont();
    const char* text_end = FindRenderedTextEnd(label);
    const ImVec2 label_size = font->CalcTextSizeA(text_sz, FLT_MAX, 0.f, label, text_end);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    (void)hovered; (void)held;

    // Dedicated 150ms ease-in-out transition for the selected <-> not-selected
    // state — drives the underline opacity, glow strength, and text-color
    // cross-fade whenever the tab selection changes.
    const float select_t = GlowAlpha(id, selected, 0.15f);

    RenderNavCursor(bb, id);

    // No button/pill background is drawn — the tab is plain text; only the
    // underline, glow, and text color communicate hover/active state.
    constexpr float kUnderlineH = 2.f; // underline thickness
    constexpr float kTextGap    = 4.f; // padding between text and underline

    // Text is horizontally centered and bottom-anchored above a fixed-height
    // reserved strip (gap + underline), so its position never shifts when the
    // tab is (de)selected — only color/underline opacity animate.
    const float text_bottom = bb.Max.y - kUnderlineH - kTextGap;
    const ImVec2 text_pos(pos.x + ImTrunc((size.x - label_size.x) / 2.f), text_bottom - label_size.y);

    // No shadow/glow behind text (user request)
    // Text cross-fades from light gray (#888888) when inactive to pure white
    // (#ffffff) when active, in lockstep with the underline/glow.
    const ImVec4 col_inactive(0.42f, 0.42f, 0.46f, 1.f);
    const ImVec4 col_active(0.86f, 0.86f, 0.90f, 1.f);
    const ImU32 text_col = GetColorU32(ImLerp(col_inactive, col_active, select_t));
    window->DrawList->AddText(font, text_sz, text_pos, text_col, label, text_end);


    return pressed;
}

void ImAdd::ScrollBar(const char* str_id, ImGuiWindow* window, const ImVec2& size_arg)
{
    if (!window || window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, GetFrameHeight(), CalcItemWidth());
    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return;

    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    // Scroll metrics
    float visible_height = size.y;
    float total_height = window->ContentSize.y;
    float scroll_max = ImMax(window->ScrollMax.y, 0.0f);
    float scroll_y = window->Scroll.y;

    float scroll_height = (total_height > 0.0f)
        ? (visible_height / total_height) * visible_height
        : visible_height;

    scroll_height = ImClamp(scroll_height, 15.0f, visible_height);

    float scroll_top = (scroll_max > 0.0f)
        ? (scroll_y / scroll_max) * (visible_height - scroll_height)
        : 0.0f;

    // Handle drag-to-scroll
    if (held && scroll_max > 0.0f)
    {
        float mouse_delta = g.IO.MouseDelta.y;
        float scrollable_range = visible_height - scroll_height;
        if (scrollable_range > 0.0f)
        {
            float ratio = scroll_max / scrollable_range;
            window->Scroll.y = ImClamp(window->Scroll.y + mouse_delta * ratio, 0.0f, scroll_max);
        }
    }

	// Handle mouse wheel scrolling
    bool hovered_window = IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    if (!held && (hovered || hovered_window) && scroll_max > 0.0f && !IsKeyDown(ImGuiKey_LeftShift))
    {
        // Disable native ImGui scrolling
        window->Flags |= ImGuiWindowFlags_NoScrollWithMouse;

        const float wheel_speed = 40.0f; // tweak to taste
        window->Scroll.y = ImClamp(
            window->Scroll.y - g.IO.MouseWheel * wheel_speed,
            0.0f,
            scroll_max
        );
    }

    // Grab color — accent when active/hovered, dimmer at rest
    const ImVec4 accent     = GetStyleColorVec4(ImGuiCol_SliderGrab);
    const float  grab_t     = GlowAlpha(id, hovered || held, 0.15f);
    const ImVec4 grab_rest  (0.28f, 0.28f, 0.32f, 1.f);
    const ImVec4 grab_col   = ImLerp(grab_rest, accent, grab_t);

    const ImVec2 grab_min(total_bb.Min.x, total_bb.Min.y + scroll_top);
    const ImVec2 grab_max(total_bb.Max.x, total_bb.Min.y + scroll_top + scroll_height);
    const ImRect grab_bb(grab_min, grab_max);

    // Draw track
    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max,
        GetColorU32(ImGuiCol_ScrollbarBg), style.ScrollbarRounding);

    // Glow behind the grab — same gaussian as buttons/sliders, fades in on hover
    DrawGlowRect(window->DrawList, grab_bb, style.ScrollbarRounding,
        grab_t * 0.5f, ImVec4(accent.x, accent.y, accent.z, 0.16f), 8.f);

    // Draw grab
    window->DrawList->AddRectFilled(grab_min, grab_max,
        GetColorU32(grab_col), style.ScrollbarRounding);

    RenderNavCursor(total_bb, id);
}

bool ImAdd::BeginChild(const char* str_id, std::vector<const char*> tabs, int* selected_tab_index_callback, const ImVec2& size_arg, const char* badge, ImVec4 badge_col)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* parent_window = g.CurrentWindow;
    //if (parent_window->SkipItems)
    //    return;

    const ImGuiID id = parent_window->GetID(str_id);
    const ImGuiStyle& style = g.Style;

    std::string str_id_tabs      = std::string(str_id) + "##child##tabs";
    std::string str_id_scrollbar = std::string(str_id) + "##child##scrollbar";

    bool has_tabs   = tabs.size() > 0;
    bool has_badge  = badge && badge[0] != '\0';
    // A header (title text + optional badge) is drawn whenever there are
    // real subtabs to show OR a badge was explicitly requested — this lets
    // sections show the same tab-header treatment without needing a
    // functional, clickable sub-tab strip underneath it.
    bool has_header = has_tabs || has_badge;
    float tabs_height = GetFrameHeight();
    // Height of the floating title bar — sized to fit the tab-header text.
    // The "Active"/"Disabled" badge now sits inline at the far right of this
    // same row, so it no longer needs extra reserved height below the title.
    const float title_bar_h  = has_header ? (MenuTheme::Sz(MenuTheme::Typography::kTabHeaderSize) + style.FramePadding.y * 2.f) : 0.f;
    // Visible gap between title bar and the main panel
    const float title_gap    = has_header ? MenuTheme::Sz(MenuTheme::Typography::kTabHeaderPadBottom) : 0.f;
    // Total space reserved above the tabs inside the child
    const float title_h      = has_header ? title_bar_h + title_gap : 0.f;
    // Small gap between the tab strip and the section content below it
    const float tabs_bottom_pad = has_tabs ? 4.f : 0.f;

    // Before BeginChild: draw the floating title bar + main panel background on the
    // parent draw list so draw order is guaranteed correct.
    if (has_header)
    {
        const ImVec2 child_pos  = parent_window->DC.CursorPos;
        const ImVec2 child_size = ImVec2(
            size_arg.x > 0.f ? size_arg.x : parent_window->WorkRect.Max.x - child_pos.x,
            size_arg.y > 0.f ? size_arg.y : parent_window->WorkRect.Max.y - child_pos.y);

        // Title text — bold white, uppercase, centred, with a white underline
        // and a soft gaussian glow behind it (same DrawGlowRect as slider fill).
        const char* display = str_id;
        while (display[0] == '#') ++display;
        if (display[0] != '\0')
        {
            std::string display_upper(display);
            for (char& c : display_upper)
                c = ImToUpper(c);

            const float title_sz = MenuTheme::FontPx(MenuTheme::Typography::kTabHeaderSize);
            ImFont* title_font   = MenuTheme::Fonts::Bold ? MenuTheme::Fonts::Bold : GetFont();
            const float text_w   = MenuTheme::CalcSpacedTextWidth(title_font, title_sz, display_upper.c_str(), MenuTheme::Sz(MenuTheme::Typography::kTabHeaderLetterSpacing));
            const float text_h   = title_font->CalcTextSizeA(title_sz, FLT_MAX, 0.f, display_upper.c_str()).y;

            const float tx = child_pos.x + (child_size.x - text_w) * 0.5f;
            const float ty = child_pos.y + style.FramePadding.y;

            // Title text
            MenuTheme::AddTextSpaced(
                parent_window->DrawList, title_font, title_sz,
                ImVec2(tx, ty),
                MenuTheme::Typography::kTabHeaderColor, display_upper.c_str(),
                MenuTheme::Sz(MenuTheme::Typography::kTabHeaderLetterSpacing));
        }

        // Badge text ("Active"/"Disabled") — bottom-right corner of the child
        // panel. Hidden while the last sub-tab ("Settings") is active so it
        // doesn't clutter the settings view.
        if (has_badge)
        {
            // Resolve the current tab index to decide visibility
            const int cur_tab   = (has_tabs && selected_tab_index_callback) ? *selected_tab_index_callback : 0;
            const int last_tab  = has_tabs ? (int)tabs.size() - 1 : -1;
            const bool on_settings = has_tabs && (cur_tab == last_tab) && (last_tab > 0);

            if (!on_settings)
            {
                ImFont* badge_font = MenuTheme::Fonts::Regular ? MenuTheme::Fonts::Regular : GetFont();
                const float badge_sz = MenuTheme::FontPx(MenuTheme::Typography::kBadgeSize);
                const float badge_w  = badge_font->CalcTextSizeA(badge_sz, FLT_MAX, 0.f, badge).x;
                // Bottom-right: align to bottom edge of child panel, right-padded
                const float bx = child_pos.x + child_size.x - style.ChildPadding.x - badge_w;
                const float by = child_pos.y + child_size.y - style.ChildPadding.y - badge_sz;
                parent_window->DrawList->AddText(badge_font, badge_sz, ImVec2(bx, by), GetColorU32(badge_col), badge);
            }
        }

        // Main panel background intentionally omitted — no fill behind the
        // widgets, they sit directly on the main window background.
    }

    PushStyleVar(ImGuiStyleVar_WindowPadding, style.ChildPadding);
    bool result = ImGui::BeginChild(str_id, size_arg, ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    PopStyleVar();

    // Rows inside a section use a tighter, fixed vertical rhythm regardless of
    // the ambient spacing used between sections. Popped in ImAdd::EndChild(),
    // which callers only invoke when this BeginChild returned true.
    if (result)
        PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, MenuTheme::Sz(MenuTheme::Typography::kRowSpacing)));

    //if (result)
    {
        ImGuiWindow*    window  = GetCurrentWindow();
        ImVec2          cur_pos = window->DC.CursorPos;
        ImVec2          pos     = window->Pos;
        ImVec2          size    = window->Size;
        ImRect          window_bb(pos, pos + size);
        bool            has_scroll = window->ScrollMax.y > 0;

        // Content fade progress for this section — eased 0..1 over 150ms,
        // reset to 0 whenever the active subtab changes. Read back in
        // EndChild() (keyed by the same id) to fade the newly-revealed
        // content in from the background instead of popping instantly.
        float content_fade = 1.f;

        if (has_tabs)
        {
            // Tabs sit below the title row + gap
            SetCursorScreenPos(pos + ImVec2(0.0f, style.ChildBorderSize + title_h));
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.ChildBorderSize, 0.0f));
            if (ImGui::BeginChild(str_id_tabs.c_str(), ImVec2(size.x, tabs_height), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground))
            {
                PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, style.ItemSpacing.y));
                PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

                float tab_width = ImTrunc((size.x - style.ChildBorderSize * (tabs.size() + 1)) / tabs.size());

                static std::map<ImGuiID, int> s_tab_index;
                auto it_idx = s_tab_index.find(id);
                if (it_idx == s_tab_index.end())
                    it_idx = s_tab_index.insert({ id, 0 }).first;

                const int tab_count = (int)tabs.size();
                for (int i = 0; i < tab_count; i++)
                {
                    bool selected = selected_tab_index_callback
                        ? (*selected_tab_index_callback == i)
                        : (it_idx->second == i);

                    // No background/pill is drawn behind tabs — Tab() renders
                    // plain text with an underline + glow on the active tab only.
                    float this_tab_w = (i == tab_count - 1) ? GetContentRegionAvail().x : tab_width;

                    if (Tab(tabs[i], selected, ImVec2(this_tab_w, tabs_height)))
                    {
                        if (selected_tab_index_callback)
                            *selected_tab_index_callback = i;
                        else
                            it_idx->second = i;
                    }

                    if (i < tab_count - 1)
                        SameLine(0, 0);
                }

                PopStyleVar(2);

                // Track the resolved tab index across frames — resetting the
                // fade timer to 0 the instant it changes.
                const int cur_sel = selected_tab_index_callback ? *selected_tab_index_callback : it_idx->second;
                static std::map<ImGuiID, int>   s_last_sel;
                static std::map<ImGuiID, float> s_fade_t;
                auto it_last = s_last_sel.find(id);
                if (it_last == s_last_sel.end())
                {
                    s_last_sel.insert({ id, cur_sel });
                    s_fade_t[id] = 1.f; // no fade-in on first appearance
                }
                else if (it_last->second != cur_sel)
                {
                    it_last->second = cur_sel;
                    s_fade_t[id] = 0.f;
                }
                float& fade_p = s_fade_t[id];
                fade_p = ImClamp(fade_p + g.IO.DeltaTime / 0.15f, 0.f, 1.f);
                content_fade = fade_p * fade_p * (3.f - 2.f * fade_p); // smoothstep ease-in-out
            }
            ImGui::EndChild();
            PopStyleVar();
        }

        // Sub-container background fill and border/focus-glow intentionally
        // removed — sections show only their header text and sit directly on
        // the main glassmorphism background, per the flattened, modern look.

		      // Vertical space reserved above the content: title + functional tab
		      // strip when there are real subtabs, just the title when there's
		      // only a header (no subtabs), or nothing at all otherwise.
		      const float reserved_h = has_tabs ? (title_h + tabs_height + tabs_bottom_pad) : title_h;

		      // Stash this section's content-fade progress + the vertical offset
		      // where its content begins, so EndChild() (called later) can draw
		      // the reveal overlay.
		      if (has_tabs && content_fade < 0.999f)
		          g_tab_fade_state[window->ID] = { content_fade, reserved_h };
		      else
		          g_tab_fade_state.erase(window->ID);

		      float scroll_offset_y = style.ChildBorderSize * 3.0f + (has_header ? reserved_h : style.ChildBorderSize);

		      if (has_scroll && !MenuTheme::HidePanelScroll())
		      {
		          SetCursorScreenPos(pos + ImVec2(size.x - style.ScrollbarSize - 2.f, scroll_offset_y + style.ChildPadding.y));
		          ImAdd::ScrollBar(str_id_scrollbar.c_str(), window, ImVec2(style.ScrollbarSize, size.y - scroll_offset_y - style.ChildPadding.y * 2.0f));
		      }

		      SetCursorScreenPos(cur_pos + ImVec2(0.0f, style.ChildBorderSize * 3.0f + (has_header ? reserved_h : 0.0f)));

		      if (has_scroll && !MenuTheme::HidePanelScroll())
		      {
		          window->ContentRegionRect.Max.x -= style.ScrollbarSize + style.ChildPadding.x;
		      }

		      window->DrawList->PushClipRect(window_bb.Min + ImVec2(style.ChildBorderSize, style.ChildBorderSize * 3.0f + (has_header ? reserved_h : style.ChildBorderSize)), window_bb.Max - ImVec2(style.ChildBorderSize, has_header ? 0.0f : style.ChildBorderSize), true);
    }

    PushItemWidth(GetContentRegionAvail().x);

    return result;
}

bool ImAdd::BeginChild(const char* str_id, std::vector<const char*> tabs, const ImVec2& size_arg)
{
	return ImAdd::BeginChild(str_id, tabs, (int*)NULL, size_arg);
}

bool ImAdd::BeginChild(const char* str_id, const ImVec2& size_arg)
{
	return ImAdd::BeginChild(str_id, {}, (int*)NULL, size_arg);
}

bool ImAdd::BeginChild(const char* str_id, const ImVec2& size_arg, const char* badge, ImVec4 badge_col)
{
	return ImAdd::BeginChild(str_id, {}, (int*)NULL, size_arg, badge, badge_col);
}

void ImAdd::EndChild()
{
    PopItemWidth();
    PopStyleVar(); // matches the row-spacing ItemSpacing pushed in BeginChild

    ImGuiWindow* window = GetCurrentWindow();

    // Tab-switch content fade: BeginChild() stashed this section's ease
    // progress (keyed by the child window's own id, computed there). Paint a
    // background-colored overlay over the content area whose alpha runs from
    // opaque (just switched) down to fully transparent (fully faded in) —
    // this "reveals" the new subtab's content over ~150ms without needing
    // every individual widget to know about the fade.
    {
        auto it = g_tab_fade_state.find(window->ID);
        if (it != g_tab_fade_state.end())
        {
            const float fade      = it->second.first;
            const float content_y = it->second.second;
            const ImVec2 omin = window->Pos + ImVec2(0.f, content_y);
            const ImVec2 omax = window->Pos + window->Size;
            const float overlay_alpha = (1.f - fade) * 0.75f; // matches the main window's own bg opacity
            if (overlay_alpha > 0.003f)
                window->DrawList->AddRectFilled(omin, omax, IM_COL32(10, 10, 10, (int)(overlay_alpha * 255.f)));
        }
    }

    window->DrawList->PopClipRect();

    if (MenuTheme::IsFittingContent() && window->Name &&
        !strstr(window->Name, "##child##tabs") &&
        strcmp(window->Name, "##body") != 0 &&
        strcmp(window->Name, "client_body") != 0)
    {
        const float pad = window->WindowPadding.y * 2.f;
        const float needed = window->ContentSize.y + pad;
        MenuTheme::NoteContentHeight(needed);
    }

    ImGui::EndChild();
}

bool ImAdd::BeginReveal(const char* str_id, bool show, float duration_sec)
{
    ImGuiWindow* window = GetCurrentWindow();
    const ImGuiID id = window->GetID(str_id);
    RevealState& st = g_reveal_state[id];

    ImGuiContext& g = *GImGui;
    const float step = (duration_sec > 0.f) ? (g.IO.DeltaTime / duration_sec) : 1.f;
    st.progress = ImClamp(st.progress + (show ? step : -step), 0.f, 1.f);

    if (st.progress <= 0.001f && !show)
        return false; // fully collapsed — draw/measure nothing, take up no space

    g_reveal_stack.push_back(id);

    // Ease-in-out (smoothstep) instead of a linear grow/shrink
    const float eased      = st.progress * st.progress * (3.f - 2.f * st.progress);
    const float animated_h = ImMax(st.height * eased, 1.f);

    PushStyleVar(ImGuiStyleVar_Alpha, g.Style.Alpha * eased);
    ImGui::BeginChild(str_id, ImVec2(0.f, animated_h), 0,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
    return true;
}

void ImAdd::EndReveal()
{
    ImGuiWindow* child = GetCurrentWindow();
    // Natural (un-clipped) content height reached this frame — used to size
    // next frame's animation now that we know how tall the content wants to be.
    const float measured = ImMax(child->DC.CursorMaxPos.y - child->DC.CursorStartPos.y, 0.f);

    ImGui::EndChild();
    PopStyleVar();

    if (!g_reveal_stack.empty())
    {
        const ImGuiID id = g_reveal_stack.back();
        g_reveal_stack.pop_back();
        g_reveal_state[id].height = measured;
    }
}

bool ImAdd::SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImFont* font = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : GetFont();
    const float label_sz_f = MenuTheme::FontPx(MenuTheme::Typography::kLabelSize);
    const float value_sz_f = MenuTheme::FontPx(MenuTheme::Typography::kValueSize);

    const float total_w = ImMax(CalcItemWidth(), GetContentRegionAvail().x);
    const ImVec2 pos = window->DC.CursorPos;

    const char* label_display_end = FindRenderedTextEnd(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const bool has_label = label_size.x > 0;
    const ImVec2 label_size_px = has_label ? font->CalcTextSizeA(label_sz_f, FLT_MAX, 0.f, label, label_display_end) : ImVec2(0.f, 0.f);

    const float label_gap    = has_label ? style.ItemInnerSpacing.x : 0.f;
    const float value_col_w  = 48.f;
    const float frame_height = 7.0f;
    const float label_row_h  = ImMax(label_size_px.y, value_sz_f) + 4.f;
    const float row_h        = label_row_h + 14.f;
    const float frame_x0 = pos.x;
    const float frame_x1 = pos.x + total_w;
    const float frame_y0 = pos.y + label_row_h;

    const ImRect frame_bb(ImVec2(frame_x0, frame_y0), ImVec2(frame_x1, frame_y0 + frame_height));
    const ImRect total_bb(pos, pos + ImVec2(total_w, row_h));

    ItemSize(total_bb);
    if (!ItemAdd(total_bb, id, &frame_bb, 0))
        return false;

    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
    const bool clicked = hovered && IsMouseClicked(0, ImGuiInputFlags_None, id);
    const bool held    = g.ActiveId == id;
    const bool make_active = (clicked || g.NavActivateId == id);

    if (make_active)
    {
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }

    // ── Grab behavior (needed for position) ────────────────────────────────
    ImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, 0, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    static std::map<ImGuiID, float> s_grab_t;
    static std::map<ImGuiID, bool>  s_grab_init;
    const float track_w = ImMax(1.f, frame_bb.Max.x - frame_bb.Min.x);
    const float target_t = ImClamp((grab_bb.GetCenter().x - frame_bb.Min.x) / track_w, 0.f, 1.f);
    auto& grab_t = s_grab_t[id];
    if (!s_grab_init[id]) { grab_t = target_t; s_grab_init[id] = true; }
    grab_t += (target_t - grab_t) * ImClamp(g.IO.DeltaTime / 0.06f, 0.f, 1.f);

    // ── Value display (smoothed like before) ───────────────────────────────
    static std::map<ImGuiID, double> s_display_value;
    const double target_val = (data_type == ImGuiDataType_Float)
        ? (double)*(float*)p_data : (double)*(int*)p_data;
    auto& disp_val = s_display_value[id];
    if (disp_val == 0.0 && target_val != 0.0) disp_val = target_val;
    disp_val += (target_val - disp_val) * ImClamp((double)(g.IO.DeltaTime / 0.1f), 0.0, 1.0);
    const double diff = disp_val - target_val;
    if (diff < 0.0005 && diff > -0.0005) disp_val = target_val;

    char value_buf[64];
    if (data_type == ImGuiDataType_Float)
    {
        float disp_f = (float)disp_val;
        DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, &disp_f, format);
    }
    else
    {
        int disp_i = (int)(disp_val >= 0.0 ? disp_val + 0.5 : disp_val - 0.5);
        DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, &disp_i, format);
    }

    (void)held;
    (void)hovered;

    // value text: dim → brighter when dragging, mid-bright on hover
    const ImVec4 val_col(122/255.f, 131/255.f, 165/255.f, 1.f);

    // label text: dim at rest, brightens on hover (matches EXTERIUM text_color lerp)
    const ImVec4 lbl_col(122/255.f, 131/255.f, 165/255.f, 1.f);

    const float rnd = IM_ROUND(frame_height * 0.5f);

    MenuTheme::AddRectFilledCrisp(window->DrawList, frame_bb.Min, frame_bb.Max,
        IM_COL32(41, 46, 66, 255), rnd);

    const float fill_right = ImClamp(IM_ROUND(frame_bb.Min.x + grab_t * track_w), frame_bb.Min.x, frame_bb.Max.x);
    if (fill_right > frame_bb.Min.x + 1.f)
    {
        const float fill_rnd = ImMin(rnd, IM_ROUND((fill_right - frame_bb.Min.x) * 0.5f));
        MenuTheme::AddRectFilledCrisp(window->DrawList, frame_bb.Min,
            ImVec2(fill_right, frame_bb.Max.y),
            IM_COL32(0, 0, 0, 255), fill_rnd);
    }

    const ImVec2 val_sz_px = font->CalcTextSizeA(value_sz_f, FLT_MAX, 0.f, value_buf);
    const ImVec2 val_pos(pos.x + total_w - val_sz_px.x, pos.y);
    window->DrawList->AddText(font, value_sz_f, val_pos, GetColorU32(val_col), value_buf);

    if (has_label)
    {
        window->DrawList->AddText(font, label_sz_f, ImVec2(pos.x, pos.y), GetColorU32(lbl_col), label, label_display_end);
    }

    return value_changed;
}

bool ImAdd::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format)
{
    return ImAdd::SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format);
}

bool ImAdd::SliderInt(const char* label, int* v, int v_min, int v_max, const char* format)
{
    return ImAdd::SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format);
}

void ImAdd::RenderText(ImVec2 pos, const char* text, const char* text_end, bool hide_text_after_hash, bool has_outlines)
{
    ImGuiContext& g = *GImGui;

    if (has_outlines)
    {
        PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_Border]);

        ImGui::RenderText(pos + ImVec2(0, 1), text, text_end, hide_text_after_hash);
        ImGui::RenderText(pos + ImVec2(0, -1), text, text_end, hide_text_after_hash);
        ImGui::RenderText(pos + ImVec2(1, 0), text, text_end, hide_text_after_hash);
        ImGui::RenderText(pos + ImVec2(-1, 0), text, text_end, hide_text_after_hash);
        ImGui::RenderText(pos + ImVec2(1, 1), text, text_end, hide_text_after_hash);
        ImGui::RenderText(pos + ImVec2(-1, -1), text, text_end, hide_text_after_hash);
        ImGui::RenderText(pos + ImVec2(1, -1), text, text_end, hide_text_after_hash);
        ImGui::RenderText(pos + ImVec2(-1, 1), text, text_end, hide_text_after_hash);

        PopStyleColor();
    }

    ImGui::RenderText(pos, text, text_end, hide_text_after_hash);
}

void ImAdd::ImageRounded(ImDrawList* dl, ImTextureID tex, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, float rounding, ImU32 corner_mask_col)
{
    const ImVec2 p0 = p_min;
    const ImVec2 p1 = p_max;
    rounding = ImMin(rounding, ImMin(p1.x - p0.x, p1.y - p0.y) * 0.5f);

    dl->AddImage(ImTextureRef(tex), p0, p1, uv_min, uv_max);
    if (rounding < 0.5f || (corner_mask_col & IM_COL32_A_MASK) == 0)
        return;

    const float r = rounding;

    // Top-left
    {
        const ImVec2 c(p0.x + r, p0.y + r);
        dl->PathLineTo(p0);
        dl->PathLineTo(ImVec2(p0.x + r, p0.y));
        dl->PathArcTo(c, r, IM_PI, -IM_PI * 0.5f, 8);
        dl->PathFillConvex(corner_mask_col);
    }
    // Top-right
    {
        const ImVec2 c(p1.x - r, p0.y + r);
        dl->PathLineTo(ImVec2(p1.x, p0.y));
        dl->PathLineTo(ImVec2(p1.x - r, p0.y));
        dl->PathArcTo(c, r, -IM_PI * 0.5f, 0.f, 8);
        dl->PathFillConvex(corner_mask_col);
    }
    // Bottom-right
    {
        const ImVec2 c(p1.x - r, p1.y - r);
        dl->PathLineTo(p1);
        dl->PathLineTo(ImVec2(p1.x - r, p1.y));
        dl->PathArcTo(c, r, 0.f, IM_PI * 0.5f, 8);
        dl->PathFillConvex(corner_mask_col);
    }
    // Bottom-left
    {
        const ImVec2 c(p0.x + r, p1.y - r);
        dl->PathLineTo(ImVec2(p0.x, p1.y));
        dl->PathLineTo(ImVec2(p0.x + r, p1.y));
        dl->PathArcTo(c, r, IM_PI * 0.5f, IM_PI, 8);
        dl->PathFillConvex(corner_mask_col);
    }
}

namespace
{
    float                         g_stagger_base_alpha = 1.f;
    float                       (*g_stagger_row_fn)(int) = nullptr;

    float StaggerSmoothstep(float t)
    {
        t = ImClamp(t, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }
}

void ImAdd::SetStaggerAnim(float base_alpha, float (*row_progress)(int row))
{
    g_stagger_base_alpha = base_alpha;
    g_stagger_row_fn     = row_progress;
}

bool ImAdd::BeginStaggerRow(int row)
{
    const float p = g_stagger_row_fn ? g_stagger_row_fn(row) : 1.f;
    if (p <= 0.001f)
        return false;

    const float eased = StaggerSmoothstep(p);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * g_stagger_base_alpha * eased);
    // Slide down from above: start ~18px above, ease to 0. Fast but smooth via smoothstep.
    const float y_off = (1.f - eased) * -18.f;
    if (y_off != 0.f)
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_off);
    ImGui::BeginGroup();
    return true;
}

void ImAdd::EndStaggerRow()
{
    ImGui::EndGroup();
    ImGui::PopStyleVar();
}