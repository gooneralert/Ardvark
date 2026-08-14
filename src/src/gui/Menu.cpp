#include "pch.h"
#include "Menu.h"
#include "gui.h"
#include "icons.h"
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
#include <Windows.h>

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

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_bInitialized = false;
}

void Menu::InvalidateDeviceObjects()
{
    if (!m_bInitialized) return;
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
