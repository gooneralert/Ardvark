#pragma once

struct ImDrawList;
struct ImVec2;
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

    // draws a frosted-glass backdrop for the given screen-space rect:
    // a blurred capture of the game window behind the menu + a dark tint + subtle sheen
    void draw(ImDrawList* draw_list, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, float alpha = 1.f);
}
