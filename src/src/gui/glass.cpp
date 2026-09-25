// -----------------------------------------------------------------------------
// glass.cpp - frosted "liquid glass" for every overlay window.
//
// The old OS-compositor backdrop this file used to build (DirectComposition
// effect graph + DWM shared-thumbnail visuals of the windows behind us) is
// gone for good. Nothing in this build touches Windows blur, acrylic or DWM:
// the panels are rendered by the LiquidUI (liquidDX11) glass kit vendored
// under src/gui/LiquidUI:
//
//   * Glass::Backdrop - captures the desktop behind the overlay through DXGI
//                       output duplication and runs it through a down/up
//                       gaussian chain (soft + heavy pyramid).
//   * Glass::Renderer - draws every submitted "glass rect" with the liquid
//                       glass pixel shader: blur mix, tint, refraction,
//                       fresnel rim light, specular highlight, grain, ...
//                       straight into the overlay swap chain.
//
// gui::render() keeps talking to this file through the same little API it
// always did (new_frame -> add_rect()/draw() -> commit()); the GPU pass itself
// runs from Menu::Render() right before ImGui's draw data is submitted, via
// render_pass().
// -----------------------------------------------------------------------------
#include "pch.h"
#include "glass.h"
#include "liquid_ui.h"
#include "renderer/Renderer.h"
#include "app/Graphics.h"
#include "app/Settings.h"
#include "core/console/Console.h"
#include <Windows.h>
#include <cstdarg>
#include <cmath>
#include <vector>

namespace glass
{
    namespace
    {
        Glass::Renderer g_renderer;
        Glass::Backdrop g_backdrop;

        bool  g_renderer_ok = false;   // glass shaders/states are up
        bool  g_backdrop_ok = false;   // desktop duplication capture is up
        float g_capture_wait = 0.f;    // capture throttle accumulator

        // mirrors of the gui settings; pushed into the materials every frame
        float g_frost = 0.10f;
        float g_blur = 6.f;
        float g_tint[4] = { 0.106f, 0.106f, 0.114f, 1.f };

        // flat fallback backdrop: used when desktop duplication is unavailable
        // (exclusive fullscreen, another duplication client, ...) so the shader
        // still samples a texture instead of nothing - panels then render as
        // tinted glass without the blur.
        ID3D11ShaderResourceView* g_fallback = nullptr;

        struct Rect { float x, y, w, h, round, alpha; };
        std::vector<Rect> g_rects;     // this frame's panels, in window coords
        bool  g_clip_on = false;
        float g_clip[4] = { -1e6f, -1e6f, 1e6f, 1e6f };

        float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

        void GLog(const char* fmt, ...)
        {
            char buf[512];
            va_list ap; va_start(ap, fmt);
            _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
            va_end(ap);
            Cheat::Console::Log(Cheat::Console::Color::Gray, "[glass] %s", buf);
        }

        void make_fallback(ID3D11Device* device)
        {
            if (!device || g_fallback) return;
            // dark neutral grey, close to the panel tint, so glass without a
            // capture still reads as glass instead of black
            const float px[4] = { 0.10f, 0.10f, 0.12f, 1.f };
            D3D11_TEXTURE2D_DESC td{};
            td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_IMMUTABLE;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA sd{};
            sd.pSysMem = px;
            sd.SysMemPitch = 4 * sizeof(float);
            ID3D11Texture2D* tex = nullptr;
            if (SUCCEEDED(device->CreateTexture2D(&td, &sd, &tex)) && tex)
            {
                device->CreateShaderResourceView(tex, nullptr, &g_fallback);
                tex->Release();
            }
        }

        // The LiquidUI widget kit draws its icons from Font Awesome (the solid
        // + brands faces ship next to the kit). Load them from the vendored
        // data folder when we can find it; without it DrawIcon falls back to
        // its built-in vector icons.
        bool file_exists(const std::string& p)
        {
            const DWORD a = GetFileAttributesA(p.c_str());
            return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
        }

        std::string find_asset(const char* rel)
        {
            char exe[MAX_PATH] = {};
            const DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
            if (!n || n >= MAX_PATH) return std::string();
            std::string dir(exe, n);
            const size_t slash = dir.find_last_of("\\/");
            if (slash == std::string::npos) return std::string();
            dir.erase(slash);
            for (int i = 0; i <= 6; ++i)
            {
                std::string cand = dir + "\\" + rel;
                if (file_exists(cand)) return cand;
                dir += "\\..";
            }
            return std::string();
        }

