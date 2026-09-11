#pragma once

#include "imgui.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace glass
{
    void init(ID3D11Device* device, ID3D11DeviceContext* context);
    void shutdown();
    void invalidate();

    // frost strength 0..1 — 0 = clear glass, 1 = heavy milky frost.
    // drives the wash intensity
    void set_frost(float f);

    // blur radius 0..100 — (kept for API compatibility; DWM acrylic blur is fixed)
    void set_blur(float f);

    // glass tint color (rgb 0..1) + tint strength (a 0..1).
    // rgb drives the DWM acrylic tint / wash color, a scales how much of it is applied.
    void set_tint(float r, float g, float b, float a);

    // sizes/positions the OS-level acrylic backdrop window to the menu rect each frame
    // (legacy single-window entry point; internally uses add_rect/commit)
    void set_menu_rect(float x, float y, float w, float h);

    // --- multi-window acrylic support ----------------------------------------
    // call new_frame() once at the start of the ui frame, add_rect() for every
    // window that should get the acrylic backdrop (menu, lua, players, explorer,
    // ...), and commit() once at the end of the frame.
    void new_frame();
    void add_rect(float x, float y, float w, float h, float rounding = 8.f);
    void commit();

    // --- introspection ------------------------------------------------------
    // this frame's collected glass rects (window coords, in the same space as
    // the ImGui background draw list). Used by the dark menu backdrop to carve
    // holes around the frosted windows so their acrylic blur stays see-through.
    // Valid between new_frame() and the next new_frame().
    int  rect_count();
    bool rect_at(int i, float& x, float& y, float& w, float& h);

    // draws a frosted-glass backdrop for the given screen-space rect:
    // a blurred capture of the game window behind the menu + a dark tint + subtle sheen
    void draw(ImDrawList* draw_list, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, float alpha = 1.f);

    // returns the current glass tint (rgb + strength, each 0..1) driven by the
    // gui tint slider — used to tint secondary windows' chrome the same way.
    void tint_color(float* r, float* g, float* b, float* a);

    // draws a tinted glass header band across the top of a window: rounded top
    // corners (matches the window rounding), tinted by the same glass tint as
    // the main menu, with a subtle hairline divider under it.
    void draw_header(ImDrawList* draw_list, const ImVec2& wp, const ImVec2& ws, float title_h, float alpha = 1.f, float rounding = 8.f);

    // Rounds the corners of a glass rect: paints the 4 corner cutouts (the
    // quarter-circle sectors between the square rect and the rounded window
    // shape) with `col`, so the OS acrylic's square corners are hidden and the
    // frosted glass visually follows the rounded ui shape. Call this on the
    // background draw list with the same color used for the dark backdrop.
    void mask_corners(ImDrawList* draw_list, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, ImU32 col);
}
