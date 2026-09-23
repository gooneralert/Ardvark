#pragma once

#include "imgui.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace glass
{
    // true once the LiquidUI glass renderer is up (shaders compiled). When it is
    // false the UI still runs, it just has no frosted panels.
    bool ready();

    void init(ID3D11Device* device, ID3D11DeviceContext* context);
    void shutdown();
    void invalidate();

    // frost strength 0..1 — 0 = clear glass, 1 = heavy milky frost.
    // drives the tint opacity + refraction of the LiquidUI glass material
    void set_frost(float f);

    // blur strength 0..100 — how much of the blurred desktop capture is mixed in
    void set_blur(float f);

    // glass tint color (rgb 0..1) + tint strength (a 0..1).
    // rgb is the panel tint, a scales how much of it is applied.
    void set_tint(float r, float g, float b, float a);

    // legacy single-window entry point (Renderer.cpp hides the panel with a
    // zero rect when the game is gone); internally uses add_rect
    void set_menu_rect(float x, float y, float w, float h);

    // --- multi-window glass support -----------------------------------------
    // call new_frame() once at the start of the ui frame, add_rect() for every
    // window that should get a frosted glass panel (menu, lua, players,
    // explorer, ...), and commit() once at the end of the frame. The panels
    // themselves are drawn by render_pass() (see below).
    void new_frame();
    void add_rect(float x, float y, float w, float h, float rounding = 8.f, float alpha = 1.f);
    void commit();

    // records the rect for the dark menu backdrop's hole carving WITHOUT
    // submitting a panel - for windows that already draw their own panel (the
    // main menu card draw itself through the LiquidUI widget kit).
    void add_hole(float x, float y, float w, float h, float rounding = 8.f);

    // optional clip for the panels submitted from here on (window coords, same
    // space as add_rect). Used by windows that slide in from behind another
    // window so their panel can be cut off at the other window's edge.
    void set_clip(float x0, float y0, float x1, float y1);
    void clear_clip();

    // runs the LiquidUI glass pass for this frame's collected rects. Has to be
    // called after ImGui::Render() and before
    // ImGui_ImplDX11_RenderDrawData(), while the overlay render target is
    // still bound (Menu::Render does that).
    void render_pass();

    // --- introspection ------------------------------------------------------
    // this frame's collected glass rects (window coords, in the same space as
    // the ImGui background draw list). Used by the dark menu backdrop to carve
    // holes around the frosted windows so their blur stays see-through.
    // Valid between new_frame() and the next new_frame().
    int  rect_count();
    bool rect_at(int i, float& x, float& y, float& w, float& h, float* rounding = nullptr);

    // queues a LiquidUI glass panel for the given screen-space rect. Windows
    // that already registered the rect with add_rect() are skipped; ImGui-only
    // surfaces that never do (combo dropdowns, for instance) get their panel
    // from here.
    void draw(ImDrawList* draw_list, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, float alpha = 1.f);

    // returns the current glass tint (rgb + strength, each 0..1) driven by the
    // gui tint slider — used to tint secondary windows' chrome the same way.
    void tint_color(float* r, float* g, float* b, float* a);

    // draws a tinted glass header band across the top of a window: rounded top
    // corners (matches the window rounding), tinted by the same glass tint as
    // the main menu, with a subtle hairline divider under it.
    void draw_header(ImDrawList* draw_list, const ImVec2& wp, const ImVec2& ws, float title_h, float alpha = 1.f, float rounding = 8.f);

    // --- glass rects / corners ------------------------------------------------
    // Rounds the corners of a glass rect: paints the 4 corner cutouts (the
    // quarter-circle sectors between the square rect and the rounded window
    // shape) with `col`, so the frosted panel's rounded corners stay clear of
    // the dark backdrop. Call this on the background draw list with the same
    // color used for the dark backdrop.
    void mask_corners(ImDrawList* draw_list, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, ImU32 col);
}
