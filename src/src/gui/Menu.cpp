#include "pch.h"
#include "Menu.h"
#include "gui.h"
#include "icons.h"
#include "glass.h"
#include "resources/fonts/fonts.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "app/Settings.h"
#include "app/Graphics.h"
#include "renderer/Renderer.h"
#include "features/visuals/ESP.h"
#include "features/visuals/MeshDxShader.h"
#include "features/visuals/KillEffects.h"
#include "features/visuals/Crosshair.h"
#include "features/visuals/EngineChams.h"
#include "features/aim/Aim.h"
#include "features/aim/Triggerbot.h"
#include "features/aim/RaycastSilent.h"
#include "features/aim/MagicBullet.h"
#include "features/aim/ViewportSilent.h"
#include "features/lua/LuaExecutor.h"
#include "features/lua/vm/LuaGc.h"
#include "features/local/Btools.h"
#include "features/lua/vm/LuaVM.h"
#include "features/lua/vm/LuaDrawing.h"
#include "features/lua/vm/CallGate.h"
#include "features/misc/Misc.h"
#include "features/misc/HitboxExpander.h"
#include "features/misc/PlayerAvatars.h"
#include "features/explorer/Explorer.h"
#include "features/mcp/McpBridge.h"
#include "core/console/Console.h"
#include <Windows.h>
#include <cstdio>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Cheat {
namespace GUI {

bool Menu::Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.IniFilename = nullptr;
    io.ConfigWindowsResizeFromEdges = true;

    fonts::load(io);
    io.FontDefault = fonts::by_index(g_Settings.gui.font);
    gui::setup_style();
    gui::icons_init(pDevice);
    glass::init(pDevice, pDeviceContext);

    if (!ImGui_ImplWin32_Init(hWnd))
        return false;
    if (!ImGui_ImplDX11_Init(pDevice, pDeviceContext))
        return false;

    m_bInitialized = true;

    Cheat::Features::Explorer::Initialize();
    Cheat::Features::Misc::Start();
    Cheat::Visuals::EngineChams::Start();
    if (Cheat::g_Settings.misc.mcp)
        Cheat::Features::McpBridge::Start();

    return true;
}

