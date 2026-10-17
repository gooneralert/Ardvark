#include "pch.h"
#include "menu_window.h"

#include "gui/gui.h"
#include "gui/glass.h"
#include "matcha-ui/src/gui.h"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

// the vendored win32 backend keeps the handler's declaration #if 0'd (the
// overlay declares it too) - declare it here for our own window
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include <cstring>
#include <string>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace menu_window
{
namespace
{
    HWND                    s_hwnd   = nullptr;
    ID3D11Device*           s_device = nullptr;
    ID3D11DeviceContext*    s_ctx    = nullptr;
    IDXGISwapChain*         s_swap   = nullptr;
    ID3D11RenderTargetView* s_rtv    = nullptr;
    ImGuiContext*           s_imgui  = nullptr;
    bool                    s_ready  = false;
    bool                    s_shown  = false;
    int                     s_w      = 0;
    int                     s_h      = 0;

    void create_rtv()
    {
        ID3D11Texture2D* back = nullptr;
        if (SUCCEEDED(s_swap->GetBuffer(0, IID_PPV_ARGS(&back))) && back)
        {
            s_device->CreateRenderTargetView(back, nullptr, &s_rtv);
            back->Release();
        }
    }

    bool create_device()
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount        = 1;
        desc.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow       = s_hwnd;
        desc.SampleDesc.Count   = 1;
        desc.Windowed           = TRUE;
        desc.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_DRIVER_TYPE drivers[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP };
        for (D3D_DRIVER_TYPE drv : drivers)
        {
            if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(
                    nullptr, drv, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                    &desc, &s_swap, &s_device, nullptr, &s_ctx)))
            {
                create_rtv();
                return true;
            }
        }
        return false;
    }

    // matcha's blur: extend the DWM frame over the whole client area so the
    // swapchain's per-pixel alpha composites, then ask DWM to blur whatever is
    // behind the window
    void enable_blur(HWND hwnd)
    {
        struct accent_policy { int state; int flags; COLORREF color; int animation; };
        struct composition_data { int attribute; void* data; SIZE_T size; };

        constexpr int blur_behind      = 3;
        constexpr int accent_attribute = 19;

        using set_composition_fn = BOOL(WINAPI*)(HWND, const composition_data*);
        auto set_composition = reinterpret_cast<set_composition_fn>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
        if (!set_composition)
            return;

        MARGINS margins{ -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        accent_policy accent{ blur_behind, 0, 0, 0 };
        composition_data data{ accent_attribute, &accent, sizeof(accent) };
        set_composition(hwnd, &data);
    }

    void center_on_cursor_monitor()
    {
        POINT cur{};
        GetCursorPos(&cur);
        HMONITOR mon = MonitorFromPoint(cur, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(mi) };
        if (mon && GetMonitorInfoW(mon, &mi))
        {
            const RECT& w = mi.rcWork;
            int x = w.left + ((w.right - w.left) - s_w) / 2;
            int y = w.top + ((w.bottom - w.top) - s_h) / 2;
            SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, s_w, s_h, SWP_NOACTIVATE);
        }
    }

    // We're summoned by a hotkey while the game owns the foreground, so a bare
    // SetForegroundWindow() is usually denied. Attaching our input queue to the
    // current foreground thread for the call makes Windows accept it, which is
    // what the win32 backend needs to feed the mouse into ImGui.
    void force_foreground(HWND hwnd)
    {
        HWND  fg      = GetForegroundWindow();
        DWORD fg_tid  = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
        DWORD our_tid = GetCurrentThreadId();

        const bool attach = fg_tid != 0 && fg_tid != our_tid;
        if (attach)
            AttachThreadInput(fg_tid, our_tid, TRUE);

        ShowWindow(hwnd, SW_SHOW);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);

        if (attach)
            AttachThreadInput(fg_tid, our_tid, FALSE);
    }

    // matcha's frameless-window drag: grab anywhere that isn't an ImGui widget
    // and isn't over an open popup, then move the HWND with the cursor.
    void drag_window()
    {
        static bool  dragging = false;
        static POINT grab{};

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !ImGui::IsAnyItemHovered()
            && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
        {
            RECT rc{};
            GetWindowRect(s_hwnd, &rc);
            GetCursorPos(&grab);
            grab.x -= rc.left;
            grab.y -= rc.top;
            dragging = true;
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            dragging = false;

        if (dragging)
        {
            POINT cur{};
            GetCursorPos(&cur);
            SetWindowPos(s_hwnd, nullptr, cur.x - grab.x, cur.y - grab.y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
    }

    LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        // the win32 backend routes through the *current* context, and the
        // overlay has its own - make ours current for the duration
        ImGuiContext* prev = ImGui::GetCurrentContext();
        if (s_imgui) ImGui::SetCurrentContext(s_imgui);
        const bool handled = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam) != 0;
        if (s_imgui) ImGui::SetCurrentContext(prev);
        if (handled)
            return 1;

        switch (msg)
        {
        case WM_SIZE:
            if (s_swap && wparam != SIZE_MINIMIZED)
            {
                if (s_rtv) { s_rtv->Release(); s_rtv = nullptr; }
                s_swap->ResizeBuffers(0, LOWORD(lparam), HIWORD(lparam), DXGI_FORMAT_UNKNOWN, 0);
                create_rtv();
            }
            return 0;

        case WM_SYSCOMMAND:
            if ((wparam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            s_shown = false;
            return 0;

        case WM_MOUSEACTIVATE:
            return MA_ACTIVATE;   // clicking the menu must give it keyboard focus

        case WM_DESTROY:
            return 0;   // never PostQuitMessage: the overlay owns the loop
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}   // namespace

bool init(HINSTANCE instance)
{
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ardvark.menu";
    if (!RegisterClassExW(&wc))
        return false;

    const ImVec2 sz = mmenu::size();
    s_w = (int)sz.x;
    s_h = (int)sz.y;

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int x = work.left + ((work.right - work.left) - s_w) / 2;
    const int y = work.top + ((work.bottom - work.top) - s_h) / 2;

    s_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, L"Ardvark", WS_POPUP,
        x, y, s_w, s_h, nullptr, nullptr, instance, nullptr);
    if (!s_hwnd)
        return false;

    DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(s_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    COLORREF border = RGB(57, 57, 63);
    DwmSetWindowAttribute(s_hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
    enable_blur(s_hwnd);

    if (!create_device())
        return false;

    // the overlay's context must stay current for the overlay - create ours,
    // configure it, then hand control back
    ImGuiContext* prev = ImGui::GetCurrentContext();
    s_imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(s_imgui);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigWindowsResizeFromEdges = false;
    mmenu::init();   // matcha's font (Inter Display + lucide icons) and style

    ImGui_ImplWin32_Init(s_hwnd);
    ImGui_ImplDX11_Init(s_device, s_ctx);

    ImGui::SetCurrentContext(prev);

    s_ready = true;
    s_shown = false;
    return true;
}

void shutdown()
{
    if (!s_ready)
        return;

    ImGuiContext* prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(s_imgui);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(s_imgui);
    ImGui::SetCurrentContext(prev);
    s_imgui = nullptr;

    if (s_rtv)    { s_rtv->Release();    s_rtv = nullptr; }
    if (s_swap)   { s_swap->Release();   s_swap = nullptr; }
    if (s_ctx)    { s_ctx->Release();    s_ctx = nullptr; }
    if (s_device) { s_device->Release(); s_device = nullptr; }

    if (s_hwnd)   { DestroyWindow(s_hwnd); s_hwnd = nullptr; }
    UnregisterClassW(L"ardvark.menu", GetModuleHandleW(nullptr));

    s_ready = false;
    s_shown = false;
}

void hide()
{
    if (s_hwnd && s_shown)
    {
        ShowWindow(s_hwnd, SW_HIDE);
        s_shown = false;
    }
}

bool visible() { return s_ready && s_shown; }
HWND hwnd()    { return s_hwnd; }

bool owns_foreground()
{
    if (!s_hwnd) return false;
    HWND fg = GetForegroundWindow();
    return fg == s_hwnd || (fg && IsChild(s_hwnd, fg));
}

void tick()
{
    if (!s_ready)
        return;

    const bool want = gui::menu_visible();
    if (want != s_shown)
    {
        s_shown = want;
        if (want)
        {
            center_on_cursor_monitor();
            force_foreground(s_hwnd);
        }
        else
        {
            ShowWindow(s_hwnd, SW_HIDE);
        }
    }

    if (!want)
        return;

    ImGuiContext* prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(s_imgui);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // the menu has its own real DWM blur; suppress the overlay's LiquidUI glass
    // so any widget drawn here (combo dropdowns call glass::draw) can't queue a
    // panel into the overlay's pass, which would paint it in the wrong place
    glass::set_suppressed(true);
    mmenu::render();
    glass::set_suppressed(false);

    drag_window();

    ImGui::Render();

    const float clear[4] = { 0.f, 0.f, 0.f, 0.f };
    s_ctx->OMSetRenderTargets(1, &s_rtv, nullptr);
    s_ctx->ClearRenderTargetView(s_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    s_swap->Present(1, 0);

    ImGui::SetCurrentContext(prev);
}
}