        void load_icon_fonts()
        {
            static const char* kSolid[] = {
                "src\\gui\\LiquidUI\\LiquidUI\\data\\fa\\fa-solid-900.ttf",
                "gui\\LiquidUI\\LiquidUI\\data\\fa\\fa-solid-900.ttf",
                "LiquidUI\\LiquidUI\\data\\fa\\fa-solid-900.ttf",
                "data\\fa\\fa-solid-900.ttf",
            };
            static const char* kBrands[] = {
                "src\\gui\\LiquidUI\\LiquidUI\\data\\fa\\fa-brands-400.ttf",
                "gui\\LiquidUI\\LiquidUI\\data\\fa\\fa-brands-400.ttf",
                "LiquidUI\\LiquidUI\\data\\fa\\fa-brands-400.ttf",
                "data\\fa\\fa-brands-400.ttf",
            };

            ImGuiIO& io = ImGui::GetIO();
            std::string path;
            for (const char* rel : kSolid)
            {
                path = find_asset(rel);
                if (!path.empty()) break;
            }
            if (path.empty()) { GLog("icon font not found - vector icons only"); return; }

            if (ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), 32.0f))
                Glass::SetIconFont(f);
            for (const char* rel : kBrands)
            {
                path = find_asset(rel);
                if (!path.empty()) break;
            }
            if (!path.empty())
                if (ImFont* b = io.Fonts->AddFontFromFileTTF(path.c_str(), 32.0f))
                    Glass::SetIconFontBrands(b);
        }

        // pushes the gui glass sliders (frost / blur / tint) into the LiquidUI
        // material model:
        //   blur  -> how much of the captured desktop blur is mixed in
        //   frost -> tint opacity ("milkiness") + how strong the rim refracts
        //   tint  -> panel tint colour, its alpha scales the tint opacity
        void apply_settings()
        {
            if (!Glass::g) return;
            auto& gui = Cheat::g_Settings.gui;

            const float blur  = clampf(g_blur / 100.f, 0.f, 1.f);
            const float frost = clampf(g_frost, 0.f, 1.f);
            const float ta    = clampf(g_tint[3], 0.f, 1.f);

            // profile menu switches: "Glass" off drops the material to a flat
            // dark sheet with no blur at all, "Blur" off keeps the desktop
            // sharp behind the tint
            const bool glass_on = gui.glass_on;
            const bool blur_on  = gui.blur > 0.01f;

            const float blur_mix = (glass_on && blur_on)
                                 ? clampf(std::sqrt(blur) * 1.15f, 0.05f, 1.f) : 0.f;
            const float opacity  = clampf((0.25f + 0.55f * frost) * ta * (glass_on ? 1.f : 1.35f),
                                          0.f, 1.f);

            // the "Customize" tab owns these live knobs; every default is zero
            // because the panels are a flat 2D sheet - blur + tint only, no
            // refraction, lighting, specular or rim
            Glass::SetGlobalMaterial(blur_mix, opacity,
                                     clampf(gui.glass_sat, 0.3f, 3.f),
                                     glass_on ? clampf(gui.glass_refr, 0.f, 60.f) : 0.f,
                                     glass_on ? clampf(gui.glass_chroma, 0.f, 8.f) : 0.f,
                                     clampf(gui.glass_edge, 0.f, 1.f),
                                     clampf(gui.glass_shadow, 0.f, 1.f));
            Glass::SetAccent(gui.accent[0], gui.accent[1], gui.accent[2]);
            Glass::SetAppearance(gui.appearance);
            Glass::SetLiquidFlow(clampf(gui.glass_flow, 0.f, 10.f));
            Glass::g->SetLights(gui.light1_angle, gui.light1_amt,
                                gui.light2_angle, gui.light2_amt);

            Glass::GlassEdgeConfig ec;      // edge knobs, Customize-tab driven
            ec.fresnel_exp  = clampf(gui.fresnel, 1.f, 6.f);
            ec.bevel_scale  = clampf(gui.bevel, 0.f, 2.f);
            ec.specular_amt = clampf(gui.specular_amt, 0.f, 3.f);
            ec.sheen_amt    = clampf(gui.sheen, 0.f, 3.f);
            ec.grain_amt    = clampf(gui.grain, 0.f, 4.f);
            ec.ambient_rim  = clampf(gui.ambient_rim, 0.f, 2.f);
            ec.panel_rim_angle = 135.f;
            // flat 2D: the only edge left is a hairline "lip". There is no
            // bevel lighting, no depth shadow and no cross-panel light bleed -
            // `lip` is not scaled by `highlight`, so it must be zeroed/set here.
            ec.lip          = glass_on ? 0.06f : 0.f;
            ec.light_height = 0.f;
            ec.edge_shadow  = 0.f;
            Glass::g->SetEdgeConfig(ec);

            // the Settings-tab tint swatch drives the panel wash. SetGlobal
            // Material only pushes blur/opacity, so the colour is written here
            // - after SetAppearance, which restores the material defaults.
            const float tr = clampf(g_tint[0], 0.f, 1.f);
            const float tg = clampf(g_tint[1], 0.f, 1.f);
            const float tb = clampf(g_tint[2], 0.f, 1.f);
            const Glass::Material tinted[3] = {
                Glass::Material::Thin, Glass::Material::Regular, Glass::Material::Thick
            };
            for (Glass::Material m : tinted)
            {
                Glass::MaterialParams& mp = Glass::EditParams(m);
                mp.tint_rgb[0] = tr;
                mp.tint_rgb[1] = tg;
                mp.tint_rgb[2] = tb;
            }

            // ---- flat, dark panel tuning ------------------------------------
            // The kit's stock panel materials carry a lot of inner shadow,
            // border rim and sheen - that is the "3D" look. These pin the
            // three panel materials to the flat reference look and drop the
            // sampled backdrop slightly so the glass reads dark instead of
            // washed out. Written after SetAppearance(), which restores the
            // per-material defaults, so this always wins; the Customize tab
            // still owns the effect knobs on top of it.
            for (Glass::Material m : tinted)
            {
                Glass::MaterialParams& mp = Glass::EditParams(m);
                mp.inner_shadow     = 0.f;
                mp.border_intensity = 0.f;
                mp.border_width     = 0.8f;
                mp.sheen            = 0.f;
                mp.grain            = 0.f;
                mp.brightness       = -0.10f;
            }
        }

        // one frosted panel for the given rect. `alpha` is the panel's fade (the
        // window open/close animation), `mat` its LiquidUI material.
        void submit(float x, float y, float w, float h, float rounding, float alpha,
                    Glass::Material mat)
        {
            if (!g_renderer_ok || alpha <= 0.001f || w <= 1.f || h <= 1.f) return;
            Glass::Primitive p{};   // value-init: unset fields (elevate, blobs) feed the shader
            p.cx = x + w * 0.5f;
            p.cy = y + h * 0.5f;
            p.hw = w * 0.5f;
            p.hh = h * 0.5f;
            p.corner_radius = clampf(rounding, 0.f, std::min(w, h) * 0.5f);
            p.fade = alpha;
            p.material = mat;
            g_renderer.Submit(p);
        }

        // true when this exact rect is already queued (windows register their
        // panel with add_rect() and then call draw() with a 1px-inset copy of
        // the same rect - it must not be queued twice, that would double-blend
        // the glass). Popups sitting *inside* another window's rect are not
        // filtered by this: their rect differs far too much.
        bool already_submitted(float x, float y, float w, float h)
        {
            const float eps = 3.f;
            for (const Rect& r : g_rects)
                if (std::fabs(r.x - x) < eps && std::fabs(r.y - y) < eps &&
                    std::fabs(r.w - w) < eps && std::fabs(r.h - h) < eps)
                    return true;
            return false;
        }

        void client_size(int& w, int& h, int& screen_x, int& screen_y)
        {
            w = h = screen_x = screen_y = 0;
            HWND overlay = Cheat::Renderer::GetHwnd();
            if (!overlay) return;
            RECT cr{};
            if (GetClientRect(overlay, &cr))
            {
                w = cr.right - cr.left;
                h = cr.bottom - cr.top;
            }
            RECT wr{};
            if (GetWindowRect(overlay, &wr))
            {
                screen_x = wr.left;
                screen_y = wr.top;
            }
        }
    }

    bool ready() { return g_renderer_ok; }

    void init(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (g_renderer_ok || !device || !context) return;

        if (!g_renderer.Init(device, context))
        {
            GLog("LiquidUI glass renderer init failed (shader compile?)");
            return;
        }

        // the widget kit (BeginCard/SidebarNav/...) submits through this
        Glass::g = &g_renderer;
        Glass::SetAppearance(0);          // dark ink
        Glass::SetDensity(1.0f);
        Glass::SetWidgetScale(1.0f);
        Glass::Set24Hour(false);
        Glass::SetLiquidFlow(0.f);
        Glass::SetSfxEnabled(false);      // no ui clicks over the game audio
        g_renderer_ok = true;

        load_icon_fonts();                // Font Awesome for the widget icons

        // desktop capture behind the overlay (the panels blur this)
        HWND overlay = Cheat::Renderer::GetHwnd();
        g_backdrop_ok = overlay ? g_backdrop.Init(device, context, overlay) : false;
        if (!g_backdrop_ok) make_fallback(device);

        apply_settings();
        GLog("LiquidUI glass active (%s)", g_backdrop_ok ? "desktop capture" : "capture unavailable");
    }

    void shutdown()
    {
        if (!g_renderer_ok) return;
        Glass::g = nullptr;
        g_backdrop.Shutdown();
        g_renderer.Shutdown();
        if (g_fallback) { g_fallback->Release(); g_fallback = nullptr; }
        g_rects.clear();
        g_renderer_ok = false;
        g_backdrop_ok = false;
    }

    void invalidate()
    {
        // DX11 device objects survive a resize (only our own swap chain does
        // not), so there is nothing to rebuild here. Kept for Menu::Invalidate
        // DeviceObjects().
    }

    void set_frost(float f)
    {
        g_frost = clampf(f, 0.f, 1.f);
    }

    void set_blur(float f)
    {
        g_blur = clampf(f, 0.f, 100.f);
    }

    void set_tint(float r, float g, float b, float a)
    {
        g_tint[0] = clampf(r, 0.f, 1.f);
        g_tint[1] = clampf(g, 0.f, 1.f);
        g_tint[2] = clampf(b, 0.f, 1.f);
        g_tint[3] = clampf(a, 0.f, 1.f);
    }

    void tint_color(float* r, float* g, float* b, float* a)
    {
        if (r) *r = g_tint[0];
        if (g) *g = g_tint[1];
        if (b) *b = g_tint[2];
        if (a) *a = g_tint[3];
    }

    void set_menu_rect(float x, float y, float w, float h)
    {
        // legacy single-window entry point (Renderer.cpp hides the backdrop
        // with a zero rect when the game is gone)
        if (w > 1.f && h > 1.f) add_rect(x, y, w, h, 8.f);
    }

    void new_frame()
    {
        g_rects.clear();
        g_clip_on = false;
        g_clip[0] = g_clip[1] = -1e6f;
        g_clip[2] = g_clip[3] = 1e6f;
        if (!g_renderer_ok) return;

        ImGuiIO& io = ImGui::GetIO();

        // The kit integrates every spring (the glass entrance fade included)
        // with explicit Euler steps and no dt cap, so a large frame delta - the
        // startup hitch, alt-tab, a breakpoint - would blow the fade up to
        // inf/NaN within a few frames and STICK there (NaN never recovers).
        // NaN in the swap chain's alpha channel then takes the whole layered
        // window's composition down with it: nothing renders at all. Clamp dt
        // like the reference app does (<= 0 -> 60 Hz, spikes -> 30 Hz).
        if (!(io.DeltaTime > 0.f))
            io.DeltaTime = 1.f / 60.f;
        else if (io.DeltaTime > 0.05f)
            io.DeltaTime = 1.f / 30.f;

        const float dt = io.DeltaTime;

        // refresh the desktop capture behind the panels - the Customize tab's
        // "glass refresh (Hz)" throttles it (the blur chain is the expensive
        // part, not the capture itself)
        const float hz = (float)(Cheat::g_Settings.gui.blur_hz < 1 ? 1
                                 : (Cheat::g_Settings.gui.blur_hz > 240 ? 240
                                    : Cheat::g_Settings.gui.blur_hz));
        g_capture_wait += dt;
        if (g_backdrop_ok && g_capture_wait >= 1.f / hz)
        {
            g_capture_wait = 0.f;
            g_backdrop.Capture();
            // the blur chain finishes with no render target bound; everything
            // that draws after us in this frame (crosshair, lua draw, the
            // glass pass) expects the overlay target to still be bound
            ID3D11RenderTargetView* rtv = Cheat::Core::g_RenderTargetView;
            if (rtv && Cheat::Core::g_DeviceContext)
                Cheat::Core::g_DeviceContext->OMSetRenderTargets(1, &rtv, nullptr);
        }

        int w = 0, h = 0, wx = 0, wy = 0;
        client_size(w, h, wx, wy);
        if (w <= 0 || h <= 0) return;

        // the shader samples the captured desktop in *desktop* space, so it
        // needs where our window sits inside the capture
        const int desk_w = g_backdrop_ok ? g_backdrop.width() : w;
        const int desk_h = g_backdrop_ok ? g_backdrop.height() : h;
        const int org_x  = g_backdrop_ok ? (wx - g_backdrop.originX()) : 0;
        const int org_y  = g_backdrop_ok ? (wy - g_backdrop.originY()) : 0;

        g_renderer.BeginFrame(w, h, org_x, org_y, desk_w, desk_h, io.MousePos);
        apply_settings();
    }

    void add_rect(float x, float y, float w, float h, float rounding, float alpha)
    {
        if (w <= 1.f || h <= 1.f) return;
        Rect r{ x, y, w, h, rounding, alpha };
        g_rects.push_back(r);
        if (!g_renderer_ok || !g_clip_on) { submit(x, y, w, h, rounding, alpha, Glass::Material::Regular); return; }
        g_renderer.SetClipRect(g_clip[0], g_clip[1], g_clip[2], g_clip[3]);
        submit(x, y, w, h, rounding, alpha, Glass::Material::Regular);
        g_renderer.ClearClipRect();
    }

    void add_hole(float x, float y, float w, float h, float rounding)
    {
        if (w <= 1.f || h <= 1.f) return;
        g_rects.push_back(Rect{ x, y, w, h, rounding, 1.f });
    }

    void set_clip(float x0, float y0, float x1, float y1)
    {
        g_clip_on = true;
        g_clip[0] = x0; g_clip[1] = y0; g_clip[2] = x1; g_clip[3] = y1;
    }

    void clear_clip()
    {
        g_clip_on = false;
        g_clip[0] = g_clip[1] = -1e6f;
        g_clip[2] = g_clip[3] = 1e6f;
    }

    void commit()
    {
        // nothing deferred: the panels are queued in the renderer and drawn by
        // render_pass() once ImGui has produced its draw data
    }

    int rect_count()
    {
        return (int)g_rects.size();
    }

    bool rect_at(int i, float& x, float& y, float& w, float& h, float* rounding)
    {
        if (i < 0 || i >= (int)g_rects.size()) return false;
        const Rect& r = g_rects[(size_t)i];
        x = r.x; y = r.y; w = r.w; h = r.h;
        if (rounding) *rounding = r.round;
        return true;
    }

    void render_pass()
    {
        if (!g_renderer_ok) return;

        ID3D11DeviceContext* ctx = Cheat::Core::g_DeviceContext;
        ID3D11RenderTargetView* rtv = Cheat::Core::g_RenderTargetView;
        if (!ctx || !rtv) return;

        int w = 0, h = 0, wx = 0, wy = 0;
        client_size(w, h, wx, wy);
        if (w <= 0 || h <= 0) return;

        ID3D11ShaderResourceView* heavy = g_backdrop_ok ? g_backdrop.heavySRV() : g_fallback;
        ID3D11ShaderResourceView* soft  = g_backdrop_ok ? g_backdrop.softSRV()  : g_fallback;
        if (!heavy || !soft) return;

        // the overlay owns the target; restore it plus the viewport (ImGui sets
        // its own on top of us right after)
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT vp{};
        vp.Width = (float)w; vp.Height = (float)h; vp.MaxDepth = 1.f;
        ctx->RSSetViewports(1, &vp);

        g_renderer.Render(heavy, soft);

        static bool s_logged_pass = false;
        if (!s_logged_pass)
        {
            s_logged_pass = true;
            GLog("glass pass running (%d panel%s this frame)", rect_count(), rect_count() == 1 ? "" : "s");
        }
    }

    // draws a frosted panel for the given rect. Windows that already registered
    // it with add_rect() are skipped (the panel is queued once); ImGui-only
    // backdrops that never call add_rect() - dropdown popups, for instance -
    // get their panel from here.
    void draw(ImDrawList* draw_list, const ImVec2& rect_min, const ImVec2& rect_max,
              float rounding, float alpha)
    {
        (void)draw_list;
        if (!g_renderer_ok) return;

        const float x = rect_min.x, y = rect_min.y;
        const float w = rect_max.x - rect_min.x;
        const float h = rect_max.y - rect_min.y;
        if (w <= 2.f || h <= 2.f) return;
        if (already_submitted(x, y, w, h)) return;

        g_rects.push_back(Rect{ x, y, w, h, rounding, alpha });
        submit(x, y, w, h, rounding, alpha, Glass::Material::Regular);
    }

    // subtle tinted band across the top of a window so titles keep reading as a
    // header over the glass (rounded top corners matching the window)
    void draw_header(ImDrawList* draw_list, const ImVec2& wp, const ImVec2& ws,
                     float title_h, float alpha, float rounding)
    {
        if (!draw_list) return;
        if (ws.x <= 2.f || ws.y <= 2.f || title_h <= 0.f) return;

        const ImU32 fill = IM_COL32(
            (int)(g_tint[0] * 255.f), (int)(g_tint[1] * 255.f), (int)(g_tint[2] * 255.f),
            (ImU32)(96.f * alpha));
        const ImVec2 min(wp.x + 1.f, wp.y + 1.f);
        const ImVec2 max(wp.x + ws.x - 1.f, wp.y + title_h);
        if (max.x <= min.x || max.y <= min.y) return;

        draw_list->AddRectFilled(min, max, fill, rounding, ImDrawFlags_RoundCornersTop);
        draw_list->AddRectFilled(ImVec2(min.x, max.y), ImVec2(max.x, max.y + 1.f),
            IM_COL32(235, 240, 237, (ImU32)(46.f * alpha)));
    }

    // The dark menu backdrop carves square holes around the glass windows; the
    // glass itself is rounded, so this paints the corner sectors between the
    // square hole and the rounded panel with the backdrop colour to keep the
    // shape genuinely rounded.
    void mask_corners(ImDrawList* draw_list, const ImVec2& rect_min, const ImVec2& rect_max,
                      float rounding, ImU32 col)
    {
        if (!draw_list) return;
        const float w = rect_max.x - rect_min.x;
        const float h = rect_max.y - rect_min.y;
        if (w <= 2.f || h <= 2.f) return;
        if (rounding <= 0.5f) return;
        const float r = std::min(rounding, std::min(w, h) * 0.5f);

        const ImVec2 corner[4] = {
            { rect_min.x, rect_min.y }, { rect_max.x, rect_min.y },
            { rect_max.x, rect_max.y }, { rect_min.x, rect_max.y }
        };
        const ImVec2 center[4] = {
            { rect_min.x + r, rect_min.y + r }, { rect_max.x - r, rect_min.y + r },
            { rect_max.x - r, rect_max.y - r }, { rect_min.x + r, rect_max.y - r }
        };
        const float a0[4] = { 180.f, 270.f, 0.f, 90.f };
        const float a1[4] = { 270.f, 360.f, 90.f, 180.f };

        constexpr int seg = 10;
        constexpr float kDeg = 3.14159265f / 180.f;
        for (int k = 0; k < 4; ++k)
        {
            ImVec2 prev = corner[k];
            for (int i = 0; i <= seg; ++i)
            {
                const float a = (a0[k] + (a1[k] - a0[k]) * (float)i / (float)seg) * kDeg;
                const ImVec2 p(center[k].x + std::cos(a) * r, center[k].y + std::sin(a) * r);
                if (i > 0)
                    draw_list->AddTriangleFilled(prev, p, corner[k], col);
                prev = p;
            }
        }
    }
}