void Menu::Render()
{
    if (!m_bInitialized) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    {
        ImFont* ui_f = fonts::by_index(g_Settings.gui.font);
        ImGui::PushFont(ui_f);

        if (Renderer::IsGameActive()) {
            Visuals::ESP::Render();
            if (Cheat::Core::g_RenderTargetView)
                Cheat::Visuals::MeshDxShader::Flush(Cheat::Core::g_RenderTargetView);
            Features::HitboxExpander::Render();
            Features::Aim::Render();
            Features::Triggerbot::Render();
            Visuals::KillEffects::Tick();
        }
        if (g_Settings.lua.executor)
            Features::LuaExecutor::Initialize();
        if (Features::LuaVM::Ready())
            Features::LuaDrawing::Render();
        DrawMenu();
        Features::PlayerAvatars::Tick();
        if (g_Settings.misc.mcp && !Features::McpBridge::Running())
            Features::McpBridge::Start();
        else if (!g_Settings.misc.mcp && Features::McpBridge::Running())
            Features::McpBridge::Stop();
        Visuals::Crosshair::Render();

        ImGui::PopFont();
    }
    ImGui::Render();

    // LiquidUI glass pass: draws every window's frosted panel (blurred desktop
    // capture + liquid glass shader) into the overlay target, then ImGui paints
    // the ui on top of it.
    glass::render_pass();

    // ---- diagnostic: probe the backbuffer after the full frame is drawn ----
    {
        static bool s_probed = false;
        if (!s_probed && ImGui::GetFrameCount() > 60)
        {
            s_probed = true;
            ID3D11Device* dev = Cheat::Core::g_Device;
            ID3D11DeviceContext* ctx = Cheat::Core::g_DeviceContext;
            IDXGISwapChain* sc = Cheat::Core::g_SwapChain;
            if (dev && ctx && sc)
            {
                ID3D11Texture2D* back = nullptr;
                if (SUCCEEDED(sc->GetBuffer(0, IID_PPV_ARGS(&back))) && back)
                {
                    D3D11_TEXTURE2D_DESC bd{};
                    back->GetDesc(&bd);
                    // copy 64x64 at the menu's position + the screen center into
                    // a staging texture and read the real pixels back
                    const int px = std::min((int)gui::menu_pos().x + 60, (int)bd.Width - 65);
                    const int py = std::min((int)gui::menu_pos().y + 60, (int)bd.Height - 65);
                    D3D11_TEXTURE2D_DESC sd = bd;
                    sd.Width = 64; sd.Height = 64;
                    sd.MipLevels = 1; sd.ArraySize = 1;
                    sd.SampleDesc.Count = 1;
                    sd.Usage = D3D11_USAGE_STAGING;
                    sd.BindFlags = 0;
                    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                    sd.MiscFlags = 0;
                    ID3D11Texture2D* stage = nullptr;
                    if (SUCCEEDED(dev->CreateTexture2D(&sd, nullptr, &stage)) && stage)
                    {
                        D3D11_BOX box{};
                        box.left = px; box.top = py;
                        box.right = px + 64; box.bottom = py + 64;
                        box.back = 1;
                        ctx->CopySubresourceRegion(stage, 0, 0, 0, 0, back, 0, &box);
                        D3D11_MAPPED_SUBRESOURCE map{};
                        if (SUCCEEDED(ctx->Map(stage, 0, D3D11_MAP_READ, 0, &map)))
                        {
                            const uint8_t* p = (const uint8_t*)map.pData;
                            long long sum = 0, a = 0, nz = 0;
                            for (int y = 0; y < 64; ++y)
                            {
                                const uint8_t* row = p + (size_t)y * map.RowPitch;
                                for (int x = 0; x < 64; ++x)
                                {
                                    const uint8_t b0 = row[x * 4 + 0], g0 = row[x * 4 + 1];
                                    const uint8_t r0 = row[x * 4 + 2], a0 = row[x * 4 + 3];
                                    sum += b0 + g0 + r0;
                                    a += a0;
                                    if (b0 | g0 | r0 | a0) ++nz;
                                    (void)b0;
                                }
                            }
                            Cheat::Console::Log(Cheat::Console::Color::Gray,
                                "[probe] backbuffer %ux%u | menu area avg=BGRA sum=%lld alpha_sum=%lld nonzero=%lld/4096",
                                bd.Width, bd.Height, sum, a, nz);
                            ctx->Unmap(stage, 0);
                        }
                        else
                            Cheat::Console::Log(Cheat::Console::Color::Red, "[probe] Map failed");
                        stage->Release();
                    }
                    back->Release();
                }
                ID3D11RenderTargetView* crt = nullptr;
                ctx->OMGetRenderTargets(1, &crt, nullptr);
                RECT wr{};
                GetWindowRect(Cheat::Renderer::GetHwnd(), &wr);
                const ImVec2 mp = gui::menu_pos();
                Cheat::Console::Log(Cheat::Console::Color::Gray,
                    "[probe] drawdata vtx=%d idx=%d | rtv=%p | menu at %.0f,%.0f | overlay rect %ld,%ld %ldx%ld",
                    ImGui::GetDrawData()->TotalIdxCount, ImGui::GetDrawData()->TotalVtxCount,
                    (void*)crt, mp.x, mp.y,
                    wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top);
                if (crt) crt->Release();
            }
        }
    }

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool Menu::IsVisible()
{
    return m_bMenuVisible;
}

bool Menu::IsPointOverUI(float x, float y)
{
    if (!m_bInitialized)
        return false;

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx || ctx->Windows.Size <= 0)
        return false;

    const ImVec2 p(x, y);
    for (int i = ctx->Windows.Size - 1; i >= 0; --i) {
        ImGuiWindow* w = ctx->Windows[i];
        if (!w || !w->Active || w->Hidden)
            continue;
        if (w->IsFallbackWindow)
            continue;
        if (w->Flags & ImGuiWindowFlags_Tooltip)
            continue;

        if (!(w->Flags & ImGuiWindowFlags_NoInputs)) {
            if (w->Rect().Contains(p))
                return true;
        }
    }
    return false;
}

bool Menu::ShouldCaptureMouse(float x, float y)
{
    if (!m_bInitialized) return false;

    if (m_bMenuVisible)
        return true;

    return gui::point_over_ui(x, y) || IsPointOverUI(x, y);
}

