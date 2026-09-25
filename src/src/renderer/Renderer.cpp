#include "pch.h"
#include "Renderer.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include "gui/Menu.h"
#include "gui/glass.h"
#include "imgui.h"
#include "app/Graphics.h"
#include "app/Settings.h"
#include "core/memory/Memory.h"
#include "core/console/Console.h"
#include "features/visuals/Crosshair.h"
#include "features/visuals/boxfill/BoxFill.h"
#include "features/visuals/MeshDxShader.h"
#include <windowsx.h>
// NOTE: only for the per-pixel transparency plumbing below (DWM honoring the
// client-area alpha). This does NOT request blur or acrylic - that would be
// DwmEnableBlurBehindWindow / SetWindowCompositionAttribute, which this build
// deliberately never calls. All glass is LiquidUI's own shader.
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace Cheat {

    // окно роблокса по пиду, берём самое жирное
    HWND Renderer::FindGameWindow()
    {
        DWORD pid = g_Memory.GetPID();
        if (!pid)
        {
            return nullptr;
        }

        struct Ctx
        {
            DWORD pid;
            HWND best = nullptr;
            int best_area = 0;
        } ctx{ pid };

        EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL
        {
            auto* c = (Ctx*)lp;
            DWORD wpid = 0;
            GetWindowThreadProcessId(hwnd, &wpid);
            if (wpid != c->pid)
            {
                return TRUE;
            }

            if (!IsWindowVisible(hwnd) || IsIconic(hwnd))
            {
                return TRUE;
            }

            if (GetWindow(hwnd, GW_OWNER) != nullptr)
            {
                return TRUE;
            }

            RECT cr{};
            if (!GetClientRect(hwnd, &cr))
            {
                return TRUE;
            }

            int w = cr.right - cr.left;
            int h = cr.bottom - cr.top;
            if (w < 200 || h < 200)
            {
                return TRUE;
            }

            wchar_t title[256]{};
            GetWindowTextW(hwnd, title, 256);
            if (title[0] == L'\0')
            {
                return TRUE;
            }

            int area = w * h;
            if (area > c->best_area)
            {
                c->best_area = area;
                c->best = hwnd;
            }
            return TRUE;
        }, (LPARAM)&ctx);

        return ctx.best;
    }

    void Renderer::ResizeSwapchain(int w, int h)
    {
        if (!m_SwapChain || w <= 0 || h <= 0)
        {
            return;
        }

        if (w == m_Width && h == m_Height)
        {
            return;
        }

        CleanupRenderTarget();
        if (FAILED(m_SwapChain->ResizeBuffers(0, (UINT)w, (UINT)h, DXGI_FORMAT_UNKNOWN, 0)))
        {
            return;
        }

        CreateRenderTarget();
        m_Width = w;
        m_Height = h;
        Visuals::MeshDxShader::Resize((unsigned)w, (unsigned)h);
    }

    // клеим оверлей к клиентской области
    void Renderer::SyncToGameWindow()
    {
        DWORD pid = g_Memory.GetPID();
        DWORD now = GetTickCount();

        // раз в 300мс пересканируем, иначе держим кэш
        bool need_enum =
            !m_GameHwnd ||
            !IsWindow(m_GameHwnd) ||
            pid != m_CachedEnumPid ||
            (now - m_LastEnumTick) >= 300;

        if (need_enum)
        {
            m_GameHwnd = FindGameWindow();
            m_LastEnumTick = now;
            m_CachedEnumPid = pid;
        }

        m_GameActive = false;

        if (!m_GameHwnd || !IsWindow(m_GameHwnd) || IsIconic(m_GameHwnd))
        {
            if (IsWindowVisible(m_Hwnd))
            {
                ShowWindow(m_Hwnd, SW_HIDE);
            }
            return;
        }

        HWND fg = GetForegroundWindow();
        // меню открыто ок тока если фокус на игре или на оверлее
        // иначе Win+Shift+S / чужое окно — гуй в скрине
        bool focused =
            (fg == m_GameHwnd) ||
            IsChild(m_GameHwnd, fg) ||
            (fg == m_Hwnd);

        if (!focused)
        {
            if (IsWindowVisible(m_Hwnd))
            {
                ShowWindow(m_Hwnd, SW_HIDE);
            }
            return;
        }

        RECT cr{};
        if (!GetClientRect(m_GameHwnd, &cr))
        {
            if (IsWindowVisible(m_Hwnd))
            {
                ShowWindow(m_Hwnd, SW_HIDE);
            }
            return;
        }

        POINT tl = { cr.left, cr.top };
        POINT br = { cr.right, cr.bottom };
        ClientToScreen(m_GameHwnd, &tl);
        ClientToScreen(m_GameHwnd, &br);

        int w = br.x - tl.x;
        int h = br.y - tl.y;
        if (w < 64 || h < 64)
        {
            if (IsWindowVisible(m_Hwnd))
            {
                ShowWindow(m_Hwnd, SW_HIDE);
            }
            return;
        }

        ResizeSwapchain(w, h);

        // Only do the (expensive) move/size sync when the game window actually
        // moved or resized, or when the overlay is currently hidden (tab-out).
        // A cheap TOPMOST/visibility re-assert is issued at most once per
        // second: DWM can occasionally reorder topmost windows, so without any
        // re-assert the overlay could end up behind the game window - but
        // re-asserting every frame throttled the loop to DWM's composition
        // rate (monitor refresh), which no vsync/cap setting could override.
        static RECT s_last{ -1, -1, -1, -1 };
        // SetWindowPos synchronizes with DWM's compositor, so calling it every
        // frame throttles the render loop to the monitor's refresh rate no
        // matter what vsync/cap settings say. Only issue it when the game rect
        // actually changed, the overlay is hidden, or on a low-frequency
        // re-assert (DWM can reorder topmost windows vs the glass backdrop).
        static DWORD s_lastAssertTick = 0;
        const bool rectChanged =
            s_last.left != tl.x || s_last.top != tl.y ||
            s_last.right != br.x || s_last.bottom != br.y;
        const bool hidden = !IsWindowVisible(m_Hwnd);
        const bool periodic = (now - s_lastAssertTick) >= 1000;

        if (rectChanged || hidden || periodic)
        {
            s_lastAssertTick = now;
            if (rectChanged)
                s_last = { tl.x, tl.y, br.x, br.y };
            SetWindowPos(
                m_Hwnd, HWND_TOPMOST,
                rectChanged ? tl.x : 0, rectChanged ? tl.y : 0,
                rectChanged ? w : 0, rectChanged ? h : 0,
                SWP_NOACTIVATE | (rectChanged ? SWP_SHOWWINDOW : SWP_NOMOVE | SWP_NOSIZE) |
                (hidden ? SWP_SHOWWINDOW : 0));
        }

        m_GameActive = true;
    }

    bool Renderer::Initialize(HINSTANCE instance)
    {
        WNDCLASSEXW wc = {
            sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, instance,
            nullptr, nullptr, nullptr, nullptr, L"jewsploit.overlay", nullptr
        };
        if (!RegisterClassExW(&wc))
        {
            Console::Log(Console::Color::Red, "overlay fail window class");
            return false;
        }

        m_Hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED |
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName,
            L"",
            WS_POPUP,
            0, 0, 2, 2,
            nullptr, nullptr, wc.hInstance, nullptr);
        if (!m_Hwnd)
        {
            Console::Log(Console::Color::Red, "overlay fail window");
            return false;
        }

        // Layered window: overall opacity 255, the per-pixel alpha channel of
        // the B8G8R8A8 swapchain does the real work. There is deliberately NO
        // OS blur and NO DWM frame here - every frosted panel is drawn by the
        // LiquidUI glass renderer from its own DXGI desktop capture, so the
        // overlay needs nothing from outside its own swapchain.
        SetLayeredWindowAttributes(m_Hwnd, 0, 255, LWA_ALPHA);

        // Per-pixel transparency plumbing - NOT acrylic and NOT blur:
        // a DISCARD-model DXGI swapchain only gets its alpha channel honoured
        // by DWM when the frame is extended into the whole client area. Without
        // this call the transparent pixels composite as solid black. Nothing
        // here asks Windows for a blur, tint or glass effect - the frosted look
        // is drawn entirely by the LiquidUI shader from its own desktop capture.
        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(m_Hwnd, &margins);

        if (!CreateDevice())
        {
            Console::Log(Console::Color::Red, "overlay fail dx11");
            CleanupDevice();
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        ShowWindow(m_Hwnd, SW_HIDE);

        Cheat::Core::g_Device = m_Device;
        Cheat::Core::g_DeviceContext = m_DeviceContext;
        Cheat::Core::g_SwapChain = m_SwapChain;
        Cheat::Core::g_RenderTargetView = m_RenderTargetView;

        Cheat::Visuals::BoxFill::Init(m_Device);
        Visuals::MeshDxShader::Init(m_Device, m_DeviceContext);

        if (!GUI::Menu::Initialize(m_Hwnd, m_Device, m_DeviceContext))
        {
            Console::Log(Console::Color::Red, "overlay fail menu");
            CleanupDevice();
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        return true;
    }

    void Renderer::MainLoop()
    {
        bool running = true;
        // QPC-based cap: target overlay frame interval in microseconds.
        // 0 = uncapped (present as fast as the loop + swap chain allow).
        long long target_us = 0;
        LARGE_INTEGER t_freq{};
        QueryPerformanceFrequency(&t_freq);
        LARGE_INTEGER t_next{};
        QueryPerformanceCounter(&t_next);

        while (running)
        {
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT)
                {
                    running = false;
                }
            }
            if (!running)
            {
                break;
            }

            SyncToGameWindow();

            if (!m_GameActive)
            {
                // nothing to hide anymore: the frosted panels only exist while
                // the ui renders (they live in the overlay's swap chain now)
                glass::set_menu_rect(0, 0, 0, 0);
                Visuals::Crosshair::NotifyInactive();
                Sleep(15);
                continue;
            }

            // enforce the user-requested cap (0 = off / uncapped). vsync is handled
            // by Present's sync interval below, so only do software pacing when we
            // are NOT syncing to the monitor.
            if (g_Settings.gui.vsync)
            {
                target_us = 0;
            }
            else
            {
                const int cap = g_Settings.gui.fps_cap > 0 ? g_Settings.gui.fps_cap : 0;
                const long long want = cap > 0 ? (1000000LL / (long long)cap) : 0;
                if (want > 0)
                {
                    for (;;)
                    {
                        LARGE_INTEGER now{};
                        QueryPerformanceCounter(&now);
                        const long long elapsed = (long long)((double)(now.QuadPart - t_next.QuadPart) * 1000000.0 / (double)t_freq.QuadPart);
                        if (elapsed < 0)
                        {
                            // not yet — sleep the remainder in small slices so we
                            // still drain WM_QUIT promptly
                            long long left_us = -elapsed;
                            const long long slice = left_us > 2000 ? 1000 : left_us;
                            Sleep((DWORD)((slice + 999) / 1000));
                            continue;
                        }
                        target_us = want;
                        break;
                    }
                }
                else
                {
                    target_us = 0;
                    LARGE_INTEGER now{};
                    QueryPerformanceCounter(&now);
                    t_next = now;
                }
            }

            float clear[4] = { 0.f, 0.f, 0.f, 0.f };
            m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);
            m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, clear);

            GUI::Menu::Render();

            // клик-тру / фокус под imgui
            {
                POINT pt{};
                GetCursorPos(&pt);
                ScreenToClient(m_Hwnd, &pt);
                bool over_ui = GUI::Menu::ShouldCaptureMouse((float)pt.x, (float)pt.y);

                if (GUI::Menu::IsVisible() || over_ui)
                {
                    SetClickThrough(!over_ui);
                    if (GUI::Menu::IsVisible())
                    {
                        ClipCursor(nullptr);
                    }

                    bool want_text = GUI::Menu::IsVisible() &&
                        ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
                    SetTextInputFocus(want_text);
                }

                else
                {
                    SetClickThrough(true);
                    SetTextInputFocus(false);
                }
            }

            // vsync: on = present syncs to the monitor's refresh, off = uncapped
            // (present as fast as the loop can, less lag relative to the game camera)
            const HRESULT pres = m_SwapChain->Present(g_Settings.gui.vsync ? 1 : 0, 0);
            if (FAILED(pres))
            {
                static bool s_logged_present = false;
                if (!s_logged_present)
                {
                    s_logged_present = true;
                    Console::Log(Console::Color::Red,
                        "Present failed: 0x%08X", (unsigned)pres);
                }
            }
            else
            {
                HRESULT rm = m_Device->GetDeviceRemovedReason();
                if (rm != S_OK)
                {
                    static bool s_logged_rm = false;
                    if (!s_logged_rm)
                    {
                        s_logged_rm = true;
                        Console::Log(Console::Color::Red,
                            "DEVICE REMOVED: 0x%08X", (unsigned)rm);
                    }
                }
                static int s_present_tick = 0;
                if ((++s_present_tick % 600) == 0)
                    Console::Log(Console::Color::Gray,
                        "[probe] frames flowing: %d", s_present_tick);
            }

            // advance the software cap's next-frame target; snap to now when a frame
            // overran its budget so lag doesn't accumulate into burst-then-idle
            if (target_us > 0)
            {
                LARGE_INTEGER now{};
                QueryPerformanceCounter(&now);
                long long next = t_next.QuadPart
                    + (long long)((double)target_us * (double)t_freq.QuadPart / 1000000.0);
                if (next < now.QuadPart)
                    next = now.QuadPart;
                t_next.QuadPart = next;
            }
        }

        Visuals::Crosshair::Shutdown();
    }

    void Renderer::SetClickThrough(bool click_through)
    {
        if (!m_Hwnd || !IsWindow(m_Hwnd))
        {
            return;
        }

        bool want_text = ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
        LONG want;
        if (click_through)
        {
            want = (LONG)(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED |
                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
        }

        else
        {
            want = (LONG)(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
        }

        // без NOACTIVATE текст в imgui не фокусится
        if (want_text && !click_through)
        {
            want &= ~WS_EX_NOACTIVATE;
        }

        LONG cur = GetWindowLong(m_Hwnd, GWL_EXSTYLE);
        if (cur == want)
        {
            return;
        }

        SetWindowLong(m_Hwnd, GWL_EXSTYLE, want);
        SetWindowPos(
            m_Hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
            SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }

    void Renderer::SetTextInputFocus(bool want_text)
    {
        if (!m_Hwnd || !IsWindow(m_Hwnd))
        {
            return;
        }

        static bool s_had_text = false;

        LONG style = GetWindowLong(m_Hwnd, GWL_EXSTYLE);

        if (want_text)
        {
            style &= ~(WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);
            SetWindowLong(m_Hwnd, GWL_EXSTYLE, style);
            SetWindowPos(
                m_Hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            if (GetFocus() != m_Hwnd)
            {
                SetFocus(m_Hwnd);
            }
            s_had_text = true;
        }

        else if (s_had_text)
        {
            style |= WS_EX_NOACTIVATE;
            SetWindowLong(m_Hwnd, GWL_EXSTYLE, style);
            SetWindowPos(
                m_Hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                SWP_FRAMECHANGED | SWP_NOACTIVATE);
            if (m_GameHwnd && IsWindow(m_GameHwnd))
            {
                SetForegroundWindow(m_GameHwnd);
            }
            s_had_text = false;
        }
    }

    void Renderer::Shutdown()
    {
        GUI::Menu::Shutdown();
        Cheat::Visuals::BoxFill::Shutdown();
        Visuals::MeshDxShader::Shutdown();

        CleanupDevice();
        Cheat::Core::g_Device = nullptr;
        Cheat::Core::g_DeviceContext = nullptr;
        Cheat::Core::g_SwapChain = nullptr;
        Cheat::Core::g_RenderTargetView = nullptr;

        if (m_Hwnd)
        {
            DestroyWindow(m_Hwnd);
            m_Hwnd = nullptr;
        }
    }

    bool Renderer::CreateDevice()
    {
        // Layered windows (WS_EX_LAYERED) are incompatible with the flip-model
        // swap chain, so use the classic D3D11CreateDeviceAndSwapChain DISCARD
        // path like before. B8G8R8A8 gives us an alpha channel: the per-pixel
        // transparency of the layered window is what lets the game show through,
        // and the LiquidUI glass panels paint their own blur on top of it.
        // Triple-buffered: with 2 buffers a windowed swap chain's Present blocks
        // waiting for a free buffer (throttling the whole loop to the compositor
        // cadence ~60-72Hz), so a 3rd buffer keeps Present returning immediately.
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 3;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 0;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = m_Hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT flags = 0;
        D3D_FEATURE_LEVEL level;
        D3D_FEATURE_LEVEL levels[2] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0,
        };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            levels, 2, D3D11_SDK_VERSION, &sd,
            &m_SwapChain, &m_Device, &level, &m_DeviceContext);
        if (hr != S_OK)
        {
            return false;
        }

        CreateRenderTarget();

        DXGI_SWAP_CHAIN_DESC cur{};
        if (SUCCEEDED(m_SwapChain->GetDesc(&cur)))
        {
            m_Width = (int)cur.BufferDesc.Width;
            m_Height = (int)cur.BufferDesc.Height;
        }
        return true;
    }

    void Renderer::CreateRenderTarget()
    {
        ID3D11Texture2D* back = nullptr;
        m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
        if (!back)
        {
            return;
        }

        m_Device->CreateRenderTargetView(back, nullptr, &m_RenderTargetView);
        back->Release();

        Cheat::Core::g_RenderTargetView = m_RenderTargetView;
        Cheat::Core::g_SwapChain = m_SwapChain;
    }

    void Renderer::CleanupDevice()
    {
        CleanupRenderTarget();

        if (m_SwapChain)
        {
            m_SwapChain->Release();
            m_SwapChain = nullptr;
        }

        if (m_DeviceContext)
        {
            m_DeviceContext->Release();
            m_DeviceContext = nullptr;
        }

        if (m_Device)
        {
            m_Device->Release();
            m_Device = nullptr;
        }
    }

    void Renderer::CleanupRenderTarget()
    {
        if (m_RenderTargetView)
        {
            m_RenderTargetView->Release();
            m_RenderTargetView = nullptr;
        }
        Cheat::Core::g_RenderTargetView = nullptr;
    }

    LRESULT WINAPI Renderer::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        if (msg == WM_NCHITTEST)
        {
            if (GUI::Menu::IsVisible())
            {
                return HTCLIENT;
            }
            return HTTRANSPARENT;
        }

        if (GUI::Menu::HandleMessage(hwnd, msg, wparam, lparam))
        {
            return true;
        }

        switch (msg)
        {
        case WM_SIZE:
            if (m_Device != nullptr && wparam != SIZE_MINIMIZED)
            {
                int w = (int)LOWORD(lparam);
                int h = (int)HIWORD(lparam);
                if (w > 0 && h > 0)
                {
                    ResizeSwapchain(w, h);
                }
            }
            return 0;

        case WM_MOUSEACTIVATE:
            if (GUI::Menu::IsVisible() && ImGui::GetCurrentContext() &&
                ImGui::GetIO().WantTextInput)
            {
                return MA_ACTIVATE;
            }
            return MA_NOACTIVATE;

        case WM_SYSCOMMAND:
            if ((wparam & 0xfff0) == SC_KEYMENU)
            {
                return 0;
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

}