float Menu::DrawMenu()
{
    const bool was_open = gui::any_ui_open();
    gui::render();
    const bool open = gui::any_ui_open();

    if (open != m_bMenuVisible)
    {
        m_bMenuVisible = open;
        Renderer::SetClickThrough(!m_bMenuVisible);

        if (m_bMenuVisible)
        {
            ClipCursor(nullptr);
            if (GetCapture())
                ReleaseCapture();

            if (!g_Settings.crosshair.enabled)
            {
                while (ShowCursor(TRUE) < 0) {}
            }
        }
        else
        {
            Renderer::SetTextInputFocus(false);

            HWND game = Renderer::GetGameHwnd();
            if (game && IsWindow(game))
            {
                RECT cr{};
                if (GetClientRect(game, &cr))
                {
                    POINT tl{ cr.left, cr.top };
                    POINT br{ cr.right, cr.bottom };
                    ClientToScreen(game, &tl);
                    ClientToScreen(game, &br);
                    RECT clip{ tl.x, tl.y, br.x, br.y };
                    ClipCursor(&clip);
                }

                SetForegroundWindow(game);
            }

            for (int i = 0; i < 8; ++i)
            {
                if (ShowCursor(FALSE) < 0)
                    break;
            }
        }
    }
    else if (open != was_open)
    {
        m_bMenuVisible = open;
        Renderer::SetClickThrough(!m_bMenuVisible);
    }

    // Music player: while the menu is closed, the card becomes interactive
    // when the physical cursor is over it. Elsewhere the overlay stays
    // click-through so the game keeps full mouse control.
    static bool s_cardHover = false;
    if (!m_bMenuVisible && gui::music_visible())
    {
        bool hover = false;
        HWND overlay = Renderer::GetHwnd();
        POINT p{};
        if (overlay && GetCursorPos(&p) && ScreenToClient(overlay, &p))
            hover = gui::point_over_ui((float)p.x, (float)p.y);

        if (hover != s_cardHover)
        {
            s_cardHover = hover;
            Renderer::SetClickThrough(!hover);

            if (hover)
            {
                ClipCursor(nullptr);
                while (ShowCursor(TRUE) < 0) {}
            }
            else
            {
                HWND game = Renderer::GetGameHwnd();
                if (game && IsWindow(game))
                {
                    RECT cr{};
                    if (GetClientRect(game, &cr))
                    {
                        POINT tl{ cr.left, cr.top };
                        POINT br{ cr.right, cr.bottom };
                        ClientToScreen(game, &tl);
                        ClientToScreen(game, &br);
                        RECT clip{ tl.x, tl.y, br.x, br.y };
                        ClipCursor(&clip);
                    }
                }

                for (int i = 0; i < 8; ++i)
                {
                    if (ShowCursor(FALSE) < 0)
                        break;
                }
            }
        }
    }
    else if (m_bMenuVisible && s_cardHover)
    {
        s_cardHover = false;   // menu opened: the menu path owns the window state
    }

    m_menuAlpha = open ? 1.f : 0.f;
    return 1.f;
}

void Menu::Shutdown()
{
    if (!m_bInitialized)
        return;

    Cheat::Features::Misc::Stop();
    Cheat::Visuals::EngineChams::Stop();

    Cheat::Features::RaycastSilent::Remove();
    Cheat::Features::MagicBullet::Remove();
    Cheat::Features::CallGate::Remove();
    Cheat::Features::ViewportSilent::Shutdown();

    Cheat::Visuals::Crosshair::Shutdown();
    Cheat::Features::Explorer::Shutdown();
    Cheat::Features::McpBridge::Stop();
    Cheat::Features::LuaExecutor::Shutdown();
    Cheat::Features::LuaGc::Stop();
    Cheat::Features::Btools::Shutdown();

    glass::shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_bInitialized = false;
}

void Menu::InvalidateDeviceObjects()
{
    if (!m_bInitialized) return;

    glass::invalidate();
    ImGui_ImplDX11_InvalidateDeviceObjects();
}

void Menu::CreateDeviceObjects()
{
    if (!m_bInitialized) return;
    ImGui_ImplDX11_CreateDeviceObjects();
}

bool Menu::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!m_bInitialized)
        return false;
    return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
}

}
}
